param(
	[string]$SdkRoot = 'C:\SailfishOS',
	[string]$Target = 'SailfishOS-5.1.0.11-i486'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$work = Join-Path $root '.gea-sailfish\native-tests'
New-Item -ItemType Directory -Force $work | Out-Null
Copy-Item -Force -Path (Join-Path $root 'native\rom_library.cpp'), (Join-Path $root 'native\rom_library.h'), (Join-Path $root 'native\tests\rom_library_test.cpp') -Destination $work
$sfdk = Join-Path $SdkRoot 'bin\sfdk.exe'
if (!(Test-Path $sfdk)) { throw "Sailfish SDK missing at $sfdk" }
$config = "target=$Target"
$noise = 'Already initialized|D-Bus connection failed'
# sfdk writes progress to stderr; with Stop that would abort the script.
$ErrorActionPreference = 'Continue'
Push-Location $work
try {
	& $sfdk --no-session -c $config build-init 2>&1 | ForEach-Object { "$_" } | Where-Object { $_ -notmatch $noise }
	& $sfdk --no-session -c $config build-shell g++ -std=c++17 -Wall -Wextra -Werror -O1 -o rom_library_test rom_library_test.cpp rom_library.cpp 2>&1 | ForEach-Object { "$_" } | Where-Object { $_ -notmatch $noise }
	if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
	& $sfdk --no-session -c $config build-shell ./rom_library_test 2>&1 | ForEach-Object { "$_" } | Where-Object { $_ -notmatch $noise }
	exit $LASTEXITCODE
} finally {
	Pop-Location
}
