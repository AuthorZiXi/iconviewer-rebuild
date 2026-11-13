<#
    Renders docs/social-preview.png (1280x640), the image GitHub uses for the
    repository card / Open Graph preview.
    Run:  powershell -File tools/social/make-social-preview.ps1
    Then upload it in Settings -> Social preview.
#>
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

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

$W = 1280
$H = 640
$bmp = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

$bgRect = New-Object System.Drawing.RectangleF 0, 0, $W, $H
$bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush $bgRect, ([System.Drawing.Color]::FromArgb(255, 9, 14, 28)), ([System.Drawing.Color]::FromArgb(255, 20, 32, 62)), 60.0
$g.FillRectangle($bg, $bgRect)

$glowPath = New-Object System.Drawing.Drawing2D.GraphicsPath
$glowRect = New-Object System.Drawing.RectangleF -260, -240, 900, 900
$glowPath.AddEllipse($glowRect)
$pgb = New-Object System.Drawing.Drawing2D.PathGradientBrush $glowPath
$pgb.CenterColor = [System.Drawing.Color]::FromArgb(80, 70, 130, 255)
$pgb.SurroundColors = [System.Drawing.Color[]]@([System.Drawing.Color]::FromArgb(0, 20, 32, 62))
$pgb.CenterPoint = New-Object System.Drawing.PointF 190, 210
$g.FillPath($pgb, $glowPath)

# screenshot card
$shot = [System.Drawing.Image]::FromFile((Join-Path $root 'docs\screenshots\rebuild_main.png'))
$sw = [single]($shot.Width * 0.78)
$sh = [single]($shot.Height * 0.78)
$sx = 748
$sy = 116
$cardPath = RoundRectPath $sx $sy $sw $sh 12
for ($i = 10; $i -ge 1; $i--) {
  $sp = RoundRectPath ($sx - $i * 0.4) ($sy + $i * 1.2) ($sw + $i * 0.8) ($sh + $i * 0.8) (12 + $i * 0.4)
  $sb = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(10, 0, 0, 0))
  $g.FillPath($sb, $sp)
  $sb.Dispose()
  $sp.Dispose()
}
$g.SetClip($cardPath)
$srcRect = New-Object System.Drawing.RectangleF 7, 7, ($shot.Width - 14), ($shot.Height - 14)
$dstRect = New-Object System.Drawing.RectangleF $sx, $sy, $sw, $sh
$g.DrawImage($shot, $dstRect, $srcRect, [System.Drawing.GraphicsUnit]::Pixel)
$g.ResetClip()
$border = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(70, 255, 255, 255)), 1.5
$g.DrawPath($border, $cardPath)
$shot.Dispose()

$icon = [System.Drawing.Image]::FromFile((Join-Path $root 'docs\icon.png'))
$g.DrawImage($icon, 96, 92, 148, 148)
$icon.Dispose()

$fmt = [System.Drawing.StringFormat]::GenericTypographic
$titleFont = New-Object System.Drawing.Font 'Segoe UI', 50, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$white = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 244, 247, 255))
$g.DrawString('IconViewer Rebuild', $titleFont, $white, 96, 268, $fmt)

$accent = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 92, 143, 255))
$g.FillRectangle($accent, 98, 340, 84, 5)

$subFont = New-Object System.Drawing.Font 'Segoe UI', 24, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
$sub = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 168, 182, 208))
$g.DrawString('Every icon and device image', $subFont, $sub, 98, 372, $fmt)
$g.DrawString('inside EXE, DLL, ICO and CUR files', $subFont, $sub, 98, 404, $fmt)

$pillFont = New-Object System.Drawing.Font 'Segoe UI', 19, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
$cjkFont = New-Object System.Drawing.Font 'Microsoft YaHei UI', 19, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
$pillText = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 213, 222, 234))
$items = @(
  @{ text = 'C++ / Win32'; font = $pillFont },
  @{ text = 'Windows 7+ x64'; font = $pillFont },
  @{ text = 'MIT'; font = $pillFont },
  @{ text = ('EN / ' + [char]0x4E2D + [char]0x6587); font = $cjkFont }
)
$px = 98.0
$py = 462.0
$ph = 44.0
foreach ($it in $items) {
  $sz = $g.MeasureString($it.text, $it.font, 1000, $fmt)
  $pw = [single]($sz.Width + 40)
  $pp = RoundRectPath $px $py $pw $ph 22
  $fill = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(26, 255, 255, 255))
  $g.FillPath($fill, $pp)
  $line = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(48, 255, 255, 255)), 1
  $g.DrawPath($line, $pp)
  $g.DrawString($it.text, $it.font, $pillText, ($px + 20), ($py + 10), $fmt)
  $fill.Dispose()
  $line.Dispose()
  $pp.Dispose()
  $px += $pw + 14
}

$footFont = New-Object System.Drawing.Font 'Segoe UI', 17, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
$foot = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 108, 124, 152))
$g.DrawString('github.com/AuthorZiXi/iconviewer-rebuild   |   MIT   |   no dependencies', $footFont, $foot, 98, 572, $fmt)
$creditFont = New-Object System.Drawing.Font 'Segoe UI', 15, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
$credit = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 92, 106, 132))
$g.DrawString('Built with DSH + DeepSeek-V4.1-Flash (1017)', $creditFont, $credit, 748, 590, $fmt)

$g.Dispose()
$bmp.Save((Join-Path $root 'docs\social-preview.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output ('wrote ' + (Join-Path $root 'docs\social-preview.png'))
