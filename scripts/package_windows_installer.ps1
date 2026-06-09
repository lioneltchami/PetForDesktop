param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [Parameter(Mandatory = $true)]
    [string]$ArchivePath,

    [Parameter(Mandatory = $true)]
    [string]$ChecksumPath,

    [string]$IsccPath = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$installerScript = Join-Path $repoRoot 'installer/petForDesktop.iss'
$sourceInstallerDir = Join-Path $repoRoot 'out/packages/legacy-nsis'
$sourceInstallerPath = Join-Path $sourceInstallerDir "PetForDesktopInstaller_$Version.exe"

if (-not (Test-Path $installerScript)) {
    throw "Installer script not found: $installerScript"
}

if (-not (Test-Path $IsccPath)) {
    throw "Inno Setup compiler not found: $IsccPath"
}

$archiveDir = Split-Path -Parent $ArchivePath
$checksumDir = Split-Path -Parent $ChecksumPath
if ($archiveDir) {
    New-Item -ItemType Directory -Force -Path $archiveDir | Out-Null
}
if ($checksumDir -and $checksumDir -ne $archiveDir) {
    New-Item -ItemType Directory -Force -Path $checksumDir | Out-Null
}

New-Item -ItemType Directory -Force -Path $sourceInstallerDir | Out-Null
Remove-Item -Force $sourceInstallerPath -ErrorAction SilentlyContinue
Remove-Item -Force $ArchivePath -ErrorAction SilentlyContinue
Remove-Item -Force $ChecksumPath -ErrorAction SilentlyContinue

& $IsccPath "/DMyAppVersion=$Version" $installerScript
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup compilation failed with exit code $LASTEXITCODE"
}

if (-not (Test-Path $sourceInstallerPath)) {
    throw "Expected installer not found: $sourceInstallerPath"
}

Copy-Item -Force $sourceInstallerPath $ArchivePath
$hash = (Get-FileHash -Algorithm SHA256 -Path $ArchivePath).Hash.ToLower()
"$hash  $([System.IO.Path]::GetFileName($ArchivePath))" | Out-File -FilePath $ChecksumPath -Encoding utf8
