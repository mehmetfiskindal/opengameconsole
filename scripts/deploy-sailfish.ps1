param(
	[string]$PhoneHost = '192.168.1.220',
	[string]$User = 'defaultuser'
)
$ErrorActionPreference = 'Stop'
$rpmDir = Join-Path $PSScriptRoot '..\.gea-sailfish\build\opengameconsole-aarch64\project\RPMS'
$rpm = Get-ChildItem -Path $rpmDir -Recurse -Filter 'harbour-gea-opengameconsole-*.aarch64.rpm' -ErrorAction SilentlyContinue |
	Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $rpm) { throw "No aarch64 RPM under $rpmDir. Run npm run sailfish:build first." }
$target = "$User@$PhoneHost"
& scp $rpm.FullName "${target}:~/"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ssh -t $target "pkcon install-local -y ~/$($rpm.Name)"
exit $LASTEXITCODE
