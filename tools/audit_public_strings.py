"""Audit the files and Git history reachable from a ref before publishing it.

Exact private strings belong in .git/public-string-denylist.txt, which is local
to this checkout and is never committed. The audit reads every unique Git blob
reachable from the selected ref, commit metadata, tracked and untracked
non-ignored worktree files, and reports matches without printing the values.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DENYLIST = ROOT / ".git" / "public-string-denylist.txt"
SAFE_EMAILS = {"noreply@users.noreply.github.com"}
PATTERNS = [
    ("email address", re.compile(r"(?i)(?<![\w.+-])[\w.+-]+@[\w.-]+\.[a-z]{2,}(?![\w.-])")),
    ("Windows user profile path", re.compile(r"(?i)[a-z]:\\users\\[^\\\s\"'<>]+")),
    ("absolute Windows path", re.compile(r"(?i)(?<![a-z])[a-z]" + re.escape(":") + r"[\\/][^\s\"'<>]+")),
    ("GitHub token", re.compile(r"\b(?:gh[pousr]_[A-Za-z0-9_]{20,}|github_pat_[A-Za-z0-9_]{20,})\b")),
    ("AWS access key", re.compile(r"\bAKIA[0-9A-Z]{16}\b")),
    ("private key marker", re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----")),
    (
        "literal credential assignment",
        re.compile(
            r"(?i)\b(?:password|passwd|passphrase|secret|api[_ -]?key|access[_ -]?token)"
            r"\b\s*[:=]\s*['\"](?!YOUR_[A-Z0-9_]+['\"])[^'\"\r\n]{8,}['\"]"
        ),
    ),
]


def git(*args: str, check: bool = True) -> bytes:
    result = subprocess.run(
        ["git", *args], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    if check and result.returncode:
        raise RuntimeError(result.stderr.decode("utf-8", "replace").strip())
    return result.stdout


def load_denylist(path: Path) -> list[str]:
    if not path.is_file():
        return []
    return [line.strip() for line in path.read_text(encoding="utf-8-sig").splitlines() if line.strip()]


def findings(data: bytes, denylist: list[str]) -> set[str]:
    found: set[str] = set()
    for value in denylist:
        if value.encode("utf-8") in data:
            found.add("private exact string")
    try:
        text = data.decode("utf-8")
        is_utf8_text = True
    except UnicodeDecodeError:
        text = data.decode("utf-8", "ignore")
        is_utf8_text = False
    for label, pattern in PATTERNS:
        if label == "absolute Windows path" and not is_utf8_text:
            continue
        for match in pattern.finditer(text):
            value = match.group(0)
            if label == "email address" and (
                value.casefold() in SAFE_EMAILS
                or value.casefold().endswith("@users.noreply.github.com")
            ):
                continue
            found.add(label)
    return found


def scan_worktree(denylist: list[str], result: set[tuple[str, str]]) -> None:
    paths = git("ls-files", "-co", "--exclude-standard", "-z").decode("utf-8", "surrogateescape")
    for item in paths.split("\0"):
        if not item:
            continue
        path = ROOT / item
        try:
            data = path.read_bytes()
        except (OSError, IsADirectoryError):
            continue
        for label in findings(data, denylist):
            result.add((f"worktree:{item}", label))


def scan_history(ref: str, denylist: list[str], result: set[tuple[str, str]]) -> tuple[int, int]:
    commits = git("rev-list", ref).decode("ascii").splitlines()
    blobs_seen: set[str] = set()
    for commit in commits:
        metadata = git("show", "-s", "--format=%an%n%ae%n%cn%n%ce%n%B", commit)
        for label in findings(metadata, denylist):
            result.add((f"commit:{commit[:12]}", label))
        tree = git("ls-tree", "-r", "-z", "--full-tree", commit)
        for entry in tree.split(b"\0"):
            if not entry:
                continue
            header, raw_path = entry.split(b"\t", 1)
            mode, kind, sha = header.decode("ascii").split()
            path = raw_path.decode("utf-8", "replace")
            for label in findings(raw_path, denylist):
                result.add((f"history-path:{commit[:12]}:{path}", label))
            if kind != "blob" or sha in blobs_seen:
                continue
            blobs_seen.add(sha)
            content = git("cat-file", "blob", sha)
            for label in findings(content, denylist):
                result.add((f"blob:{commit[:12]}:{path}", label))
    return len(commits), len(blobs_seen)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ref", default="main", help="Git ref to audit (default: main)")
    parser.add_argument(
        "--denylist",
        type=Path,
        default=DEFAULT_DENYLIST,
        help="UTF-8 file with one private string per line (default: .git/public-string-denylist.txt)",
    )
    args = parser.parse_args()
    try:
        git("rev-parse", "--verify", args.ref)
        denylist = load_denylist(args.denylist)
        result: set[tuple[str, str]] = set()
        scan_worktree(denylist, result)
        commit_count, blob_count = scan_history(args.ref, denylist, result)
    except (RuntimeError, OSError) as error:
        print(f"AUDIT ERROR: {error}", file=sys.stderr)
        return 2

    if result:
        print("FAIL: possible private strings found; matched values are hidden.")
        for location, label in sorted(result):
            print(f"{label}: {location}")
        return 1
    print(
        "PASS: no configured private strings, detected email addresses, "
        "user-profile paths, credential tokens, or private-key markers found."
    )
    print(
        f"Scanned worktree files, {commit_count} commits, and {blob_count} unique Git blobs reachable from {args.ref}."
    )
    if not denylist:
        print("NOTE: no private exact-string denylist was found; heuristic checks still ran.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
