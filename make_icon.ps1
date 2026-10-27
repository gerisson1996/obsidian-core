Add-Type -AssemblyName System.Drawing

$width = 256
$height = 256
$bmp = New-Object System.Drawing.Bitmap($width, $height)
$g = [System.Drawing.Graphics]::FromImage($bmp)

$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic

# Background gradient (deep cyber obsidian #0b0f19 to #161e2e)
$bgBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    (New-Object System.Drawing.Point(0, 0)),
    (New-Object System.Drawing.Point(256, 256)),
    [System.Drawing.Color]::FromArgb(255, 11, 15, 25),
    [System.Drawing.Color]::FromArgb(255, 20, 28, 45)
)
$g.FillRectangle($bgBrush, 0, 0, $width, $height)

# Center radial glow
$pathGlow = New-Object System.Drawing.Drawing2D.GraphicsPath
$pathGlow.AddEllipse(20, 20, 216, 216)
$pbg = New-Object System.Drawing.Drawing2D.PathGradientBrush($pathGlow)
$pbg.CenterColor = [System.Drawing.Color]::FromArgb(60, 0, 229, 255)
$pbg.SurroundColors = @([System.Drawing.Color]::FromArgb(0, 11, 15, 25))
$g.FillEllipse($pbg, 20, 20, 216, 216)

# Outer rounded rectangle frame
function Get-RoundedRectPath($rect, $radius) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $radius * 2
    $p.AddArc($rect.X, $rect.Y, $d, $d, 180, 90)
    $p.AddArc($rect.Right - $d, $rect.Y, $d, $d, 270, 90)
    $p.AddArc($rect.Right - $d, $rect.Bottom - $d, $d, $d, 0, 90)
    $p.AddArc($rect.X, $rect.Bottom - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

$rectOuter = New-Object System.Drawing.Rectangle(10, 10, 236, 236)
$rectInner = New-Object System.Drawing.Rectangle(12, 12, 232, 232)

$penFrameOuter = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(70, 0, 180, 255), 4)
$penFrameInner = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(200, 0, 240, 255), 2)

$pathOuter = Get-RoundedRectPath $rectOuter 26
$pathInner = Get-RoundedRectPath $rectInner 24

$g.DrawPath($penFrameOuter, $pathOuter)
$g.DrawPath($penFrameInner, $pathInner)

# Center Joy-Con / Switch Icon
# Left Joy-Con (Neon Cyan #00e5ff)
$leftBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    (New-Object System.Drawing.Point(62, 45)),
    (New-Object System.Drawing.Point(120, 155)),
    [System.Drawing.Color]::FromArgb(255, 0, 245, 255),
    [System.Drawing.Color]::FromArgb(255, 0, 150, 235)
)

$leftRect = New-Object System.Drawing.Rectangle(64, 48, 52, 104)
$leftPath = New-Object System.Drawing.Drawing2D.GraphicsPath
$r = 24
$leftPath.AddArc($leftRect.X, $leftRect.Y, $r*2, $r*2, 180, 90)
$leftPath.AddLine($leftRect.X + $r, $leftRect.Y, $leftRect.Right, $leftRect.Y)
$leftPath.AddLine($leftRect.Right, $leftRect.Y, $leftRect.Right, $leftRect.Bottom)
$leftPath.AddLine($leftRect.Right, $leftRect.Bottom, $leftRect.X + $r, $leftRect.Bottom)
$leftPath.AddArc($leftRect.X, $leftRect.Bottom - $r*2, $r*2, $r*2, 90, 90)
$leftPath.CloseFigure()
$g.FillPath($leftBrush, $leftPath)

# Left Stick
$stickBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 12, 18, 30))
$g.FillEllipse($stickBrush, 82, 68, 18, 18)

# Left D-Pad buttons
$dpadBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 12, 18, 30))
$g.FillEllipse($dpadBrush, 87, 104, 8, 8)
$g.FillEllipse($dpadBrush, 87, 124, 8, 8)
$g.FillEllipse($dpadBrush, 77, 114, 8, 8)
$g.FillEllipse($dpadBrush, 97, 114, 8, 8)

# Right Joy-Con (Neon Red/Pink #ff1744)
$rightBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    (New-Object System.Drawing.Point(136, 45)),
    (New-Object System.Drawing.Point(194, 155)),
    [System.Drawing.Color]::FromArgb(255, 255, 45, 95),
    [System.Drawing.Color]::FromArgb(255, 220, 0, 70)
)
$rightRect = New-Object System.Drawing.Rectangle(140, 48, 52, 104)
$rightPath = New-Object System.Drawing.Drawing2D.GraphicsPath
$rightPath.AddLine($rightRect.X, $rightRect.Y, $rightRect.Right - $r, $rightRect.Y)
$rightPath.AddArc($rightRect.Right - $r*2, $rightRect.Y, $r*2, $r*2, 270, 90)
$rightPath.AddLine($rightRect.Right, $rightRect.Y + $r, $rightRect.Right, $rightRect.Bottom - $r)
$rightPath.AddArc($rightRect.Right - $r*2, $rightRect.Bottom - $r*2, $r*2, $r*2, 0, 90)
$rightPath.AddLine($rightRect.Right - $r, $rightRect.Bottom, $rightRect.X, $rightRect.Bottom)
$rightPath.CloseFigure()
$g.FillPath($rightBrush, $rightPath)

# Right ABXY Buttons
$g.FillEllipse($dpadBrush, 163, 64, 8, 8)
$g.FillEllipse($dpadBrush, 163, 84, 8, 8)
$g.FillEllipse($dpadBrush, 153, 74, 8, 8)
$g.FillEllipse($dpadBrush, 173, 74, 8, 8)

# Right Stick
$g.FillEllipse($stickBrush, 156, 110, 18, 18)

# Center divider line
$divPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(140, 255, 255, 255), 2)
$g.DrawLine($divPen, 128, 54, 128, 146)

# Title Text: "NX PORT HUB"
$fontTitle = New-Object System.Drawing.Font("Arial Black", 16, [System.Drawing.FontStyle]::Bold)
$fontSub = New-Object System.Drawing.Font("Arial", 8, [System.Drawing.FontStyle]::Bold)

$formatCenter = New-Object System.Drawing.StringFormat
$formatCenter.Alignment = [System.Drawing.StringAlignment]::Center

# Glow text
$textGlowBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(100, 0, 240, 255))
$g.DrawString("NX PORT HUB", $fontTitle, $textGlowBrush, 128, 171, $formatCenter)
$g.DrawString("NX PORT HUB", $fontTitle, $textGlowBrush, 128, 173, $formatCenter)

$textWhiteBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 255, 255, 255))
$g.DrawString("NX PORT HUB", $fontTitle, $textWhiteBrush, 128, 172, $formatCenter)

$subBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(220, 0, 229, 255))
$g.DrawString("OBSIDIAN NATIVE PORTS", $fontSub, $subBrush, 128, 204, $formatCenter)

# Save as JPEG high quality
$codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.FormatDescription -eq "JPEG" }
$encoderParams = New-Object System.Drawing.Imaging.EncoderParameters(1)
$encoderParams.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::Quality, 96L)

$targetPath = "C:\Users\geris\.gemini\antigravity\scratch\obsidian-core\app\icon.jpg"
$bmp.Save($targetPath, $codec, $encoderParams)
$g.Dispose()
$bmp.Dispose()
Write-Output "SUCCESS: Icon saved to $targetPath"
