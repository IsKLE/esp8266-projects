param(
  [Parameter(Mandatory = $true)] [ValidatePattern('^COM[0-9]+$')] [string]$Port,
  [ValidateRange(1, 3600)] [int]$Seconds = 15,
  [string]$LogPath
)

$ErrorActionPreference = 'Stop'
$python = if ($env:ESP8266_PYTHON) { $env:ESP8266_PYTHON } else { (Get-Command python -ErrorAction Stop).Source }
$monitorArgs = @((Join-Path $PSScriptRoot 'tools\serial_monitor.py'), '--port', $Port, '--seconds', $Seconds)
if ($env:ESP8266_SERIAL_LIBRARY) { $monitorArgs += @('--serial-library', $env:ESP8266_SERIAL_LIBRARY) }
if ($LogPath) { $monitorArgs += @('--log', $LogPath) }
& $python -u @monitorArgs
if ($LASTEXITCODE -ne 0) { throw 'Serial capture failed.' }
