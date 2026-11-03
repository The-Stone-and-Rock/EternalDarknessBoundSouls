param([int]$seconds = 40, [string]$out = "burst-$(Get-Date -Format yyyyMMdd-HHmmss)")
# Watches the game's window without touching it: small pictures as fast as it can, with each half's brightness.
# They go in Screenshots for Claude\, in a folder with the last part of -out as its name.
$out = Join-Path ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\Screenshots for Claude'))) (Split-Path $out -Leaf)
New-Item -ItemType Directory -Force $out | Out-Null
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
using System.Diagnostics;
using System.Runtime.InteropServices;
public static class Burst {
  [DllImport("user32.dll")] static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("dwmapi.dll")] static extern int DwmGetWindowAttribute(IntPtr h, int a, out RECT r, int s);
  public struct RECT { public int L, T, R, B; }
  public static string Run(IntPtr h, int seconds, string dir) {
    SetProcessDPIAware();
    RECT r;
    if (DwmGetWindowAttribute(h, 9, out r, Marshal.SizeOf(typeof(RECT))) != 0) GetWindowRect(h, out r);
    int w = r.R - r.L, ht = r.B - r.T;
    var big = new Bitmap(w, ht, PixelFormat.Format32bppArgb);
    var gb = Graphics.FromImage(big);
    int sw = 640, sh = Math.Max(1, ht * 640 / w);
    var small = new Bitmap(sw, sh, PixelFormat.Format32bppArgb);
    var gs = Graphics.FromImage(small);
    gs.InterpolationMode = InterpolationMode.Bilinear;
    var log = new StreamWriter(Path.Combine(dir, "frames.csv"));
    log.WriteLine("i,ms,left,right");
    var sw2 = Stopwatch.StartNew();
    int i = 0;
    while (sw2.ElapsedMilliseconds < seconds * 1000L) {
      gb.CopyFromScreen(r.L, r.T, 0, 0, new Size(w, ht));
      gs.DrawImage(big, 0, 0, sw, sh);
      var d = small.LockBits(new Rectangle(0, 0, sw, sh), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
      long[] sum = new long[2];
      unsafe_sum(d, sw, sh, sum);
      small.UnlockBits(d);
      log.WriteLine(i + "," + sw2.ElapsedMilliseconds + "," + (sum[0] / (sw / 2 * sh)) + "," + (sum[1] / (sw / 2 * sh)));
      small.Save(Path.Combine(dir, i.ToString("D5") + ".jpg"), ImageFormat.Jpeg);
      i++;
    }
    log.Close();
    return w + "x" + ht + " " + i + " pictures";
  }
  static void unsafe_sum(BitmapData d, int w, int h, long[] sum) {
    byte[] row = new byte[d.Stride];
    for (int y = 0; y < h; y += 2) {
      Marshal.Copy(d.Scan0 + y * d.Stride, row, 0, d.Stride);
      for (int x = 0; x < w; x++)
        sum[x < w / 2 ? 0 : 1] += 2 * (row[x * 4] + row[x * 4 + 1] + row[x * 4 + 2]);
    }
  }
}
'@
$p = Get-Process EdTest,EternalDarkness -ErrorAction SilentlyContinue | Select-Object -First 1
[Burst]::Run($p.MainWindowHandle, $seconds, $out)
