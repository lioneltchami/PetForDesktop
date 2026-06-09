param(
    [Parameter(Mandatory = $true)]
    [string]$SourceRoot,

    [Parameter(Mandatory = $true)]
    [string]$ArchivePath,

    [Parameter(Mandatory = $true)]
    [string]$ChecksumPath
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $SourceRoot)) {
    throw "Source root not found: $SourceRoot"
}

$archiveDir = Split-Path -Parent $ArchivePath
$checksumDir = Split-Path -Parent $ChecksumPath
if ($archiveDir) {
    New-Item -ItemType Directory -Force -Path $archiveDir | Out-Null
}
if ($checksumDir -and $checksumDir -ne $archiveDir) {
    New-Item -ItemType Directory -Force -Path $checksumDir | Out-Null
}

$stagingRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("petfordesktop_windows_stage_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $stagingRoot | Out-Null

try {
    Copy-Item -Recurse -Force (Join-Path $SourceRoot "*") $stagingRoot
    Compress-Archive -Path (Join-Path $stagingRoot "*") -DestinationPath $ArchivePath -CompressionLevel Optimal -Force

    $hash = (Get-FileHash -Algorithm SHA256 -Path $ArchivePath).Hash.ToLower()
    "$hash  $([System.IO.Path]::GetFileName($ArchivePath))" | Out-File -FilePath $ChecksumPath -Encoding utf8
}
finally {
    Remove-Item -Recurse -Force $stagingRoot -ErrorAction SilentlyContinue
}
