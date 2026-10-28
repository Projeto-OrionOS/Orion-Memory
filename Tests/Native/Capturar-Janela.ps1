# Captura local para os testes de interface do Orion Memory.
# Requer Windows com PowerShell e System.Drawing.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][Int64]$WindowHandle,
    [Parameter(Mandatory=$true)][string]$Output
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class OrionWindowRect {
    [StructLayout(LayoutKind.Sequential)]
    public struct Bounds { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool GetWindowRect(IntPtr handle, out Bounds bounds);
}
'@
$bounds = New-Object OrionWindowRect+Bounds
if (-not [OrionWindowRect]::GetWindowRect([IntPtr]$WindowHandle, [ref]$bounds)) {
    throw 'A janela de teste nao foi encontrada.'
}
$width = $bounds.Right - $bounds.Left
$height = $bounds.Bottom - $bounds.Top
if ($width -lt 1 -or $height -lt 1 -or $width -gt 10000 -or $height -gt 10000) {
    throw 'Dimensoes da janela de teste invalidas.'
}
$fullPath = [IO.Path]::GetFullPath($Output)
$directory = [IO.Path]::GetDirectoryName($fullPath)
if (-not [IO.Directory]::Exists($directory)) { [IO.Directory]::CreateDirectory($directory) | Out-Null }
$bitmap = New-Object System.Drawing.Bitmap($width, $height)
$graphics = $null
try {
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($bounds.Left, $bounds.Top, 0, 0, $bitmap.Size)
    $bitmap.Save($fullPath, [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    if ($null -ne $graphics) { $graphics.Dispose() }
    $bitmap.Dispose()
}
Write-Output $fullPath
