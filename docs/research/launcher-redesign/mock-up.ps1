param([string]$root = "E:\Eternal Darkness Recomp Fork", [string]$photo = "C:\Users\stevi\.claude\uploads\5bfba78f-d0ec-4a18-8e74-3f4baf38214a\3b380bd2-image.jpg")
# Mock-up 2: the same layout in the title screen's Choose Character colours (menu.rcss body.skin-title):
# panel #0a1518, lines #3b5359 / #486168, the lit thing #a0e3f1 with a glow, gold #c9ab68, blue #8fc8f0, words #8db2ba / #6d9098.
Add-Type -AssemblyName System.Drawing
$out = Join-Path $root "Screenshots for Claude\launcher-redesign"
New-Item -ItemType Directory -Force $out | Out-Null
$W = 1920; $H = 1080
$bmp = New-Object System.Drawing.Bitmap $W, $H
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
function C($hex, $alpha = 255) { $v = [Convert]::ToInt32($hex, 16); return [System.Drawing.Color]::FromArgb($alpha, ($v -shr 16) -band 255, ($v -shr 8) -band 255, $v -band 255) }
function Fill($hex, $alpha, $x, $y, $w, $h) { $br = New-Object System.Drawing.SolidBrush (C $hex $alpha); $g.FillRectangle($br, [single]$x, [single]$y, [single]$w, [single]$h); $br.Dispose() }
function Box($hex, $alpha, $x, $y, $w, $h, $t = 1) { $pn = New-Object System.Drawing.Pen (C $hex $alpha), $t; $g.DrawRectangle($pn, [single]$x, [single]$y, [single]$w, [single]$h); $pn.Dispose() }
function Glow($hex, $x, $y, $w, $h) { for ($i = 10; $i -ge 1; $i--) { Box $hex (6 + (10 - $i) * 3) ($x - $i) ($y - $i) ($w + 2 * $i) ($h + 2 * $i) 2 } }

# The owner's picture behind everything, dimmed with the title look's blue-black.
$img = [System.Drawing.Image]::FromFile($photo)
$scale = [Math]::Max($W / $img.Width, $H / $img.Height)
$iw = [int]($img.Width * $scale); $ih = [int]($img.Height * $scale)
$g.DrawImage($img, [int](($W - $iw) / 2), [int](($H - $ih) / 2), $iw, $ih)
$img.Dispose()
Fill "040c0f" 196 0 0 $W $H
$rect = New-Object System.Drawing.Rectangle 340, 0, 1000, $H
$lg = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, (C "02080a" 150), (C "02080a" 0), 0.0
$g.FillRectangle($lg, $rect); $lg.Dispose()

$pfc = New-Object System.Drawing.Text.PrivateFontCollection
foreach ($f in "AlegreyaSC-Bold.ttf", "AlegreyaSC-Regular.ttf", "IMFellEnglish-Regular.ttf", "IMFellEnglish-Italic.ttf", "FiraSans-Regular.ttf", "FiraSans-Bold.ttf", "LaBelleAurore.ttf") { $pfc.AddFontFile((Join-Path $root "res\fonts\$f")) }
function Fam($name) { foreach ($f in $pfc.Families) { if ($f.Name -eq $name) { return $f } }; return $pfc.Families[0] }
$alegreya = Fam "Alegreya SC"; $fell = Fam "IM FELL English"; $fira = Fam "Fira Sans"; $hand = Fam "La Belle Aurore"
$StReg = [System.Drawing.FontStyle]::Regular; $StBold = [System.Drawing.FontStyle]::Bold; $StItal = [System.Drawing.FontStyle]::Italic
function Text($s, $fam, $size, $style, $hex, $x, $y, $align = 0, $alpha = 255) {
    $f = New-Object System.Drawing.Font($fam, [single]$size, $style, [System.Drawing.GraphicsUnit]::Pixel)
    $br = New-Object System.Drawing.SolidBrush (C $hex $alpha)
    $fmt = New-Object System.Drawing.StringFormat
    if ($align -eq 1) { $fmt.Alignment = [System.Drawing.StringAlignment]::Center } elseif ($align -eq 2) { $fmt.Alignment = [System.Drawing.StringAlignment]::Far }
    $g.DrawString($s, $f, $br, [single]$x, [single]$y, $fmt); $f.Dispose(); $br.Dispose()
}
function Spaced($s) { return (($s.ToCharArray() | ForEach-Object { [string]$_ }) -join " ") }
function Diamond($cx, $cy, $half) {
    $pts = @((New-Object System.Drawing.PointF $cx, ($cy - $half)), (New-Object System.Drawing.PointF ($cx + $half), $cy), (New-Object System.Drawing.PointF $cx, ($cy + $half)), (New-Object System.Drawing.PointF ($cx - $half), $cy))
    $br = New-Object System.Drawing.SolidBrush (C "b3301f"); $g.FillPolygon($br, [System.Drawing.PointF[]]$pts); $br.Dispose()
    $pn = New-Object System.Drawing.Pen (C "d9b56a"), 1; $g.DrawPolygon($pn, [System.Drawing.PointF[]]$pts); $pn.Dispose()
}

