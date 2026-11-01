$ErrorActionPreference = 'Stop'
$commit = '482547a3d56e3aa8d22d7ffed33c9f1588bfe17a'
$root = Split-Path -Parent $PSScriptRoot
$destination = Join-Path $root '.reference\aravis'
$files = @(
    'src/arv-fake-camera.xml',
    'src/arv-gev-bootstrap.xml',
    'src/GenApiSchema_Version_1_1.xsd',
    'src/arvgvcp.c',
    'src/arvgvcpprivate.h',
    'src/arvgvsp.c',
    'src/arvgvspprivate.h',
    'src/arvgvfakecamera.c',
    'src/arvfakecamera.c',
    'tests/fakegv.c',
    'tests/data/genicam.xml'
)

foreach ($file in $files) {
    $target = Join-Path $destination ($file -replace '/', '\')
    $parent = Split-Path -Parent $target
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    $uri = "https://raw.githubusercontent.com/AravisProject/aravis/$commit/$file"
    Invoke-WebRequest -Uri $uri -OutFile $target
    if ((Get-Item $target).Length -eq 0) { throw "Downloaded reference is empty: $file" }
}

Write-Output "Downloaded $($files.Count) pinned Aravis reference files to $destination"
