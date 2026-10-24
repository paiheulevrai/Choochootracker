param(
    [string]$Source = "$PSScriptRoot\..\inspirations\logo pixel.jfif"
)

# The supplied pixel-art reference is already the approved locomotive artwork.
# Crop its white canvas, preserve the hard pixel edges, and make every launcher
# target from that one source.
Add-Type -AssemblyName System.Drawing

$root = (Resolve-Path "$PSScriptRoot\..").Path
$sourceImage = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Source).Path)
$crop = [System.Drawing.Rectangle]::new(300, 130, 340, 460)
$train = [System.Drawing.Bitmap]::new($crop.Width, $crop.Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$background = [bool[,]]::new($crop.Width, $crop.Height)
$pending = [System.Collections.Generic.Queue[System.Drawing.Point]]::new()
for ($x = 0; $x -lt $crop.Width; $x++) {
    $pending.Enqueue([System.Drawing.Point]::new($x, 0))
    $pending.Enqueue([System.Drawing.Point]::new($x, $crop.Height - 1))
}
for ($y = 1; $y -lt $crop.Height - 1; $y++) {
    $pending.Enqueue([System.Drawing.Point]::new(0, $y))
    $pending.Enqueue([System.Drawing.Point]::new($crop.Width - 1, $y))
}
while ($pending.Count) {
    $point = $pending.Dequeue()
    if ($background[$point.X, $point.Y]) { continue }
    $pixel = $sourceImage.GetPixel($crop.X + $point.X, $crop.Y + $point.Y)
    $spread = [Math]::Max($pixel.R, [Math]::Max($pixel.G, $pixel.B)) - [Math]::Min($pixel.R, [Math]::Min($pixel.G, $pixel.B))
    $brightness = ($pixel.R + $pixel.G + $pixel.B) / 3
    if ($brightness -lt 115 -or $spread -gt 40) { continue }
    $background[$point.X, $point.Y] = $true
    foreach ($offset in @(@(-1, 0), @(1, 0), @(0, -1), @(0, 1))) {
        $nextX = $point.X + $offset[0]; $nextY = $point.Y + $offset[1]
        if ($nextX -ge 0 -and $nextX -lt $crop.Width -and $nextY -ge 0 -and $nextY -lt $crop.Height) {
            $pending.Enqueue([System.Drawing.Point]::new($nextX, $nextY))
        }
    }
}
for ($y = 0; $y -lt $crop.Height; $y++) {
    for ($x = 0; $x -lt $crop.Width; $x++) {
        $pixel = $sourceImage.GetPixel($crop.X + $x, $crop.Y + $y)
        $spread = [Math]::Max($pixel.R, [Math]::Max($pixel.G, $pixel.B)) - [Math]::Min($pixel.R, [Math]::Min($pixel.G, $pixel.B))
        $brightness = ($pixel.R + $pixel.G + $pixel.B) / 3
        if ($background[$x, $y] -or ($brightness -ge 220 -and $spread -le 40)) {
            $train.SetPixel($x, $y, [System.Drawing.Color]::Transparent)
        } else {
            $train.SetPixel($x, $y, $pixel)
        }
    }
}

function New-IconBitmap([int]$size, [bool]$transparent = $false) {
    $bitmap = [System.Drawing.Bitmap]::new($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighSpeed
    if (!$transparent) { $graphics.Clear([System.Drawing.Color]::FromArgb(255, 5, 12, 31)) }

    $adaptive = $transparent -and $size -eq 108
    $height = [int][Math]::Round($size * $(if ($adaptive) { 0.61 } else { 0.82 }))
    $width = [int][Math]::Round($height * $crop.Width / $crop.Height * $(if ($adaptive) { 1.45 } else { 1.50 }))
    $left = [int](($size - $width) / 2)
    $top = [int](($size - $height) / 2)
    $graphics.DrawImage($train, [System.Drawing.Rectangle]::new($left, $top, $width, $height))
    $graphics.Dispose()
    return $bitmap
}

function Save-Png([string]$path, [int]$size, [bool]$transparent = $false) {
    $image = New-IconBitmap $size $transparent
    $image.Save((Join-Path $root $path), [System.Drawing.Imaging.ImageFormat]::Png)
    $image.Dispose()
}

Save-Png 'docs/play-store-assets/ChooChooTracker-play-icon-512.png' 512
Save-Png 'tracker/packaging/linux-appimage/choochootracker.png' 512
Save-Png 'tracker/platforms/android/app/src/main/res/drawable/ic_launcher_foreground.png' 108 $true
foreach ($entry in @{
    'mipmap-mdpi' = 48; 'mipmap-hdpi' = 72; 'mipmap-xhdpi' = 96
    'mipmap-xxhdpi' = 144; 'mipmap-xxxhdpi' = 192
}.GetEnumerator()) {
    Save-Png "tracker/platforms/android/app/src/main/res/$($entry.Key)/ic_launcher.png" $entry.Value
}
foreach ($entry in @{
    'icon_16x16.png' = 16; 'icon_16x16@2x.png' = 32; 'icon_32x32.png' = 32
    'icon_32x32@2x.png' = 64; 'icon_128x128.png' = 128; 'icon_128x128@2x.png' = 256
    'icon_256x256.png' = 256; 'icon_256x256@2x.png' = 512; 'icon_512x512.png' = 512
    'icon_512x512@2x.png' = 1024
}.GetEnumerator()) {
    Save-Png "tracker/packaging/macos/ChipNomad.iconset/$($entry.Key)" $entry.Value
}
$windowsImage = New-IconBitmap 256
$pngStream = [System.IO.MemoryStream]::new()
$windowsImage.Save($pngStream, [System.Drawing.Imaging.ImageFormat]::Png)
$png = $pngStream.ToArray()
$pngStream.Dispose()
$icoStream = [System.IO.File]::Create((Join-Path $root 'tracker/packaging/windows/choochootracker.ico'))
$writer = [System.IO.BinaryWriter]::new($icoStream)
$writer.Write([UInt16]0); $writer.Write([UInt16]1); $writer.Write([UInt16]1)
$writer.Write([Byte]0); $writer.Write([Byte]0); $writer.Write([Byte]0); $writer.Write([Byte]0)
$writer.Write([UInt16]1); $writer.Write([UInt16]32); $writer.Write([UInt32]$png.Length); $writer.Write([UInt32]22)
$writer.Write($png)
$writer.Dispose()
$windowsImage.Dispose()
$train.Dispose()
$sourceImage.Dispose()
