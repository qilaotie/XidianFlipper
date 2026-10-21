param(
    [string]$IdfPath = 'C:\esp\v5.5.4\esp-idf',
    [string]$IdfToolsPath = 'C:\Espressif',
    [string]$IdfPythonEnvPath = 'C:\Espressif\tools\python\v5.5.4\venv',
    [switch]$DevMode,
    [switch]$SecureChat,
    [switch]$OledForceOff
)
$ErrorActionPreference = 'Stop'
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
$pythonExecutable = Join-Path $IdfPythonEnvPath 'Scripts\python.exe'
if (-not (Test-Path -LiteralPath $pythonExecutable)) { throw "ESP-IDF Python environment missing: $pythonExecutable" }
$env:IDF_TOOLS_PATH = $IdfToolsPath
$env:IDF_PYTHON_ENV_PATH = $IdfPythonEnvPath
& (Join-Path $IdfPath 'export.ps1')
if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF activation failed' }
$buildArguments = @((Join-Path $PSScriptRoot 'tools\build.py'), '--idf', $IdfPath)
if ($DevMode) { $buildArguments += '--dev-mode' }
if ($SecureChat) { $buildArguments += '--secure-chat' }
if ($OledForceOff) { $buildArguments += '--oled-force-off' }
& $pythonExecutable @buildArguments
if ($LASTEXITCODE -ne 0) { throw 'Build failed; see build/release/validation/build.log' }
