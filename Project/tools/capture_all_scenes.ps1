param([string]$Executable)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (!$Executable) {
    foreach ($candidate in @('x64\CaptureVerify\AtmosFiore.exe','x64\Release\AtmosFiore.exe','x64\Debug\AtmosFiore.exe')) {
        $target = Join-Path $projectRoot $candidate
        if (Test-Path -LiteralPath $target) { $Executable = $target; break }
    }
}
if (!$Executable) { throw 'Build AtmosFiore first, or supply -Executable with the built executable path.' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$captureProcess = Start-Process -FilePath $Executable -ArgumentList '--capture-scenes' -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -Wait
if ($captureProcess.ExitCode -ne 0) { throw "Capture process failed: $($captureProcess.ExitCode)" }
$report = @(Get-Content -LiteralPath (Join-Path $projectRoot 'data\capture\screenshots\report.json') -Raw | ConvertFrom-Json)
$catalog = @(Get-Content -LiteralPath (Join-Path $projectRoot 'data\showcases\catalog.json') -Raw | ConvertFrom-Json)
if ($report.Count -ne (($catalog.Count + 1) * 2)) { throw 'Capture run ended before all scenes were saved.' }
if (@($report | Where-Object { $_.capture_hresult -ne 0 -or $_.error }).Count) { throw 'One or more scenes failed to capture. See report.json.' }
Write-Output (Join-Path $projectRoot 'data\capture\screenshots')
