# Orion Memory - compilacao nativa Windows, sem instalacao global de ferramentas.
# Execute este script diretamente no PowerShell ou pelo workflow do GitHub Actions.
[CmdletBinding()]
param(
    [ValidateSet('win-x64','win-arm64')][string]$Runtime = 'win-x64',
    [string]$ZigPath = $env:ORION_ZIG,
    [switch]$InstallZig,
    [switch]$NoDownload,
    [switch]$SkipTests,
    [switch]$NoArchive
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$zigVersion = '0.15.2'
$managedRoot = Join-Path $env:LOCALAPPDATA "OrionOS\BuildTools\zig-$zigVersion"

function Test-Zig([string]$Candidate) {
    if ([string]::IsNullOrWhiteSpace($Candidate)) { return $null }
    if (Test-Path -LiteralPath $Candidate -PathType Container) {
        $Candidate = Join-Path $Candidate 'zig.exe'
    }
    if (-not (Test-Path -LiteralPath $Candidate -PathType Leaf)) { return $null }
    $process = $null
    try {
        $resolved = (Resolve-Path -LiteralPath $Candidate).Path
        $process = [System.Diagnostics.Process]::new()
        $process.StartInfo.FileName = $resolved
        $process.StartInfo.Arguments = 'version'
        $process.StartInfo.UseShellExecute = $false
        $process.StartInfo.CreateNoWindow = $true
        $process.StartInfo.RedirectStandardOutput = $true
        $process.StartInfo.RedirectStandardError = $true
        if (-not $process.Start()) { return $null }
        $reported = $process.StandardOutput.ReadToEnd().Trim()
        $errors = $process.StandardError.ReadToEnd().Trim()
        if (-not $process.WaitForExit(10000)) {
            $process.Kill()
            throw 'Tempo limite ao verificar o compilador.'
        }
        if ($process.ExitCode -eq 0 -and $reported -eq $zigVersion) { return $resolved }
        Write-Verbose "Compilador rejeitado: $resolved (codigo $($process.ExitCode); versao: $reported; erro: $errors)"
    } catch {
        Write-Verbose "Nao foi possivel validar o compilador: $($_.Exception.Message)"
    } finally {
        if ($process) { $process.Dispose() }
    }
    return $null
}

function Find-Zig {
    if (-not [string]::IsNullOrWhiteSpace($ZigPath)) {
        $candidate = Test-Zig $ZigPath
        if (-not $candidate) {
            throw "O caminho escolhido nao contem Zig ${zigVersion}: ${ZigPath}. Remova -ZigPath para ativar a deteccao automatica."
        }
        return $candidate
    }
    $possible = @(
        (Join-Path $managedRoot 'zig.exe'),
        (Join-Path $projectRoot 'tools\zig\zig.exe'),
        'C:\Ferramentas\zig\zig.exe',
        'C:\zig\zig.exe',
        'C:\Program Files\Zig\zig.exe'
    )
    if ($env:USERPROFILE) {
        $possible += (Join-Path $env:USERPROFILE 'scoop\apps\zig\current\zig.exe')
        $possible += (Join-Path $env:USERPROFILE 'Downloads\zig-x86_64-windows-0.15.2\zig.exe')
        $possible += (Join-Path $env:USERPROFILE 'Downloads\zig-aarch64-windows-0.15.2\zig.exe')
    }
    if ($env:ProgramData) { $possible += (Join-Path $env:ProgramData 'chocolatey\bin\zig.exe') }
    if ($env:LOCALAPPDATA) { $possible += (Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Links\zig.exe') }
    $command = Get-Command zig.exe -ErrorAction SilentlyContinue
    if ($command) { $possible = @($command.Source) + $possible }
    foreach ($path in $possible) {
        $candidate = Test-Zig $path
        if ($candidate) { return $candidate }
    }
    return $null
}

function Install-ZigPortable {
    if ($NoDownload) {
        throw 'Zig 0.15.2 nao encontrado. Rode novamente sem -NoDownload ou informe -ZigPath "C:\caminho\zig.exe".'
    }
    if (-not $InstallZig) {
        if ($env:CI -or [Console]::IsInputRedirected) {
            throw 'Em ambiente nao interativo, use -InstallZig para autorizar o download do compilador oficial.'
        }
        Write-Host 'Zig 0.15.2 nao encontrado. Para compilar, podemos baixar uma copia portatil oficial.' -ForegroundColor Yellow
        Write-Host "Destino: $managedRoot"
        $answer = Read-Host 'Autorizar o download de Zig? [S/N]'
        if ($answer -notmatch '^(s|sim|y|yes)$') { throw 'Download nao autorizado. O programa e os arquivos originais nao foram alterados.' }
    }
    $architecture = if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64') { 'aarch64-windows' } else { 'x86_64-windows' }
    $expectedName = "zig-$architecture-$zigVersion.zip"
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
        Write-Host 'Consultando Zig em ziglang.org...' -ForegroundColor Cyan
        $index = Invoke-RestMethod -Uri 'https://ziglang.org/download/index.json' -TimeoutSec 45
        $release = $index.PSObject.Properties[$zigVersion].Value.PSObject.Properties[$architecture].Value
        if (-not $release) { throw 'Versao ou arquitetura nao publicada no indice oficial.' }
        $url = [Uri][string]$release.tarball
        $checksum = ([string]$release.shasum).ToLowerInvariant()
        if ($url.Scheme -ne 'https' -or $url.Host -ne 'ziglang.org' -or
            -not $url.AbsolutePath.EndsWith("/$expectedName", [StringComparison]::Ordinal) -or
            $checksum -notmatch '^[a-f0-9]{64}$') { throw 'Metadados de download nao passaram pela validacao.' }
    } catch {
        throw "Falha ao consultar o download oficial: $($_.Exception.Message). Verifique a conexao ou informe -ZigPath."
    }

    $work = Join-Path $env:TEMP ('OrionMemoryZig-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $work -Force | Out-Null
    try {
        $archive = Join-Path $work $expectedName
        Write-Host 'Baixando compilador portatil; nenhuma configuracao do Windows sera modificada...' -ForegroundColor Cyan
        Invoke-WebRequest -Uri $url.AbsoluteUri -OutFile $archive -UseBasicParsing -TimeoutSec 900
        $actual = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -ne $checksum) { throw 'SHA-256 invalido: arquivo rejeitado antes de extrair.' }
        $unpacked = Join-Path $work 'unpacked'
        Expand-Archive -LiteralPath $archive -DestinationPath $unpacked
        $binary = Get-ChildItem -LiteralPath $unpacked -Recurse -File -Filter 'zig.exe' | Select-Object -First 1
        if (-not $binary -or -not (Test-Zig $binary.FullName)) { throw 'O pacote nao contem Zig 0.15.2 valido.' }
        New-Item -ItemType Directory -Path (Split-Path -Parent $managedRoot) -Force | Out-Null
        if (Test-Path -LiteralPath $managedRoot) { Remove-Item -LiteralPath $managedRoot -Recurse -Force }
        Move-Item -LiteralPath $binary.Directory.FullName -Destination $managedRoot
        $installed = Test-Zig (Join-Path $managedRoot 'zig.exe')
        if (-not $installed) { throw 'Falha na validacao do Zig apos extracao.' }
        return $installed
    } finally {
        if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue }
    }
}

Write-Host ''
Write-Host 'Orion Memory v1.0.0 - compilacao nativa Windows' -ForegroundColor Cyan
$zig = Find-Zig
if (-not $zig) { $zig = Install-ZigPortable }
Write-Host "Zig validado: $zig"

$destination = Join-Path $projectRoot "dist\$Runtime"
New-Item -ItemType Directory -Path $destination -Force | Out-Null
$temporaryExe = Join-Path $destination 'OrionMemory.build.exe'
$targetExe = Join-Path $destination 'OrionMemory.exe'
$target = if ($Runtime -eq 'win-arm64') { 'aarch64-windows-gnu' } else { 'x86_64-windows-gnu' }
if (Test-Path -LiteralPath $temporaryExe) { Remove-Item -LiteralPath $temporaryExe -Force }
Push-Location (Join-Path $projectRoot 'NativeHost')
try {
    Write-Host "Compilando para $Runtime..." -ForegroundColor Cyan
    & $zig cc -target $target -O2 -s -municode host.c host.rc '-Wl,--subsystem,windows' `
        -o $temporaryExe -luser32 -lshell32 -lgdi32 -ladvapi32 -lversion `
        -ldwmapi -luxtheme -lpsapi -lcomdlg32 -lcomctl32
    $compileExit = $LASTEXITCODE
} finally { Pop-Location }
if ($compileExit -ne 0 -or -not (Test-Path -LiteralPath $temporaryExe -PathType Leaf)) {
    throw "Compilacao falhou (codigo $compileExit). A versao anterior do .exe foi preservada."
}

if (-not $SkipTests) {
    Write-Host 'Executando testes internos sem limpezas reais...' -ForegroundColor Cyan
    $qa = Join-Path $projectRoot 'qa'
    New-Item -ItemType Directory -Path $qa -Force | Out-Null
    $outLog = Join-Path $qa 'native-selftest.txt'
    $errLog = Join-Path $qa 'native-selftest.err'
    $run = Start-Process -FilePath $temporaryExe -ArgumentList '--self-test' `
        -PassThru -Wait -WindowStyle Hidden -RedirectStandardOutput $outLog -RedirectStandardError $errLog
    if (Test-Path -LiteralPath $outLog) { Get-Content -LiteralPath $outLog }
    if ($run.ExitCode -ne 0) {
        Write-Warning "Testes falharam (codigo $($run.ExitCode)); consulte $outLog e $errLog"
        Remove-Item -LiteralPath $temporaryExe -Force
        throw 'O executavel nao foi publicado porque os testes internos falharam.'
    }
}

# Nunca distribuir um build parcial; mover apenas depois dos testes.
try { Move-Item -LiteralPath $temporaryExe -Destination $targetExe -Force }
catch { throw 'Feche o Orion Memory antigo no menu da bandeja > Sair e compile novamente. O Windows pode bloquear a substituicao de um .exe em uso.' }

$localeFolder = Join-Path $destination 'Languages'
New-Item -ItemType Directory -Path $localeFolder -Force | Out-Null
Copy-Item -Path (Join-Path $projectRoot 'Languages\*.json') -Destination $localeFolder -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'Languages.json') -Destination $destination -Force
$docs = @('README.md')
foreach ($doc in $docs) {
    $source = Join-Path $projectRoot $doc
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $destination -Force }
}
$binaryHash = (Get-FileHash -LiteralPath $targetExe -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $destination 'SHA256.txt'), "$binaryHash  OrionMemory.exe`r`n", [Text.UTF8Encoding]::new($false))
$localeCount = @(Get-ChildItem -LiteralPath $localeFolder -File -Filter '*.json').Count
if ($localeCount -ne 30) { throw "Foram encontrados $localeCount idiomas; esperados: 30. O ZIP nao sera criado." }

Write-Host "Executavel pronto: $targetExe" -ForegroundColor Green
if (-not $NoArchive) {
    $package = Join-Path (Join-Path $projectRoot 'dist') "OrionMemory_v1.0.0_$Runtime.zip"
    $staging = Join-Path $env:TEMP ('OrionMemoryPackage-' + [guid]::NewGuid().ToString('N'))
    $appDirectory = Join-Path $staging 'OrionMemory'
    New-Item -ItemType Directory -Path $appDirectory -Force | Out-Null
    try {
        Copy-Item -LiteralPath (Join-Path $destination 'OrionMemory.exe') -Destination $appDirectory
        Copy-Item -LiteralPath (Join-Path $destination 'SHA256.txt') -Destination $appDirectory
        Copy-Item -LiteralPath (Join-Path $destination 'Languages.json') -Destination $appDirectory
        Copy-Item -LiteralPath $localeFolder -Destination $appDirectory -Recurse
        if (Test-Path -LiteralPath (Join-Path $destination 'README.md')) {
            Copy-Item -LiteralPath (Join-Path $destination 'README.md') -Destination $appDirectory
        }
        Compress-Archive -LiteralPath $appDirectory -DestinationPath $package -CompressionLevel Optimal -Force
    } finally {
        Remove-Item -LiteralPath $staging -Recurse -Force -ErrorAction SilentlyContinue
    }
    Write-Host "ZIP pronto para usar: $package" -ForegroundColor Green
}
Write-Host 'Finalizado. O Zig permanece isolado no perfil do usuario e pode ser reutilizado.'
