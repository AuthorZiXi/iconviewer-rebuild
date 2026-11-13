<#
    Renders the IconViewer Rebuild application icon at every size we ship.
    Run from anywhere:  powershell -File tools/icon/render-icon.ps1
    Output:            tools/icon/sizes/icon_<n>.png  (input for make-ico.js)
#>
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$outDir = Join-Path $PSScriptRoot 'sizes'
if (!(Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }

function RoundRectPath([single]$x, [single]$y, [single]$w, [single]$h, [single]$r) {
  $p = New-Object System.Drawing.Drawing2D.GraphicsPath
  $d = [single]($r * 2)
  if ($d -gt $w) { $d = $w }
  if ($d -gt $h) { $d = $h }
  $p.AddArc($x, $y, $d, $d, 180, 90)
  $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
  $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
  $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
  $p.CloseFigure()
  return $p
}

function MakeIcon([int]$S, [string]$path) {
  $f = [single]$S
  $bmp = New-Object System.Drawing.Bitmap $S, $S, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.Clear([System.Drawing.Color]::Transparent)

  # rounded gradient background
  $pad = [single]($f * 0.02)
  $bw = $f - 2 * $pad
  $bgPath = RoundRectPath $pad $pad $bw $bw ([single]($f * 0.225))
  $rect = New-Object System.Drawing.RectangleF $pad, $pad, $bw, $bw
  $c1 = [System.Drawing.Color]::FromArgb(255, 104, 158, 255)
  $c2 = [System.Drawing.Color]::FromArgb(255, 38, 68, 186)
  $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, $c1, $c2, 55.0
  $g.FillPath($brush, $bgPath)

  # magnifier geometry
  $lcx = [single]($f * 0.455)
  $lcy = [single]($f * 0.455)
  $lr = [single]($f * 0.295)
  $stroke = [single]([math]::Max(1.2, $f * 0.085))

  $lensRect = New-Object System.Drawing.RectangleF ($lcx - $lr), ($lcy - $lr), (2 * $lr), (2 * $lr)
  $clipPath = New-Object System.Drawing.Drawing2D.GraphicsPath
  $clipPath.AddEllipse($lensRect)
  $g.SetClip($clipPath)
  $white = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)
  $g.FillEllipse($white, $lensRect)

  # 2x2 icon grid inside the lens
  $tile = [single]($lr * 0.62)
  $gap = [single]($lr * 0.14)
  $ox = [single]($lcx - $tile - $gap / 2)
  $oy = [single]($lcy - $tile - $gap / 2)
  $colors = @(
    [System.Drawing.Color]::FromArgb(255, 255, 92, 92),
    [System.Drawing.Color]::FromArgb(255, 255, 186, 56),
    [System.Drawing.Color]::FromArgb(255, 64, 200, 190),
    [System.Drawing.Color]::FromArgb(255, 84, 132, 240)
  )
  $trad = [single]($f * 0.022)
  for ($i = 0; $i -lt 4; $i++) {
    $col = $i % 2; $row = [math]::Floor($i / 2)
    $tx = $ox + $col * ($tile + $gap)
    $ty = $oy + $row * ($tile + $gap)
    $tp = RoundRectPath $tx $ty $tile $tile $trad
    $tb = New-Object System.Drawing.SolidBrush $colors[$i]
    $g.FillPath($tb, $tp)
    $tb.Dispose(); $tp.Dispose()
  }
  $g.ResetClip()

  # soft shadow under the magnifier (not at tiny sizes)
  if ($S -ge 32) {
    $shadow = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(60, 16, 30, 90)), ([single]($stroke * 1.15))
    $sr = New-Object System.Drawing.RectangleF (($lcx - $lr) + $f * 0.012), (($lcy - $lr) + $f * 0.012), (2 * $lr), (2 * $lr)
    $g.DrawEllipse($shadow, $sr)
    $shadow.Dispose()
  }

  # glass tint + rim
  $tint = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(26, 255, 255, 255))
  $g.FillEllipse($tint, $lensRect)
  $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::White), $stroke
  $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
  $g.DrawEllipse($pen, $lensRect)

  # handle
  $hx1 = [single]($lcx + $lr * 0.70)
  $hy1 = [single]($lcy + $lr * 0.70)
  $hx2 = [single]($f * 0.855)
  $hy2 = [single]($f * 0.855)
  $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
  $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
  $g.DrawLine($pen, $hx1, $hy1, $hx2, $hy2)

  $g.Dispose()
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  $pen.Dispose(); $tint.Dispose(); $white.Dispose(); $brush.Dispose(); $bgPath.Dispose(); $clipPath.Dispose()
}

foreach ($s in @(256,128,64,48,32,24,16)) {
  MakeIcon $s (Join-Path $outDir ("icon_" + $s + ".png"))
}
Write-Output ("rendered icon sizes into " + $outDir)
