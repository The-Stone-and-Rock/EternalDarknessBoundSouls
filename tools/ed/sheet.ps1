# A contact sheet: each picture with a caption, for one look at both players across a moment.
# -list is a file of lines: path<TAB>caption. -out the sheet.
param([string]$list, [string]$out, [int]$cols = 2, [int]$w = 800)
Add-Type -AssemblyName System.Drawing
$rows = Get-Content $list | ForEach-Object { ,($_ -split "`t", 2) }
$first = [Drawing.Image]::FromFile($rows[0][0])
$h = [int]($first.Height * $w / $first.Width) + 34
$first.Dispose()
$b = New-Object Drawing.Bitmap ($w * $cols), ($h * [math]::Ceiling($rows.Count / $cols))
$g = [Drawing.Graphics]::FromImage($b)
$g.Clear([Drawing.Color]::Black)
$font = New-Object Drawing.Font 'Consolas', 10
for ($i = 0; $i -lt $rows.Count; $i++) {
    $x = ($i % $cols) * $w
    $y = [math]::Floor($i / $cols) * $h
    $im = [Drawing.Image]::FromFile($rows[$i][0])
    $g.DrawImage($im, $x, $y + 34, $w, $h - 34)
    $im.Dispose()
    $g.DrawString($rows[$i][1], $font, [Drawing.Brushes]::Yellow, (New-Object Drawing.RectangleF $x, $y, $w, 34))
}
$b.Save($out, [Drawing.Imaging.ImageFormat]::Jpeg)
$out