# ---- Side bar
Fill "0a1518" 238 0 0 340 $H
Fill "3b5359" 255 339 0 1 $H
Text (Spaced "ETERNAL DARKNESS") $alegreya 19 $StReg "8db2ba" 34 40
Text (Spaced "BOUND SOULS") $alegreya 26 $StBold "c9ab68" 34 66
Fill "3b5359" 255 34 118 120 1; Diamond 170 118 6; Fill "3b5359" 255 186 118 120 1
$tabs = "Play", "Choose Character", "Display", "Graphics", "Sound", "Controls", "Mods", "Credits"
$ty = 162
for ($i = 0; $i -lt $tabs.Count; $i++) {
    if ($i -eq 0) {
        $tr = New-Object System.Drawing.Rectangle 0, $ty, 340, 60
        $tg = New-Object System.Drawing.Drawing2D.LinearGradientBrush $tr, (C "486168" 190), (C "486168" 0), 0.0
        $g.FillRectangle($tg, $tr); $tg.Dispose()
        Fill "a0e3f1" 255 0 $ty 4 60
        Text $tabs[$i] $hand 34 $StReg "a0e3f1" 34 ($ty + 4)
    } else {
        Text $tabs[$i] $hand 34 $StReg "6d9098" 34 ($ty + 4)
    }
    $ty += 64
}
Text "Quit" $hand 34 $StReg "6d9098" 34 ($H - 170)
Text "Built on Eternal Darkness Recomp by griches" $fira 13 $StReg "486168" 34 ($H - 62)

# ---- Main panel: one column, as wide as the title's first line, so every edge lines up.
$mx = 420
$fA = New-Object System.Drawing.Font($alegreya, [single]52, $StReg, [System.Drawing.GraphicsUnit]::Pixel)
$fB = New-Object System.Drawing.Font($alegreya, [single]40, $StBold, [System.Drawing.GraphicsUnit]::Pixel)
$line1 = "ETERNAL DARKNESS: SANITY'S REQUIEM"
$line2 = Spaced "BOUND SOULS"
$cw = [int]($g.MeasureString($line1, $fA).Width) - 14
$yA = 84; $yB = 152
function TitleWords($gfx, $hexA, $hexB) {
    $bA = New-Object System.Drawing.SolidBrush (C $hexA); $bB = New-Object System.Drawing.SolidBrush (C $hexB)
    $gfx.DrawString($line1, $fA, $bA, [single]($mx - 6), [single]$yA)
    $gfx.DrawString($line2, $fB, $bB, [single]($mx - 4), [single]$yB)
    $bA.Dispose(); $bB.Dispose()
}
$layer = New-Object System.Drawing.Bitmap $W, $H
$lgfx = [System.Drawing.Graphics]::FromImage($layer)
$lgfx.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
TitleWords $lgfx "7fe0f5" "e0b860"
$lgfx.Dispose()
$sw = [int]($W / 12); $sh = [int]($H / 12)
$small = New-Object System.Drawing.Bitmap $sw, $sh
$sg = [System.Drawing.Graphics]::FromImage($small)
$sg.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBilinear
$sg.DrawImage($layer, 0, 0, $sw, $sh); $sg.Dispose(); $layer.Dispose()
$cm = New-Object System.Drawing.Imaging.ColorMatrix
$cm.Matrix33 = 0.6
$ia = New-Object System.Drawing.Imaging.ImageAttributes
$ia.SetColorMatrix($cm)
$g.DrawImage($small, (New-Object System.Drawing.Rectangle 0, 0, $W, $H), 0, 0, $sw, $sh, [System.Drawing.GraphicsUnit]::Pixel, $ia)
$small.Dispose(); $ia.Dispose()
TitleWords $g "dff3f8" "d4b874"
$fA.Dispose(); $fB.Dispose()
Text (Spaced "FOR ONE OR TWO PLAYERS") $fira 15 $StReg "6d9098" $mx 222
Fill "3b5359" 255 $mx 262 $cw 1

