# Keeps a test run's game window off the visible desktop (ED_RUN_AWAY=1 in run.sh / jtg.sh), so the user keeps their
# desktop while tests play. Not minimized: a minimized window stops drawing and shots.ps1's pictures (PrintWindow) would be
# black. The window is moved past the right edge of all monitors without being activated, and whatever window had the
# focus before the game opened gets it back. Watched until the game closes, in case the game moves its window again.
param([string]$proc = "EternalDarkness")
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class Away {
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern int GetSystemMetrics(int i);
  public struct RECT { public int L, T, R, B; }
}
'@
$before = [Away]::GetForegroundWindow()
$right = [Away]::GetSystemMetrics(76) + [Away]::GetSystemMetrics(78)  # SM_XVIRTUALSCREEN + SM_CXVIRTUALSCREEN
$deadline = (Get-Date).AddSeconds(60)
$moved = $false
while ($true) {
    $p = Get-Process -Name $proc -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
    if (-not $p) {
        if ($moved -or (Get-Date) -gt $deadline) { break }
        Start-Sleep -Milliseconds 100
        continue
    }
    $h = $p.MainWindowHandle
    $r = New-Object Away+RECT
    if ([Away]::GetWindowRect($h, [ref]$r) -and $r.L -lt $right) {
        # SWP_NOSIZE 0x1 | SWP_NOZORDER 0x4 | SWP_NOACTIVATE 0x10
        [void][Away]::SetWindowPos($h, [IntPtr]::Zero, $right + 64, 0, 0, 0, 0x15)
        if ($before -ne [IntPtr]::Zero -and $before -ne $h) { [void][Away]::SetForegroundWindow($before) }
        $moved = $true
    }
    Start-Sleep -Milliseconds 500
}
