Add-Type -Name W -Namespace N -MemberDefinition '[DllImport("user32.dll")] public static extern bool SetProcessDPIAware(); [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r); [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr d, uint f); public struct RECT{public int L,T,R,B;}'
[N.W]::SetProcessDPIAware()|Out-Null
Add-Type -AssemblyName System.Drawing
$p=Get-Process EternalDarkness -ErrorAction SilentlyContinue | Select-Object -First 1
$h=$p.MainWindowHandle; $r=New-Object N.W+RECT; [N.W]::GetClientRect($h,[ref]$r)|Out-Null
$b=New-Object Drawing.Bitmap $r.R,$r.B; $g=[Drawing.Graphics]::FromImage($b); $d=$g.GetHdc(); [N.W]::PrintWindow($h,$d,2)|Out-Null; $g.ReleaseHdc($d); $b.Save("E:\edwin.png"); "$($r.R)x$($r.B)"
