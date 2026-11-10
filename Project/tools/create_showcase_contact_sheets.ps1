Add-Type -AssemblyName System.Drawing
$showcaseRoot = Join-Path $PSScriptRoot '..\data\showcases\audit'
$entries = Get-Content -LiteralPath (Join-Path $showcaseRoot 'report.json') -Raw | ConvertFrom-Json | Where-Object enabled
for ($page = 0; $page -lt [Math]::Ceiling($entries.Count / 16); $page++) {
    $canvas = [Drawing.Bitmap]::new(1600, 1000)
    $graphics = [Drawing.Graphics]::FromImage($canvas)
    $graphics.Clear([Drawing.Color]::FromArgb(20, 20, 20))
    $font = [Drawing.Font]::new('Segoe UI', 12)
    for ($cell = 0; $cell -lt 16; $cell++) {
        $index = $page * 16 + $cell
        if ($index -ge $entries.Count) { break }
        $entry = $entries[$index]
        $x = ($cell % 4) * 400
        $y = [Math]::Floor($cell / 4) * 250
        $picture = [Drawing.Image]::FromFile((Join-Path $showcaseRoot ($entry.id + '_on.png')))
        $graphics.DrawImage($picture, [int]$x, [int]$y, 400, 225)
        $graphics.DrawString($entry.id, $font, [Drawing.Brushes]::White, [single]($x + 8), [single]($y + 225))
        $picture.Dispose()
    }
    $canvas.Save((Join-Path $showcaseRoot ('contact_' + ($page + 1) + '.png')), [Drawing.Imaging.ImageFormat]::Png)
    $font.Dispose(); $graphics.Dispose(); $canvas.Dispose()
}
