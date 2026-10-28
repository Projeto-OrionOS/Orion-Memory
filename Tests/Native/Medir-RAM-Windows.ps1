param(
    [string]$Executable = (Join-Path $PSScriptRoot 'dist\win-x64\OrionMemory.exe'),
    [ValidateRange(3,100)][int]$Samples = 12
)
$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $Executable -ErrorAction Stop).Path
$qa = Join-Path $PSScriptRoot 'qa'
New-Item -ItemType Directory -Path $qa -Force | Out-Null
$fixture = Join-Path $qa ('ram-fixture-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
# --native-test-dir is an isolated and safe diagnostic mode, without cleaning,
# UAC changes, scheduled tasks, Windows startup modifications or global hotkeys.
$preferences = @{
    Language='pt-BR'; SkipUac=$false; StartWithWindows=$false;
    EnableHotkey=$false; ShowNotifications=$false; CloseToTray=$true;
    StartMinimized=$true; WindowBlur=$false; DarkTheme=$true;
    AutoIntervalEnabled=$false; AutoThresholdEnabled=$false
} | ConvertTo-Json -Compress
[IO.File]::WriteAllText((Join-Path $fixture 'settings.json'),$preferences,[Text.UTF8Encoding]::new($false))

$source = @'
using System;
using System.Runtime.InteropServices;
public static class OrionNativeMemory {
    [StructLayout(LayoutKind.Sequential)]
    public struct Counters {
        public UInt32 cb;
        public UInt32 PageFaultCount;
        public UIntPtr PeakWorkingSetSize;
        public UIntPtr WorkingSetSize;
        public UIntPtr QuotaPeakPagedPoolUsage;
        public UIntPtr QuotaPagedPoolUsage;
        public UIntPtr QuotaPeakNonPagedPoolUsage;
        public UIntPtr QuotaNonPagedPoolUsage;
        public UIntPtr PagefileUsage;
        public UIntPtr PeakPagefileUsage;
        public UIntPtr PrivateUsage;
        public UIntPtr PrivateWorkingSetSize;
        public UIntPtr SharedCommitUsage;
    }
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool K32GetProcessMemoryInfo(IntPtr process, ref Counters counters, UInt32 length);
}
'@
Add-Type -TypeDefinition $source -ErrorAction Stop
$process = $null
try {
    $process = Start-Process -FilePath $exe -ArgumentList @('--native-test-dir',('"' + $fixture + '"'),'--minimized') -PassThru -WindowStyle Hidden
    Start-Sleep -Seconds 2
    if ($process.HasExited) { throw "Orion Memory encerrou antes da medicao: $($process.ExitCode)" }
    $data = @()
    for ($i = 0; $i -lt $Samples; $i++) {
        $counter = New-Object OrionNativeMemory+Counters
        $counter.cb = [uint32][Runtime.InteropServices.Marshal]::SizeOf([OrionNativeMemory+Counters])
        if (-not [OrionNativeMemory]::K32GetProcessMemoryInfo($process.Handle,[ref]$counter,$counter.cb)) {
            $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
            throw "API de memoria indisponivel: erro Win32 $errorCode. PrivateWorkingSetSize requer suporte a PROCESS_MEMORY_COUNTERS_EX2."
        }
        if ($counter.PrivateWorkingSetSize.ToUInt64() -eq 0) { throw 'PrivateWorkingSetSize indisponivel nesta versao do Windows; nao sera reportado um falso zero.' }
        $data += [pscustomobject]@{
            PrivadaResidenteMiB = [math]::Round($counter.PrivateWorkingSetSize.ToUInt64()/1MB,3)
            ResidenteTotalMiB = [math]::Round($counter.WorkingSetSize.ToUInt64()/1MB,3)
            PrivadaComprometidaMiB = [math]::Round($counter.PrivateUsage.ToUInt64()/1MB,3)
        }
        Start-Sleep -Milliseconds 500
    }
    $private = @($data | ForEach-Object {$_.PrivadaResidenteMiB} | Sort-Object)
    $middle = [int][math]::Floor($private.Count/2)
    $median = if($private.Count % 2) { $private[$middle] } else { ($private[$middle-1]+$private[$middle])/2 }
    $result = [pscustomobject]@{
        Cenario='Bandeja - processo isolado sem privilegiar';
        Indicador='PROCESS_MEMORY_COUNTERS_EX2.PrivateWorkingSetSize';
        MedianaPrivadaResidenteMiB=[math]::Round($median,3);
        MetaReferenciaMiB='0,858 a 0,954 (900 kB a 1 MB decimais)';
        Amostras=$data
    }
    $result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $qa 'ram-bandeja.json') -Encoding UTF8
    Write-Host "RAM privada residente (mediana): $([math]::Round($median,3)) MiB" -ForegroundColor Cyan
    Write-Host 'RAM total e memoria comprometida nao estao sujeitas a esse limite.'
    Write-Host "Relatorio: $(Join-Path $qa 'ram-bandeja.json')"
} finally {
    if ($process -and -not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    if ($process) { $process.Dispose() }
    Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue
}
