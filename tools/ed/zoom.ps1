# ed zoom's cropper (tools/ed/zoom.py): a part of a picture, enlarged, saved as PNG. No Python image library is needed.
param([string]$Src, [int]$X, [int]$Y, [int]$W, [int]$H, [double]$Scale, [string]$Out)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($Src)
$ow = [int]($W * $Scale); $oh = [int]($H * $Scale)
$dst = New-Object System.Drawing.Bitmap $ow, $oh
$g = [System.Drawing.Graphics]::FromImage($dst)
$g.InterpolationMode = 'HighQualityBicubic'
$g.DrawImage($img, (New-Object System.Drawing.Rectangle 0, 0, $ow, $oh), (New-Object System.Drawing.Rectangle $X, $Y, $W, $H), [System.Drawing.GraphicsUnit]::Pixel)
$dst.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $img.Dispose(); $dst.Dispose()
Write-Output $Out
