param(
  [switch]$Upload,
  [string]$Port,
  [ValidatePattern('^[A-Za-z][A-Za-z0-9_]*$')] [string]$Sketch = 'HelloWorld'
)

$ErrorActionPreference = 'Stop'
$cli = if ($env:ARDUINO_CLI) { $env:ARDUINO_CLI } else { (Get-Command arduino-cli -ErrorAction Stop).Source }
$config = if ($env:ARDUINO_CLI_CONFIG) { $env:ARDUINO_CLI_CONFIG } else { Join-Path $PSScriptRoot 'arduino-cli.yaml' }
$build = Join-Path (Join-Path $PSScriptRoot 'build') $Sketch
$sketchPath = Join-Path $PSScriptRoot $Sketch
$board = 'esp8266:esp8266:generic:FlashMode=dout,eesz=4M1M,ResetMethod=nodemcu,baud=115200'

if ($Upload -and $Port -notmatch '^COM[0-9]+$') {
  throw 'Specify the connected board port, for example: .\build.ps1 -Upload -Port COM3'
}
if ($Upload -and $Port -notin [System.IO.Ports.SerialPort]::GetPortNames()) {
  throw "Serial port $Port is not connected."
}

if (-not (Test-Path -LiteralPath (Join-Path $sketchPath "$Sketch.ino"))) {
  throw "Sketch not found: $sketchPath"
}
$compileOptions = @()
if ($Sketch -eq 'NatRelay') {
  $board += ',xtal=160'
  $compileOptions = @('--build-property', 'compiler.c.elf.extra_flags=-Wl,--wrap=ethernet_input_LWIP2')
}
& $cli --config-file $config compile --fqbn $board --build-path $build @compileOptions $sketchPath
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed.' }

if ($Upload) {
  & $cli --config-file $config upload --fqbn $board --port $Port --input-dir $build $sketchPath
  if ($LASTEXITCODE -ne 0) { throw 'Upload failed.' }
}
