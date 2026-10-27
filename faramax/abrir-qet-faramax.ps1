param([string]$QtPrefix = 'C:\Qt\6.11.1\mingw_64')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $projectRoot 'build-faramax\qelectrotech.exe'
if (-not (Test-Path -LiteralPath $executable)) {
    throw 'Primero compila QET con faramax/build-windows.ps1.'
}
$profileDirectory = Join-Path $projectRoot 'build-faramax\profile'
foreach ($folder in @('config', 'data', 'settings')) {
    New-Item -ItemType Directory -Force (Join-Path $profileDirectory $folder) | Out-Null
}
$previousPath = $env:Path
$previousSettingsDirectory = $env:QET_SETTINGS_DIR
try {
    $env:Path = "$(Join-Path $QtPrefix 'bin');$previousPath"
    $env:QET_SETTINGS_DIR = Join-Path $profileDirectory 'settings'
    # A development profile keeps experimental settings in the build folder.
    $launchArguments = @(
        ('"--config-dir={0}"' -f (Join-Path $profileDirectory 'config'))
        ('"--data-dir={0}"' -f (Join-Path $profileDirectory 'data'))
        ('"--lang-dir={0}"' -f (Join-Path $projectRoot 'build-faramax\lang'))
    )
    Start-Process -FilePath $executable -WorkingDirectory $projectRoot -WindowStyle Normal `
        -ArgumentList $launchArguments
} finally {
    $env:Path = $previousPath
    $env:QET_SETTINGS_DIR = $previousSettingsDirectory
}
