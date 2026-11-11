param([string]$Executable, [string]$FFmpeg)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (!$Executable) {
    foreach ($candidate in @('x64\Release\AtmosFiore.exe','x64\ShowcaseVerify\AtmosFiore.exe','x64\CaptureVerify\AtmosFiore.exe','x64\Debug\AtmosFiore.exe')) {
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
if (!$FFmpeg) { $encoder = Get-Command ffmpeg -ErrorAction SilentlyContinue; if ($encoder) { $FFmpeg = $encoder.Source } }
foreach ($entry in @($report | Where-Object { $_.enabled -and $_.capture -eq 'video' })) {
    if (@($entry.video_frames).Count -ne 60 -or $entry.video_capture_hresult -ne 0) { throw "Video frames incomplete: $($entry.id)" }
    $frameDir = Join-Path $projectRoot "data\capture\screenshots\video\$($entry.id)"
    $output = Join-Path $projectRoot "data\capture\screenshots\video\$($entry.id).mp4"
    if ($entry.video -and $entry.video_encode_exit_code -eq 0 -and (Test-Path -LiteralPath $output)) { continue }
    if (!$FFmpeg) { throw "ffmpeg was not found and MP4 is missing: $($entry.id)" }
    & $FFmpeg -hide_banner -loglevel error -y -framerate 30 -start_number 1 -i (Join-Path $frameDir '%04d.png') -c:v libx264 -pix_fmt yuv420p -crf 18 -movflags +faststart $output
    if ($LASTEXITCODE -ne 0) { throw "MP4 encoding failed: $($entry.id)" }
}
if (!$FFmpeg) { Write-Warning 'ffmpeg was not found. Video frame sequences were saved; pass -FFmpeg to encode MP4.' }
Write-Output (Join-Path $projectRoot 'data\capture\screenshots')
