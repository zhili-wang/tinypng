param(
    [string]$QtRoot = $env:TINYPNG_QT_DIR,
    [string]$Version = "1.2.0"
)
$ErrorActionPreference = "Stop"
if ($Version -notmatch "^\d+\.\d+\.\d+$") { throw "无效版本号：$Version" }
$projectRoot = Split-Path -Parent $PSScriptRoot
if ($QtRoot) {
    $qmake = Join-Path $QtRoot "bin/qmake.exe"
} else {
    $qmake = (Get-Command qmake.exe -ErrorAction Stop).Source
}
$qtBin = Split-Path -Parent $qmake
$deploy = Join-Path $qtBin "windeployqt.exe"
if (!(Test-Path $qmake) -or !(Test-Path $deploy)) { throw "缺少完整的 Qt MSVC x64 SDK。" }
Get-Command nmake.exe -ErrorAction Stop | Out-Null
$iscc = Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6/ISCC.exe"
if (!(Test-Path $iscc)) { throw "缺少 Inno Setup 6，无法生成安装程序。" }
if (!$env:VCToolsRedistDir) { throw "请在 Visual Studio x64 Developer PowerShell 中运行。" }
$buildId = [guid]::NewGuid().ToString("N").Substring(0, 8)
$buildRoot = Join-Path $projectRoot "build/windows-x64-$Version-$buildId"
$checksRoot = Join-Path $buildRoot "checks"
$stage = Join-Path $buildRoot "package"
$installerRoot = Join-Path $projectRoot "Installer"
New-Item -ItemType Directory -Force $checksRoot, $installerRoot | Out-Null
Push-Location $checksRoot
try {
    & $qmake (Join-Path $projectRoot "tests/regression.pro") "-after" "CONFIG+=release" "CONFIG-=debug" "CONFIG-=debug_and_release" "DESTDIR=$checksRoot/bin"
    if ($LASTEXITCODE) { throw "测试项目配置失败。" }
    & nmake.exe /NOLOGO
    if ($LASTEXITCODE) { throw "测试项目编译失败。" }
    $env:QT_QPA_PLATFORM = "offscreen"
    & ./bin/regression.exe
    if ($LASTEXITCODE) { throw "SVG 或重试筛选检查失败。" }
    Remove-Item Env:QT_QPA_PLATFORM
} finally { Pop-Location }
$integrationRoot = Join-Path $buildRoot "integration"
New-Item -ItemType Directory -Force $integrationRoot | Out-Null
Push-Location $integrationRoot
try {
    & $qmake (Join-Path $projectRoot "tests/integration.pro") "-after" "CONFIG+=release" "CONFIG-=debug" "CONFIG-=debug_and_release" "DESTDIR=$integrationRoot/bin"
    if ($LASTEXITCODE) { throw "集成测试配置失败。" }
    & nmake.exe /NOLOGO
    if ($LASTEXITCODE) { throw "集成测试编译失败。" }
    $env:QT_QPA_PLATFORM = "offscreen"
    & ./bin/integration.exe
    if ($LASTEXITCODE) { throw "任务调度、暂停或重试检查失败。" }
    Remove-Item Env:QT_QPA_PLATFORM
} finally { Pop-Location }
Push-Location $buildRoot
try {
    & $qmake (Join-Path $projectRoot "tinypng/VsTinypng.pro") "-after" "CONFIG+=release" "CONFIG-=debug" "CONFIG-=debug_and_release" "DESTDIR=$buildRoot/bin" "TARGET=TinyPNG" "VERSION=$Version"
    if ($LASTEXITCODE) { throw "应用配置失败。" }
    & nmake.exe /NOLOGO
    if ($LASTEXITCODE) { throw "应用编译失败。" }
} finally { Pop-Location }
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory $stage | Out-Null
Copy-Item (Join-Path $buildRoot "bin/TinyPNG.exe") $stage
& $deploy --release --no-compiler-runtime --no-translations (Join-Path $stage "TinyPNG.exe")
if ($LASTEXITCODE) { throw "Qt 运行库部署失败。" }
# Include the MSVC runtime beside the executable so the installer can run per-user.
$crtRoot = Join-Path $env:VCToolsRedistDir "x64"
$crt = Get-ChildItem $crtRoot -Directory -Filter "Microsoft.VC*.CRT" | Sort-Object Name -Descending | Select-Object -First 1
if (!$crt) { throw "找不到 MSVC x64 CRT 运行库。" }
Copy-Item (Join-Path $crt.FullName "*.dll") $stage
$env:TINYPNG_STAGE = $stage
$env:TINYPNG_INSTALLER_DIR = $installerRoot
& $iscc "/DAppVersion=$Version" (Join-Path $PSScriptRoot "windows-installer.iss")
if ($LASTEXITCODE) { throw "Windows 安装程序生成失败。" }
$zip = Join-Path $installerRoot "TinyPNG-Windows-x64-$Version.zip"
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -Force
Write-Host "已生成 Windows x64 安装程序和便携 ZIP：$installerRoot"
