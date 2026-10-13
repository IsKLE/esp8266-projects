"""USB provisioning and diagnostics. Credentials never go to stdout or argv.

Host credentials are stored with Windows DPAPI for the current user. The board
stores its configuration in EEPROM-emulated flash. No network management server
or port mappings are installed on the board.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import json
import os
from pathlib import Path
import secrets
import struct
import sys
import time
import xml.etree.ElementTree as ET
import zlib

ROOT = Path(__file__).resolve().parents[1]
PROFILE = ROOT / 'NatRelay' / 'credentials.dpapi'
UPSTREAM_SSID = 'YOUR_WIFI_SSID'
UPSTREAM_PROFILE = 'YOUR_SAVED_WIFI_PROFILE'
if os.environ.get('ESP8266_SERIAL_LIBRARY'):
    sys.path.insert(0, os.environ['ESP8266_SERIAL_LIBRARY'])
import serial

class GUID(C.Structure):
    _fields_ = [('bytes', C.c_ubyte * 16)]
class Interface(C.Structure):
    _fields_ = [('guid', GUID), ('description', W.WCHAR * 256), ('state', W.DWORD)]
class Blob(C.Structure):
    _fields_ = [('size', W.DWORD), ('data', C.POINTER(C.c_ubyte))]

def dpapi(data, decrypt=False):
    raw = (C.c_ubyte * len(data)).from_buffer_copy(data)
    src, dst = Blob(len(data), raw), Blob()
    dll = C.WinDLL('crypt32', use_last_error=True)
    fn = dll.CryptUnprotectData if decrypt else dll.CryptProtectData
    fn.argtypes = [C.POINTER(Blob), W.LPCWSTR, C.c_void_p, C.c_void_p, C.c_void_p, W.DWORD, C.POINTER(Blob)]
    if not fn(C.byref(src), None, None, None, None, 1, C.byref(dst)):
        raise RuntimeError('Windows credential protection failed')
    try:
        return C.string_at(dst.data, dst.size)
    finally:
        C.windll.kernel32.LocalFree(C.cast(dst.data, C.c_void_p))

class Wlan:
    def __init__(self):
        self.dll = C.WinDLL('wlanapi')
        self.handle = W.HANDLE()
        version = W.DWORD()
        self.check(self.dll.WlanOpenHandle(2, None, C.byref(version), C.byref(self.handle)))
        pointer = C.c_void_p()
        self.check(self.dll.WlanEnumInterfaces(self.handle, None, C.byref(pointer)))
        try:
            count = W.DWORD.from_address(pointer.value).value
            if not count: raise RuntimeError('No Wi-Fi adapter')
            self.interface = Interface.from_buffer_copy(C.string_at(pointer.value + 8, C.sizeof(Interface)))
        finally:
            self.dll.WlanFreeMemory(pointer)
    @staticmethod
    def check(code):
        if code: raise RuntimeError('Windows Wi-Fi API failed: {}'.format(code))
    def close(self):
        self.dll.WlanCloseHandle(self.handle, None)
    def password(self, profile):
        xml = C.c_void_p()
        flags, access = W.DWORD(4), W.DWORD() # WLAN_PROFILE_GET_PLAINTEXT_KEY
        self.dll.WlanGetProfile.argtypes = [W.HANDLE, C.POINTER(GUID), W.LPCWSTR, C.c_void_p, C.POINTER(C.c_void_p), C.POINTER(W.DWORD), C.POINTER(W.DWORD)]
        self.check(self.dll.WlanGetProfile(self.handle, C.byref(self.interface.guid), profile, None, C.byref(xml), C.byref(flags), C.byref(access)))
        try:
            tree = ET.fromstring(C.wstring_at(xml))
            ns = {'w': 'http://www.microsoft.com/networking/WLAN/profile/v1'}
            key = tree.find('.//w:sharedKey/w:keyMaterial', ns)
            protected = tree.find('.//w:sharedKey/w:protected', ns)
            if key is None or protected is None or protected.text != 'false':
                raise RuntimeError('Saved Wi-Fi key could not be read; run provisioning as administrator')
            return key.text
        finally:
            self.dll.WlanFreeMemory(xml)
    def radio(self, enabled):
        state = (W.DWORD * 3)(0, 1 if enabled else 2, 1)
        self.check(self.dll.WlanSetInterface(self.handle, C.byref(self.interface.guid), 4, C.sizeof(state), C.byref(state), None))
    def connect(self, name, ssid, password):
        # User-only temporary profile. XML stays in memory.
        from xml.sax.saxutils import escape
        xml = '''<?xml version="1.0"?><WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1"><name>{name}</name><SSIDConfig><SSID><hex>{hex}</hex><name>{ssid}</name></SSID></SSIDConfig><connectionType>ESS</connectionType><connectionMode>manual</connectionMode><MSM><security><authEncryption><authentication>WPA2PSK</authentication><encryption>AES</encryption><useOneX>false</useOneX></authEncryption><sharedKey><keyType>passPhrase</keyType><protected>false</protected><keyMaterial>{password}</keyMaterial></sharedKey></security></MSM></WLANProfile>'''.format(name=escape(name), hex=ssid.encode().hex(), ssid=escape(ssid), password=escape(password))
        reason = W.DWORD()
        self.dll.WlanSetProfile.argtypes = [W.HANDLE,C.POINTER(GUID),W.DWORD,W.LPCWSTR,W.LPCWSTR,W.BOOL,C.c_void_p,C.POINTER(W.DWORD)]
        self.check(self.dll.WlanSetProfile(self.handle,C.byref(self.interface.guid),2,xml,None,True,None,C.byref(reason)))
        class Params(C.Structure):
            _fields_ = [('mode',C.c_int),('profile',W.LPCWSTR),('ssid',C.c_void_p),('bssid',C.c_void_p),('bss',C.c_int),('flags',W.DWORD)]
        params = Params(0,name,None,None,1,0)
        self.check(self.dll.WlanConnect(self.handle,C.byref(self.interface.guid),C.byref(params),None))
    def cleanup(self):
        self.dll.WlanDisconnect(self.handle,C.byref(self.interface.guid),None)
        self.dll.WlanDeleteProfile.argtypes = [W.HANDLE,C.POINTER(GUID),W.LPCWSTR,C.c_void_p]
        for name in ['ESP8266-NAT-test', 'ESP8266-upstream-test']:
            self.dll.WlanDeleteProfile(self.handle,C.byref(self.interface.guid),name,None)

def load_config():
    return json.loads(dpapi(PROFILE.read_bytes(), True).decode())
def save_config():
    if PROFILE.exists(): return load_config()
    wlan = Wlan()
    try:
        password = wlan.password(UPSTREAM_PROFILE)
    finally: wlan.close()
    cfg = {'ssid':UPSTREAM_SSID,'password':password,'ap':'ESP8266-NAT','apPassword':secrets.token_hex(8)}
    PROFILE.parent.mkdir(parents=True,exist_ok=True)
    PROFILE.write_bytes(dpapi(json.dumps(cfg).encode()))
    return cfg
def config_packet(cfg):
    fields = []
    for name,length in [('ssid',33),('password',65),('ap',33),('apPassword',65)]:
        field = cfg[name].encode('utf-8')
        if not 0 < len(field) < length or b'\0' in field: raise RuntimeError('Invalid credential length')
        fields.append(field.ljust(length,b'\0'))
    body = struct.pack('<I',0x3154414e) + b''.join(fields)
    return body + struct.pack('<I',zlib.crc32(body) & 0xffffffff)
def read_board(command, seconds, port='COM3'):
    connection = serial.Serial(baudrate=115200,timeout=.3)
    connection.dtr = False; connection.rts = False; connection.port = port
    records = []
    try:
        connection.open(); connection.reset_input_buffer()
        connection.write(command + b'\n'); connection.flush()
        end = time.monotonic() + seconds
        retries = 0
        while time.monotonic() < end:
            try:
                raw = connection.readline()
            except serial.SerialException:
                if (command.startswith(b'CONFIG ') and 'OK CONFIG SAVED' in records) or (command == b'REBOOT' and 'OK REBOOT' in records):
                    print('OK BOARD RESTARTING',flush=True)
                    break
                if retries >= 3: raise
                retries += 1
                connection.close()
                time.sleep(.3)
                connection.open()
                if command in [b'STATUS',b'TEST',b'TESTUI']:
                    connection.write(command+b'\n');connection.flush()
                print('NAT1 USB READ RETRY',flush=True)
                continue
            if not raw: continue
            text = raw.decode('utf-8',errors='replace').strip()
            # Strict allowlist: never echo commands or unknown firmware data.
            if text.startswith(('NAT1 ', 'TEST1 ', 'TESTUI ', 'OK ', 'ERR ')):
                print(text,flush=True)
                records.append(text)
    finally: connection.close()
    return records

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('action',choices=['provision','status','test','test-ui','reboot','radio-on','radio-off','connect-ap','connect-upstream','cleanup','show-password'])
    parser.add_argument('--seconds',type=int,default=15)
    parser.add_argument('--port',default='COM3')
    parser.add_argument('--log')
    args = parser.parse_args()
    if args.action in ['provision','status','test','test-ui','reboot']:
        command = {'status':b'STATUS','test':b'TEST','test-ui':b'TESTUI','reboot':b'REBOOT'}.get(args.action)
        if args.action == 'provision': command = b'CONFIG ' + config_packet(save_config()).hex().encode('ascii')
        records = read_board(command,args.seconds,args.port)
        if args.log:
            path=Path(args.log);path.parent.mkdir(parents=True,exist_ok=True);path.write_text('\n'.join(records)+'\n',encoding='utf-8')
        if args.action == 'provision' and 'OK CONFIG SAVED' not in records: raise RuntimeError('Board did not confirm configuration')
        if args.action in ['test','test-ui'] and not any('"ok":true' in s for s in records): raise RuntimeError('Board self test failed')
    elif args.action == 'show-password':
        cfg=load_config()
        # Native dialog only: password does not enter tool output or a text file.
        C.windll.user32.MessageBoxW(None,'热点：'+cfg['ap']+'\n密码：'+cfg['apPassword']+'\n\n此密码保存在本机受保护的配置中。','ESP8266 NAT 热点',0)
    else:
        wlan=Wlan()
        try:
            if args.action.startswith('radio-'): wlan.radio(args.action=='radio-on')
            elif args.action=='cleanup': wlan.cleanup();wlan.radio(False)
            else:
                cfg=load_config();ap=args.action=='connect-ap'
                wlan.connect('ESP8266-NAT-test' if ap else 'ESP8266-upstream-test',cfg['ap'] if ap else cfg['ssid'],cfg['apPassword'] if ap else cfg['password'])
        finally: wlan.close()
        print('Wi-Fi action requested: '+args.action)

if __name__ == '__main__':
    try: main()
    except Exception as error:
        # Avoid accidental disclosure by not dumping objects or API buffers.
        detail = str(error) if isinstance(error,(RuntimeError,serial.SerialException)) else type(error).__name__
        print('NAT control failed: '+detail,file=sys.stderr)
        sys.exit(1)