$gap = 24
$tw = [int](($cw - 2 * $gap) / 3)
Text (Spaced "READY") $fira 14 $StBold "a0e3f1" $mx 300
if ($env:MOCK_UPDATE -eq "1") {
    Text "Version 0.1.6  -  0.1.7 is out" $fell 19 $StReg "c9ab68" ($mx + $cw - 176) 292 2
    Fill "1d4450" 255 ($mx + $cw - 160) 286 160 34; Box "a0e3f1" 255 ($mx + $cw - 160) 286 160 34 2
    Text (Spaced "UPDATE") $fira 12 $StBold "e6f7fb" ($mx + $cw - 80) 296 1
} else {
    Text "Version 0.1.6" $fell 19 $StReg "8db2ba" ($mx + $cw - 176) 292 2
    Fill "000000" 200 ($mx + $cw - 160) 286 160 34; Box "486168" 255 ($mx + $cw - 160) 286 160 34
    Text (Spaced "CHECK UPDATES") $fira 12 $StBold "a0e3f1" ($mx + $cw - 80) 296 1
}
# Play, Load and Continue: three buttons, each as wide as the tile under it.
Glow "a0e3f1" $mx 332 $tw 94
$pts = @((New-Object System.Drawing.PointF $mx, 332), (New-Object System.Drawing.PointF ($mx + $tw), 332), (New-Object System.Drawing.PointF ($mx + $tw), 402), (New-Object System.Drawing.PointF ($mx + $tw - 24), 426), (New-Object System.Drawing.PointF $mx, 426))
$prect = New-Object System.Drawing.Rectangle $mx, 332, $tw, 94
$pb = New-Object System.Drawing.Drawing2D.LinearGradientBrush $prect, (C "1d4450"), (C "0e262d"), 90.0
$g.FillPolygon($pb, [System.Drawing.PointF[]]$pts); $pb.Dispose()
$pp = New-Object System.Drawing.Pen (C "a0e3f1"), 2
$g.DrawPolygon($pp, [System.Drawing.PointF[]]$pts); $pp.Dispose()
Text "Play" $hand 52 $StReg "e6f7fb" ($mx + $tw / 2) 342 1
function SideButton($x, $label, $under) {
    Fill "0a1518" 232 $x 332 $tw 94
    Box "486168" 255 $x 332 $tw 94
    Text $label $hand 38 $StReg "8db2ba" ($x + $tw / 2) 336 1
    Text $under $fell 17 $StItal "6d9098" ($x + $tw / 2) 392 1
}
SideButton ($mx + $tw + $gap) "Load" "Your save states"
SideButton ($mx + 2 * ($tw + $gap)) "Continue" "Newest: slot 3, co-op"

function Tile($x, $label, $value) {
    Fill "0a1518" 232 $x 462 $tw 104
    Box "3b5359" 255 $x 462 $tw 104
    Text (Spaced $label) $fira 12 $StReg "6d9098" ($x + 18) 476
    Text $value $fell 23 $StReg "cfe9f0" ($x + 18) 498
    Text (Spaced "CHANGE") $fira 11 $StReg "c9ab68" ($x + 18) 538
}
Tile $mx "HD TEXTURES" "On: 7,719 found"
Tile ($mx + $tw + $gap) "RESOLUTION" "2x (2560 x 1440)"
Tile ($mx + 2 * ($tw + $gap)) "FRAME RATE" "120 solo, 60 co-op"

function Section($y, $label) {
    Text (Spaced $label) $alegreya 17 $StBold "c9ab68" $mx $y
    Fill "3b5359" 255 ($mx + 24 + 15 * $label.Length) ($y + 12) ($cw - 24 - 15 * $label.Length) 1
}
function Line($y, $left, $right, $hex) {
    Text $left $fell 20 $StReg "8db2ba" ($mx + 16) $y
    Text $right $fell 19 $StReg $hex ($mx + $cw) $y 2
}
function Button($x, $y, $w, $label, $on) {
    Fill "000000" 200 $x $y $w 36
    if ($on) { Box "a0e3f1" 255 $x $y $w 36; Text (Spaced $label) $fira 12 $StBold "a0e3f1" ($x + $w / 2) ($y + 11) 1 }
    else { Box "486168" 255 $x $y $w 36; Text (Spaced $label) $fira 12 $StBold "486168" ($x + $w / 2) ($y + 11) 1 }
}
Section 612 "RECOMMENDED SETTINGS"
Text "Recommended settings for this PC: coming soon." $fell 20 $StItal "6d9098" ($mx + 16) 646
Button ($mx + $cw - 140) 642 140 "AUTO-SET" $false

Section 716 "CONTROLLERS"
Line 748 "Player I" "Xbox controller: connected" "c9ab68"
Line 784 "Player II" "Keyboard and mouse" "8fc8f0"
Text "Controller settings" $fell 20 $StReg "8db2ba" ($mx + 16) 830
Button ($mx + $cw - 140) 826 140 "CONFIGURE" $true

Section 900 "SAVES"
Line 932 "Save states" "3 of 100 used" "8db2ba"

# Help line.
Fill "050b0e" 240 0 ($H - 44) $W 44
Fill "3b5359" 255 0 ($H - 45) $W 1
Text "Arrows: move     A / Enter: select     Left / Right: change     B / Esc: back     F11: fullscreen" $fell 17 $StReg "5e848d" 34 ($H - 34)

$g.Dispose()
$bmp.Save((Join-Path $out $(if ($env:MOCK_UPDATE -eq "1") { "mock-7b-update-available.png" } else { "mock-7a-check-updates.png" })), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
"done"
