param(
    [string]$QtPrefix = 'C:\Qt\6.11.1\mingw_64',
    [string]$CompilerRoot = 'C:\Qt\Tools\mingw1310_64',
    [string]$CMake = 'C:\Qt\Tools\CMake_64\bin\cmake.exe',
    [string]$NinjaDirectory = 'C:\Qt\Tools\Ninja',
    [ValidateRange(1, 32)][int]$Jobs = 4,
    [string]$SQLiteDirectory,
    [switch]$KeepDebugSymbols,
    [switch]$RunTests
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build-faramax'
if (-not $SQLiteDirectory) {
    $SQLiteDirectory = Join-Path $buildDirectory 'dependencies\sqlite-amalgamation-3540100'
    if (-not (Test-Path -LiteralPath (Join-Path $SQLiteDirectory 'libsqlite3.a'))) {
        & (Join-Path $PSScriptRoot 'prepare-sqlite.ps1') -CompilerRoot $CompilerRoot
    }
}
$sqliteInclude = $SQLiteDirectory
$sqliteLibrary = Join-Path $SQLiteDirectory 'libsqlite3.a'
$linkerFlags = if ($KeepDebugSymbols) { '' } else { '-Wl,--strip-debug' }
$debugFlags = if ($KeepDebugSymbols) { '-g' } else { '-g0' }
foreach ($required in @($CMake, (Join-Path $QtPrefix 'bin\qmake.exe'),
        (Join-Path $CompilerRoot 'bin\g++.exe'), (Join-Path $NinjaDirectory 'ninja.exe'),
        (Join-Path $sqliteInclude 'sqlite3.h'), $sqliteLibrary)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "No se encuentra: $required" }
}
$previousPath = $env:Path
try {
    $env:Path = "$(Join-Path $CompilerRoot 'bin');$NinjaDirectory;$(Join-Path $QtPrefix 'bin');$previousPath"
    & $CMake -S $projectRoot -B $buildDirectory -G Ninja `
        -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_PREFIX_PATH=$QtPrefix" `
        -DBUILD_WITH_KF=OFF -DPACKAGE_TESTS=ON -DCMAKE_AUTOGEN_PARALLEL=4 -DQET_ENABLE_PCH=ON `
        "-DCMAKE_EXE_LINKER_FLAGS=$linkerFlags" `
        "-DCMAKE_C_FLAGS_DEBUG=$debugFlags" "-DCMAKE_CXX_FLAGS_DEBUG=$debugFlags" `
        "-DSQLite3_INCLUDE_DIR=$sqliteInclude" "-DSQLite3_LIBRARY=$sqliteLibrary"
    if ($LASTEXITCODE -ne 0) { throw 'Ha fallado la configuración de QET.' }
    $targets = @('qelectrotech', 'qelectrotech_lrelease')
    if ($RunTests) {
        $targets += @('tst_terminalflow', 'tst_terminalflowproject', 'tst_arrowtexts', 'tst_gneradortb',
            'tst_terminallabeldisplay', 'tst_terminalcableauto')
    }
    & $CMake --build $buildDirectory --target $targets --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Ha fallado la compilación de QET.' }
    if ($RunTests) {
        & (Join-Path (Split-Path -Parent $CMake) 'ctest.exe') --test-dir $buildDirectory `
            --output-on-failure -R '^(tst_terminalflow|tst_terminalflowproject|tst_arrowtexts|tst_gneradortb|tst_terminallabeldisplay|tst_terminalcableauto)$'
        if ($LASTEXITCODE -ne 0) { throw 'Hay pruebas que no han pasado.' }
    }
} finally {
    $env:Path = $previousPath
}
