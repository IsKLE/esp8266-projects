$ErrorActionPreference = 'Stop'
$env:NUGET_PACKAGES = if ($env:NUGET_PACKAGES) { $env:NUGET_PACKAGES } else { Join-Path $env:LOCALAPPDATA 'NuGet\Cache' }
$env:DOTNET_CLI_HOME = if ($env:DOTNET_CLI_HOME) { $env:DOTNET_CLI_HOME } else { Join-Path $env:LOCALAPPDATA 'ESP8266\dotnet-home' }
$env:DOTNET_CLI_TELEMETRY_OPTOUT = '1'
$output = Join-Path $PSScriptRoot 'PCMonitor\app'
& dotnet publish (Join-Path $PSScriptRoot 'PCMonitor\PCMonitor.csproj') -c Release -r win-x64 --self-contained false -o $output
if ($LASTEXITCODE -ne 0) { throw 'PC monitor build failed.' }
