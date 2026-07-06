param(
    [string]$OutputPath = "src\res\p_tool.ico"
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

$sizes = @(16, 20, 24, 32, 40, 48, 64, 128, 256)
$background = [System.Drawing.Color]::FromArgb(255, 0, 92, 184)
$foreground = [System.Drawing.Color]::White

function New-IconPngBytes([int]$Size) {
    $bitmap = New-Object System.Drawing.Bitmap $Size, $Size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $stream = New-Object System.IO.MemoryStream
    try {
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
        $graphics.Clear($background)

        $fontSize = [Math]::Round($Size * 0.72)
        $font = New-Object System.Drawing.Font "Segoe UI", $fontSize, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
        $brush = New-Object System.Drawing.SolidBrush $foreground
        $format = New-Object System.Drawing.StringFormat
        try {
            $format.Alignment = [System.Drawing.StringAlignment]::Center
            $format.LineAlignment = [System.Drawing.StringAlignment]::Center
            $format.FormatFlags = [System.Drawing.StringFormatFlags]::NoClip
            $rect = New-Object System.Drawing.RectangleF 0, ([float](-$Size * 0.035)), $Size, $Size
            $graphics.DrawString("P", $font, $brush, $rect, $format)
        } finally {
            $format.Dispose()
            $brush.Dispose()
            $font.Dispose()
        }

        $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
        return $stream.ToArray()
    } finally {
        $stream.Dispose()
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

$entries = foreach ($size in $sizes) {
    [pscustomobject]@{
        Size = $size
        Bytes = New-IconPngBytes $size
    }
}

$outDir = Split-Path -Parent $OutputPath
if (($outDir -ne $null) -and ($outDir.Length -gt 0) -and (-not (Test-Path -LiteralPath $outDir))) {
    New-Item -ItemType Directory -Path $outDir | Out-Null
}

$file = [System.IO.File]::Open($OutputPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
$writer = New-Object System.IO.BinaryWriter $file
try {
    $writer.Write([uint16]0)
    $writer.Write([uint16]1)
    $writer.Write([uint16]$entries.Count)

    $offset = 6 + ($entries.Count * 16)
    foreach ($entry in $entries) {
        $dimensionByte = if ($entry.Size -ge 256) { 0 } else { $entry.Size }
        $writer.Write([byte]$dimensionByte)
        $writer.Write([byte]$dimensionByte)
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        $writer.Write([uint16]1)
        $writer.Write([uint16]32)
        $writer.Write([uint32]$entry.Bytes.Length)
        $writer.Write([uint32]$offset)
        $offset += $entry.Bytes.Length
    }

    foreach ($entry in $entries) {
        $writer.Write([byte[]]$entry.Bytes)
    }
} finally {
    $writer.Dispose()
    $file.Dispose()
}

Write-Host "Generated $OutputPath"
