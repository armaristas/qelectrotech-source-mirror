param([string]$CompilerRoot = 'C:\Qt\Tools\mingw1310_64')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$dependencies = Join-Path $projectRoot 'build-faramax\dependencies'
$sourceDirectory = Join-Path $dependencies 'sqlite-amalgamation-3540100'
$archive = Join-Path $dependencies 'sqlite-amalgamation-3540100.zip'
# Official 3.54.1 archive. SHA-256 pinned after verification against the
# SHA3-256 published at https://www.sqlite.org/download.html:
# 92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30
$expectedHash = '0A9C8E34352DAD46006AA82836D5383BFF9DC6BC9CE5A60553C05AA30E6AD1F9'
New-Item -ItemType Directory -Force $dependencies | Out-Null
if (-not (Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -Uri 'https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip' -OutFile $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expectedHash) {
    throw 'La descarga de SQLite no coincide con la versión verificada.'
}
Expand-Archive -LiteralPath $archive -DestinationPath $dependencies -Force
$previousPath = $env:Path
try {
    $env:Path = "$(Join-Path $CompilerRoot 'bin');$previousPath"
    & (Join-Path $CompilerRoot 'bin\gcc.exe') -O2 -DSQLITE_THREADSAFE=1 -DSQLITE_ENABLE_COLUMN_METADATA=1 `
        -c (Join-Path $sourceDirectory 'sqlite3.c') -o (Join-Path $sourceDirectory 'sqlite3.o')
    if ($LASTEXITCODE -ne 0) { throw 'No se ha podido compilar SQLite.' }
    & (Join-Path $CompilerRoot 'bin\ar.exe') rcs (Join-Path $sourceDirectory 'libsqlite3.a') `
        (Join-Path $sourceDirectory 'sqlite3.o')
    if ($LASTEXITCODE -ne 0) { throw 'No se ha podido preparar la biblioteca SQLite.' }
} finally { $env:Path = $previousPath }
