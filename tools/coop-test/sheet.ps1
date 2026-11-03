# Puts captured frames (burst.ps1) on one contact sheet, each labelled with its number.
param([string]$dir,[string]$out,[int[]]$pics)
Add-Type -AssemblyName System.Drawing
$w=480;$h=270;$cols=3;$rows=[math]::Ceiling($pics.Count/$cols)
$b=New-Object Drawing.Bitmap ($w*$cols),($h*$rows);$g=[Drawing.Graphics]::FromImage($b);$f=New-Object Drawing.Font 'Arial',14;$br=[Drawing.Brushes]::Yellow
for($i=0;$i -lt $pics.Count;$i++){$p=$pics[$i];$im=[Drawing.Image]::FromFile((Join-Path $dir ($p.ToString('D5')+'.jpg')));$x=($i%$cols)*$w;$y=[math]::Floor($i/$cols)*$h;$g.DrawImage($im,$x,$y,$w,$h);$g.DrawString("$p",$f,$br,$x+4,$y+4);$im.Dispose()}
$b.Save($out,[Drawing.Imaging.ImageFormat]::Jpeg)
