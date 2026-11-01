param(
    [Parameter(Position = 0)]
    [ValidateSet('configure', 'build', 'test', 'xml-test', 'package', 'run')]
    [string]$Action = 'build',
    [Parameter(Position = 1)]
    [string]$Filter = ''
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$cmake = 'G:\QT\Tools\CMake_64\bin\cmake.exe'
$ninja = 'G:\QT\Tools\Ninja\ninja.exe'
$vsVars = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat'
$python = 'C:\Users\wtg\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\python.exe'
$qtRoot = 'G:\QT\6.8.3\msvc2022_64'
$qtBin = Join-Path $qtRoot 'bin'
$msvcRedist = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Redist\MSVC\14.44.35112\x64\Microsoft.VC143.CRT'
$packageDir = Join-Path $repo 'dist\camera_emulator'
$buildDir = Join-Path $repo '.build\windows-debug'
$releaseDir = Join-Path $repo '.build\windows-release'
$cameraExe = Join-Path $buildDir 'camera_emulator.exe'
$runPath = if (Test-Path -LiteralPath (Join-Path $packageDir 'camera_emulator.exe')) { Join-Path $packageDir 'camera_emulator.exe' } else { $cameraExe }
$releaseExe = Join-Path $releaseDir 'camera_emulator.exe'
$appDataDir = Join-Path $env:LOCALAPPDATA 'CameraEmulator'
$logDir = Join-Path $appDataDir 'logs'
$runLogDir = Join-Path $repo '.build\runtime-logs'

foreach ($path in @($cmake, $ninja, $vsVars)) {
    if (-not (Test-Path $path)) { throw "Required build tool not found: $path" }
}

if ($Action -eq 'run') {
    $resolvedRunPath = [System.IO.Path]::GetFullPath($runPath)
    $resolvedPackageExe = [System.IO.Path]::GetFullPath((Join-Path $packageDir 'camera_emulator.exe'))
    if ($resolvedRunPath -eq $resolvedPackageExe) { $env:CAMERA_LOG_DIR = $runLogDir }
    $env:PATH = $qtBin + ';' + $env:PATH
    $process = Start-Process -FilePath $resolvedRunPath -WorkingDirectory (Split-Path -Parent $resolvedRunPath) -Environment @{ CAMERA_LOG_DIR = $runLogDir; PATH = $qtBin + ';' + $env:PATH } -PassThru
    Write-Output ('Started camera_emulator.exe PID=' + $process.Id)
    exit 0
}

$cmakeCommand = switch ($Action) {
    'configure' { '"' + $cmake + '" --preset windows-debug' }
    'build' { '"' + $cmake + '" --build --preset windows-debug' }
    'test' {
        if ($Filter) { '"' + $cmake + '" --build --preset windows-debug --target camera_tests && "' + (Join-Path $repo '.build\windows-debug\camera_tests.exe') + '" "[' + $Filter + ']"' }
        else { '"' + $cmake + '" --build --preset windows-debug && "' + $cmake + '" --build ' + '"' + (Join-Path $repo '.build\windows-debug') + '" --target test' }
    }
    'xml-test' { '"' + $cmake + '" --build --preset windows-debug --target camera_xml_dump && "' + (Join-Path $repo '.build\windows-debug\camera_xml_dump.exe') + '" "' + (Join-Path $repo '.build\windows-debug\generated-camera.xml') + '" && "' + $python + '" "' + (Join-Path $repo 'tools\validate_xml.py') + '" "' + (Join-Path $repo '.build\windows-debug\generated-camera.xml') + '" --schema "' + (Join-Path $repo '.reference\aravis\src\GenApiSchema_Version_1_1.xsd') + '" --contract "' + (Join-Path $repo 'tests\fixtures\xml_contract.json') + '"' }
    'package' { '"' + $cmake + '" --preset windows-release && "' + $cmake + '" --build --preset windows-release --target clean && "' + $cmake + '" --build --preset windows-release --target camera_emulator && "' + $cmake + '" -E make_directory "' + $packageDir + '" && "' + $cmake + '" -E copy_if_different "' + $releaseExe + '" "' + $packageDir + '\camera_emulator.exe" && "' + $cmake + '" -E make_directory "' + $packageDir + '\virtual-cameras" && "' + $cmake + '" -E copy_if_different "' + (Join-Path $repo 'virtual-cameras\example-camera.json') + '" "' + $packageDir + '\virtual-cameras\example-camera.json" && "' + $cmake + '" -E copy_if_different "' + (Join-Path $repo 'wintun\bin\amd64\wintun.dll') + '" "' + $packageDir + '\wintun.dll" && "' + $cmake + '" -E copy_directory "' + $msvcRedist + '" "' + $packageDir + '" && "' + (Join-Path $qtBin 'windeployqt.exe') + '" --release --no-translations --dir "' + $packageDir + '" "' + (Join-Path $packageDir 'camera_emulator.exe') + '"' }
    'run' { '"' + $runPath + '"' }
}

$env:PATH = $qtBin + ';G:\QT\Tools\Ninja;' + (Split-Path -Parent $cmake) + ';' + $env:PATH
if ($Action -eq 'package') { $env:PATH = 'C:\Program Files (x86)\Common Files\MVS\Runtime\Win64_x64;' + $env:PATH }
$cmd = 'call "' + $vsVars + '" x64 -vcvars_ver=14.44 >nul && ' + $cmakeCommand
Push-Location $repo
try {
    if ($Action -eq 'package') {
        $resolvedPackageDir = [System.IO.Path]::GetFullPath($packageDir)
        $expectedPackageDir = [System.IO.Path]::GetFullPath((Join-Path $repo 'dist\camera_emulator'))
        if ($resolvedPackageDir -ne $expectedPackageDir) { throw "Unexpected package output path: $resolvedPackageDir" }
        if (Test-Path -LiteralPath $resolvedPackageDir) { Remove-Item -LiteralPath $resolvedPackageDir -Recurse -Force }
    }
    & cmd.exe /d /s /c $cmd
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
finally {
    Pop-Location
}

