param([string]$dir,[int]$from=0)
if(-not (Test-Path $dir)){$dir = Join-Path ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\Screenshots for Claude'))) $dir}
Add-Type -AssemblyName System.Drawing
# one line per half: at each picture, how many of the four item cells (mace, sword, green, lower) have dark or vivid pixels
$cells=@(@(0,156,22,28),@(38,156,26,28),@(38,190,26,22),@(4,222,26,26))
$files=Get-ChildItem "$dir\*.jpg" | Sort-Object Name
$out=@{0=New-Object System.Text.StringBuilder;320=New-Object System.Text.StringBuilder}
$i=0
foreach($f in $files){ if($i -ge $from -and ($i % 6) -eq 0){ $b=New-Object System.Drawing.Bitmap $f.FullName
  foreach($off in 0,320){ $n=0; foreach($c in $cells){ $d=0; $x0=180+$off+$c[0]
      for($x=$x0;$x -lt $x0+$c[2];$x+=2){for($y=$c[1];$y -lt $c[1]+$c[3];$y+=2){ $p=$b.GetPixel($x,$y); if($p.GetBrightness() -lt 0.17 -or $p.GetSaturation() -gt 0.7){$d++} }}
      if($d -ge 4){$n++} }
    [void]$out[$off].Append($n) }
  $b.Dispose() }; $i++ }
"L "+$out[0].ToString(); "R "+$out[320].ToString()
