param(
    [string]$SourceJpg = "$PSScriptRoot\assets\app_icon.jpg",
    [string]$OutRes    = "$PSScriptRoot\build\app.res"
)

Add-Type -AssemblyName System.Drawing

$src   = [System.Drawing.Image]::FromFile($SourceJpg)
$sizes = @(16, 32, 48, 256)
$pngs  = @()

foreach ($sz in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap($src, $sz, $sz)
    $ms  = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngs += ,($ms.ToArray())
    $bmp.Dispose()
    $ms.Dispose()
}
$src.Dispose()

$out = New-Object System.IO.MemoryStream
$w   = New-Object System.IO.BinaryWriter($out)

function Write-Hdr($dataSize, $typeId, $nameId, $memFlags, $langId) {
    $w.Write([uint32]$dataSize)
    $w.Write([uint32]32)
    $w.Write([uint16]0xFFFF); $w.Write([uint16]$typeId)
    $w.Write([uint16]0xFFFF); $w.Write([uint16]$nameId)
    $w.Write([uint32]0)
    $w.Write([uint16]$memFlags)
    $w.Write([uint16]$langId)
    $w.Write([uint32]0)
    $w.Write([uint32]0)
}

function Pad4($n) {
    $rem = $n % 4
    if ($rem) { for ($i = 0; $i -lt (4 - $rem); $i++) { $w.Write([byte]0) } }
}

# null resource
Write-Hdr 0 0 0 0 0

# RT_ICON entries (type 3)
for ($i = 0; $i -lt $pngs.Count; $i++) {
    Write-Hdr $pngs[$i].Length 3 ($i + 1) 0x1010 0x0409
    $w.Write($pngs[$i])
    Pad4 $pngs[$i].Length
}

# RT_GROUP_ICON (type 14)
$groupSize = 6 + $sizes.Count * 14
Write-Hdr $groupSize 14 1 0x1030 0x0409
$w.Write([uint16]0); $w.Write([uint16]1); $w.Write([uint16]$sizes.Count)
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $d = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] }
    $w.Write([byte]$d); $w.Write([byte]$d)
    $w.Write([byte]0);  $w.Write([byte]0)
    $w.Write([uint16]1); $w.Write([uint16]32)
    $w.Write([uint32]$pngs[$i].Length)
    $w.Write([uint16]($i + 1))
}
Pad4 $groupSize

$w.Flush()
[System.IO.File]::WriteAllBytes($OutRes, $out.ToArray())
Write-Host "icon.res -> $OutRes ($($out.Length) bytes, sizes: $($sizes -join ', '))"
