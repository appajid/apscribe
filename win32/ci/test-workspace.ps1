# GPL: see COPYING. Run the same initialized QtTest suites as macOS/Linux.
#Requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$AppDir,
    [Parameter(Mandatory = $true)][string]$OutputDir
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0
$app = Join-Path $AppDir 'Apscribe.exe'
if (-not (Test-Path -LiteralPath $app -PathType Leaf)) { throw "Missing test application: $app" }
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
foreach ($suite in @('workspace', 'script-shaping')) {
    $prefs = Join-Path $OutputDir "prefs-$suite"
    $log = Join-Path $OutputDir "$suite.txt"
    New-Item -ItemType Directory -Path $prefs -Force | Out-Null
    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = $app
    $start.Arguments = "--no-splash --prefs `"$prefs`" --document-tests --suite=$suite -o `"$log,txt`""
    $start.WorkingDirectory = $AppDir
    $start.UseShellExecute = $false
    $start.EnvironmentVariables.Remove('QT_PLUGIN_PATH')
    $start.EnvironmentVariables.Remove('QT_QPA_PLATFORM_PLUGIN_PATH')
    $start.EnvironmentVariables['QT_QPA_PLATFORM'] = 'windows'
    $start.EnvironmentVariables['APSCRIBE_REQUIRE_SCRIPT_FONTS'] = '1'
    $start.EnvironmentVariables['APSCRIBE_SCRIPT_QA_OUTPUT'] = (Join-Path $OutputDir 'script-images')
    $process = [System.Diagnostics.Process]::Start($start)
    try {
        if (-not $process.WaitForExit(180000)) {
            $process.Kill()
            throw "$suite timed out"
        }
        if (-not (Test-Path -LiteralPath $log)) { throw "$suite produced no QtTest report" }
        Get-Content -LiteralPath $log | Write-Host
        $report = Get-Content -Raw -LiteralPath $log
        if ($process.ExitCode -ne 0 -or $report -notmatch '0 failed, 0 skipped') {
            throw "$suite failed or skipped coverage (exit $($process.ExitCode))"
        }
    }
    finally {
        if (-not $process.HasExited) { $process.Kill() }
        $process.Dispose()
    }
}
