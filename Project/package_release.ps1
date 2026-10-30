$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSCommandPath
$exePath = Join-Path $projectRoot 'x64\Release\AtmosFiore.exe'
$dataPath = Join-Path $projectRoot 'data'
$dlssPath = Join-Path $projectRoot 'x64\Release\nvngx_dlss.dll'
$outputRoot = Join-Path $projectRoot 'ReleasePackage'

foreach ($path in @($exePath, $dataPath, $dlssPath,
        (Join-Path $dataPath 'AtmosFioreLogo.png'),
        (Join-Path $dataPath 'shaders\cso'))) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required release file is missing: $path"
    }
}

$redistRoots = [System.Collections.Generic.List[string]]::new()
if ($env:VCToolsRedistDir) {
    $redistRoots.Add($env:VCToolsRedistDir)
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (Test-Path -LiteralPath $vswhere) {
    $installations = @(& $vswhere -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
    foreach ($installation in $installations) {
        $redistRoots.Add((Join-Path $installation 'VC\Redist\MSVC'))
    }
}

$runtimeDir = $null
foreach ($redistRoot in $redistRoots) {
    if (-not (Test-Path -LiteralPath $redistRoot)) { continue }
    $versions = Get-ChildItem -LiteralPath $redistRoot -Directory |
        Where-Object { $_.Name -match '^\d+\.\d+' } |
        Sort-Object @{ Expression = { [version]$_.Name }; Descending = $true }
    foreach ($version in $versions) {
        $candidate = Join-Path $version.FullName 'x64\Microsoft.VC143.CRT'
        $requiredDlls = @('msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')
        if (@($requiredDlls | Where-Object { -not (Test-Path -LiteralPath (Join-Path $candidate $_)) }).Count -eq 0) {
            $runtimeDir = $candidate
            break
        }
    }
    if ($runtimeDir) { break }
}
if (-not $runtimeDir) {
    throw 'Visual Studio x64 Microsoft.VC143.CRT redistributable DLLs were not found.'
}

New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$packageName = 'AtmosFiore_Release_' + (Get-Date -Format 'yyyyMMdd_HHmmss') + '_' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$stageDir = Join-Path $outputRoot $packageName
$zipPath = Join-Path $outputRoot ($packageName + '.zip')
New-Item -ItemType Directory -Path $stageDir | Out-Null

Copy-Item -LiteralPath $exePath -Destination $stageDir
Copy-Item -LiteralPath $dataPath -Destination (Join-Path $stageDir 'data') -Recurse
Copy-Item -LiteralPath $dlssPath -Destination $stageDir
Get-ChildItem -LiteralPath $runtimeDir -Filter '*.dll' -File |
    Copy-Item -Destination $stageDir

@'
AtmosFiore Release

Run AtmosFiore.exe on Windows 10/11 (64-bit) with a Direct3D 11 GPU.
Keep the data folder and DLL files next to AtmosFiore.exe.
'@ | Set-Content -LiteralPath (Join-Path $stageDir 'README.txt') -Encoding UTF8

Compress-Archive -Path (Join-Path $stageDir '*') -DestinationPath $zipPath -CompressionLevel Optimal

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
try {
    $entryNames = @($archive.Entries | ForEach-Object FullName)
    foreach ($required in @('AtmosFiore.exe', 'data/AtmosFioreLogo.png',
            'nvngx_dlss.dll', 'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')) {
        if ($entryNames -notcontains $required) {
            throw "Required ZIP entry is missing: $required"
        }
    }
    Write-Host "ZIP created: $zipPath"
    Write-Host "Entries: $($entryNames.Count)"
    Write-Host "Size: $([math]::Round((Get-Item -LiteralPath $zipPath).Length / 1MB, 1)) MB"
}
finally {
    $archive.Dispose()
}

$resolvedOutputRoot = (Resolve-Path -LiteralPath $outputRoot).Path.TrimEnd('\')
$resolvedStageDir = (Resolve-Path -LiteralPath $stageDir).Path
if (-not $resolvedStageDir.StartsWith($resolvedOutputRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or
    -not (Split-Path -Leaf $resolvedStageDir).StartsWith('AtmosFiore_Release_', [StringComparison]::Ordinal)) {
    throw "Refusing to remove unexpected staging folder: $resolvedStageDir"
}
Remove-Item -LiteralPath $resolvedStageDir -Recurse -Force
