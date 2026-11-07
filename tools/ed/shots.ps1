# Pictures of the game's own window as a run goes (PrintWindow, so it never catches another window on top), each named by
# the wall clock the game's recording also keeps, so `ed see` and `ed sheet` find the picture of a frame. It also writes how
# much each half changed since the last picture, which is how `ed check` sees a half standing still.
param([string]$dir, [int]$everyMs = 250, [int]$width = 960, [string]$proc = "EternalDarkness")
New-Item -ItemType Directory -Force $dir | Out-Null
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.IO;
using System.Drawing;
using System.Drawing.Imaging;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
public static class Shots {
  [DllImport("user32.dll")] static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr h, IntPtr d, uint f);
  [DllImport("user32.dll")] static extern bool IsWindow(IntPtr h);
  public struct RECT { public int L, T, R, B; }
  static byte[] Gray(Bitmap s, int w, int h) {
    var d = s.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
    var row = new byte[d.Stride]; var o = new byte[w * h];
    for (int y = 0; y < h; y++) {
      Marshal.Copy(d.Scan0 + y * d.Stride, row, 0, d.Stride);
      for (int x = 0; x < w; x++) o[y * w + x] = (byte)((row[x*4] + row[x*4+1] + row[x*4+2]) / 3);
    }
    s.UnlockBits(d);
    return o;
  }
  public static void Run(Process p, string dir, int every, int width) {
    SetProcessDPIAware();
    var log = new StreamWriter(Path.Combine(dir, "shots.csv")) { AutoFlush = true };
    log.WriteLine("ms,file,left,right");
    var clock = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc);
    byte[] was = null;
    int n = 0;
    var jpg = Array.Find(ImageCodecInfo.GetImageEncoders(), e => e.MimeType == "image/jpeg");
    var q = new EncoderParameters(1); q.Param[0] = new EncoderParameter(System.Drawing.Imaging.Encoder.Quality, 70L);
    while (!p.HasExited) {
      p.Refresh();
      var h = p.MainWindowHandle;
      RECT r;
      if (h == IntPtr.Zero || !GetClientRect(h, out r) || r.R < 64 || r.B < 64) { Thread.Sleep(200); continue; }
      using (var big = new Bitmap(r.R, r.B)) {
        using (var g = Graphics.FromImage(big)) { var dc = g.GetHdc(); PrintWindow(h, dc, 3); g.ReleaseHdc(dc); }
        long ms = (long)(DateTime.UtcNow - clock).TotalMilliseconds;
        int w = width, ht = Math.Max(1, r.B * width / r.R);
        using (var small = new Bitmap(big, w, ht)) {
          string name = (n++).ToString("D6") + ".jpg";
          small.Save(Path.Combine(dir, name), jpg, q);
          int gw = 96, gh = Math.Max(1, r.B * 96 / r.R);
          using (var tiny = new Bitmap(big, gw, gh)) {
            var now = Gray(tiny, gw, gh);
            double left = 99, right = 99;
            if (was != null && was.Length == now.Length) {
              long a = 0, b = 0;
              for (int y = 0; y < gh; y++) for (int x = 0; x < gw; x++) {
                int d = Math.Abs(now[y*gw+x] - was[y*gw+x]);
                if (x < gw / 2) a += d; else b += d;
              }
              left = a / (double)(gh * gw / 2); right = b / (double)(gh * gw / 2);
            }
            was = now;
            log.WriteLine(ms + "," + name + "," + left.ToString("F3", System.Globalization.CultureInfo.InvariantCulture) + "," + right.ToString("F3", System.Globalization.CultureInfo.InvariantCulture));
          }
        }
      }
      Thread.Sleep(every);
    }
    log.Close();
  }
}
'@
for ($i = 0; $i -lt 200; $i++) {
    $p = Get-Process $proc -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($p) { break }
    Start-Sleep -Milliseconds 200
}
if ($p) { [Shots]::Run($p, $dir, $everyMs, $width) }
