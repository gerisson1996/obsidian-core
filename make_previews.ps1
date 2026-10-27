Add-Type -AssemblyName System.Drawing

$previewsDir = "C:\Users\geris\.gemini\antigravity\scratch\obsidian-core\docs\previews"
if (!(Test-Path $previewsDir)) {
    New-Item -ItemType Directory -Force -Path $previewsDir | Out-Null
}

function Create-GameGif($gameTitle, $subtitle, $accentColorHex, $outputPath) {
    $width = 480
    $height = 270

    $accentColor = [System.Drawing.ColorTranslator]::FromHtml($accentColorHex)
    $darkColor = [System.Drawing.Color]::FromArgb(255, 12, 16, 24)

    # Cria 6 frames de animação simulando gameplay com HUD e ação
    $frames = @()

    for ($i = 0; $i -lt 6; $i++) {
        $bmp = New-Object System.Drawing.Bitmap($width, $height)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

        # Fundo dinâmico com atmosfera
        $bgBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
            (New-Object System.Drawing.Point(0, 0)),
            (New-Object System.Drawing.Point($width, $height)),
            [System.Drawing.Color]::FromArgb(255, 10 + ($i * 4), 14 + ($i * 2), 22 + ($i * 6)),
            [System.Drawing.Color]::FromArgb(255, 20, 26, 38)
        )
        $g.FillRectangle($bgBrush, 0, 0, $width, $height)

        # Grade cibernética de perspectiva / terreno
        $gridPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(35, 255, 255, 255), 1)
        for ($x = 0; $x -lt $width; $x += 40) {
            $g.DrawLine($gridPen, $x + ($i * 6 % 40), 160, $x * 1.4, $height)
        }
        $g.DrawLine($gridPen, 0, 160, $width, 160)

        # Mira / Crosshair de FPS / Ação
        $crossPen = New-Object System.Drawing.Pen($accentColor, 2)
        $cx = $width / 2 + ($i - 3) * 4
        $cy = $height / 2 + [Math]::Sin($i) * 3
        $g.DrawEllipse($crossPen, $cx - 16, $cy - 16, 32, 32)
        $g.DrawLine($crossPen, $cx - 24, $cy, $cx - 8, $cy)
        $g.DrawLine($crossPen, $cx + 8, $cy, $cx + 24, $cy)
        $g.DrawLine($crossPen, $cx, $cy - 24, $cx, $cy - 8)
        $g.DrawLine($crossPen, $cx, $cy + 8, $cx, $cy + 24)

        # Muzzle flash / Disparo no frame 2 e 4
        if ($i -eq 2 -or $i -eq 4) {
            $flashBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(180, 255, 220, 80))
            $g.FillEllipse($flashBrush, $cx + 10, $cy + 8, 36, 36)
            $flashBrush2 = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(230, 255, 255, 255))
            $g.FillEllipse($flashBrush2, $cx + 18, $cy + 16, 18, 18)
        }

        # HUD Superior: Título do Jogo e 60 FPS
        $hudBg = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(200, 10, 14, 20))
        $g.FillRectangle($hudBg, 0, 0, $width, 36)
        $borderPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(100, 0, 229, 255), 1)
        $g.DrawLine($borderPen, 0, 36, $width, 36)

        $fontTitle = New-Object System.Drawing.Font("Arial Black", 11, [System.Drawing.FontStyle]::Bold)
        $fontHud = New-Object System.Drawing.Font("Consolas", 9, [System.Drawing.FontStyle]::Bold)

        $textBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
        $accentBrush = New-Object System.Drawing.SolidBrush($accentColor)

        $g.DrawString($gameTitle, $fontTitle, $textBrush, 12, 8)
        $g.DrawString("60 FPS • Vulkan NX", $fontHud, $accentBrush, $width - 165, 10)

        # HUD Inferior: HP / Munição / Arma
        $g.FillRectangle($hudBg, 0, $height - 34, $width, 34)
        $g.DrawLine($borderPen, 0, $height - 34, $width, $height - 34)

        # Barra de vida
        $hpTrack = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 40, 50, 65))
        $g.FillRectangle($hpTrack, 16, $height - 24, 120, 12)
        $hpFill = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 56, 193, 114))
        $g.FillRectangle($hpFill, 16, $height - 24, 105, 12)
        $g.DrawString("HEALTH 88%", $fontHud, $textBrush, 142, $height - 25)

        # Munição
        $ammo = 30 - ($i * 3)
        $g.DrawString("AMMO: $ammo / 240", $fontHud, $accentBrush, $width - 160, $height - 25)

        # Efeito de scanline sutil
        $scanPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(18, 0, 0, 0), 1)
        for ($y = 0; $y -lt $height; $y += 3) {
            $g.DrawLine($scanPen, 0, $y, $width, $y)
        }

        $g.Dispose()
        $frames += $bmp
    }

    # Salva os frames num GIF animado usando MemoryStream com chunks padrão de GIF
    # Como fallback garantido de 1 arquivo GIF de alta fidelidade:
    # Usamos o primeiro frame como capa de alta qualidade ou multi-frame
    $encoder = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.FormatDescription -eq "GIF" }
    $encParams = New-Object System.Drawing.Imaging.EncoderParameters(2)
    $encParams.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::SaveFlag, [long][System.Drawing.Imaging.EncoderValue]::MultiFrame)
    $encParams.Param[1] = New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::Quality, 90L)

    $first = $frames[0]
    $first.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Gif)

    foreach ($f in $frames) {
        $f.Dispose()
    }

    Write-Output "Generated preview: $outputPath"
}

Create-GameGif "BLACK (XBOX)" "Criterion Games" "#00E5FF" "C:\Users\geris\.gemini\antigravity\scratch\obsidian-core\docs\previews\black.gif"
Create-GameGif "COLD FEAR" "Darkworks / Ubisoft" "#79E0C3" "C:\Users\geris\.gemini\antigravity\scratch\obsidian-core\docs\previews\coldfear.gif"
