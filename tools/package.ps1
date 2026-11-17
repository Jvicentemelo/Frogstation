<#
.SYNOPSIS
  Gera o pacote portátil do Frogstation (.zip): descompacte em qualquer pasta e execute o Frogstation.exe.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\package.ps1
  powershell -ExecutionPolicy Bypass -File tools\package.ps1 -QtDir C:\Qt\6.12.0\msvc2022_64 -Version 0.2.0
#>
param(
    [string]$QtDir = "",        # padrão: lido de build\CMakeCache.txt (CMAKE_PREFIX_PATH)
    [string]$BuildDir = "build",
    [string]$Version = "",      # padrão: git describe, ou 0.1.0
    [string]$CorePath = "",     # mesen_libretro.dll a incluir (padrão: build\Release\cores\, senão baixa)
    [switch]$NoCore             # não inclui o núcleo (o usuário coloca em cores\)
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# --- Qt ---------------------------------------------------------------------
if (-not $QtDir) {
    $cache = Join-Path $BuildDir "CMakeCache.txt"
    if (Test-Path $cache) {
        $m = Select-String -Path $cache -Pattern '^CMAKE_PREFIX_PATH:[A-Z]+=(.+)$' | Select-Object -First 1
        if ($m) { $QtDir = $m.Matches[0].Groups[1].Value.Split(';')[0] }
    }
}
$deployqt = if ($QtDir) { Join-Path $QtDir "bin\windeployqt.exe" } else { "" }
if (-not $deployqt -or -not (Test-Path $deployqt)) {
    throw "Qt não encontrado. Informe: -QtDir C:\Qt\6.x.x\msvc2022_64"
}

# --- versão -----------------------------------------------------------------
if (-not $Version) {
    $Version = "0.1.0"
    try {
        $d = (git describe --tags --always 2>$null)
        if ($LASTEXITCODE -eq 0 -and $d) { $Version = "$d".Trim() }
    } catch { }
}

# --- build ------------------------------------------------------------------
Write-Host "==> Compilando (Release)"
cmake --build $BuildDir --config Release
if ($LASTEXITCODE -ne 0) { throw "Falha ao compilar." }
$exe = Join-Path $BuildDir "Release\Frogstation.exe"
$sdl = Join-Path $BuildDir "Release\SDL2.dll"
if (-not (Test-Path $exe)) { throw "Não encontrei $exe" }
if (-not (Test-Path $sdl)) { throw "Não encontrei $sdl" }

# --- monta a pasta ----------------------------------------------------------
$dist = Join-Path $root "dist"
$pkg  = Join-Path $dist "Frogstation"
if (Test-Path $pkg) { Remove-Item $pkg -Recurse -Force }
New-Item -ItemType Directory -Path $pkg | Out-Null
New-Item -ItemType Directory -Path (Join-Path $pkg "cores") | Out-Null
Copy-Item $exe $pkg
Copy-Item $sdl $pkg

Write-Host "==> Copiando as DLLs do Qt (windeployqt)"
& $deployqt --release --no-translations (Join-Path $pkg "Frogstation.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt falhou." }

# remove o que o programa não usa (reduz o tamanho)
foreach ($f in "opengl32sw.dll", "D3Dcompiler_47.dll", "dxcompiler.dll", "dxil.dll") {
    Remove-Item (Join-Path $pkg $f) -ErrorAction SilentlyContinue
}
$sqlDir = Join-Path $pkg "sqldrivers"
if (Test-Path $sqlDir) {
    Get-ChildItem $sqlDir -Filter *.dll | Where-Object { $_.Name -notlike "qsqlite*" } | Remove-Item
}

# --- núcleo Mesen -----------------------------------------------------------
$coreDest = Join-Path $pkg "cores"
if (-not $NoCore) {
    if (-not $CorePath) { $CorePath = Join-Path $BuildDir "Release\cores\mesen_libretro.dll" }
    if (Test-Path $CorePath) {
        Write-Host "==> Incluindo o núcleo: $CorePath"
        Copy-Item $CorePath $coreDest
    } else {
        Write-Host "==> Baixando mesen_libretro.dll"
        $tmp = Join-Path $env:TEMP "mesen_libretro.dll.zip"
        Invoke-WebRequest -UseBasicParsing -OutFile $tmp `
            "https://buildbot.libretro.com/nightly/windows/x86_64/latest/mesen_libretro.dll.zip"
        Expand-Archive $tmp -DestinationPath $coreDest -Force
        Remove-Item $tmp
    }
    if (-not (Test-Path (Join-Path $coreDest "mesen_libretro.dll"))) {
        throw "mesen_libretro.dll não foi incluído. Use -CorePath <arquivo.dll> ou -NoCore."
    }
}

# --- arquivos de texto ------------------------------------------------------
Copy-Item (Join-Path $root "LICENSE") $pkg

# a presença deste arquivo ativa o modo portátil (dados em .\data, ao lado do .exe)
Set-Content -Path (Join-Path $pkg "portable.txt") -Encoding UTF8 -Value @'
Modo portátil do Frogstation: biblioteca, capas, estados salvos e configurações ficam na
pasta "data", ao lado do Frogstation.exe. Apague este arquivo para usar %APPDATA% em vez disso.
'@

Set-Content -Path (Join-Path $pkg "LEIA-ME.txt") -Encoding UTF8 -Value @"
Frogstation $Version - frontend de NES com capas

COMO USAR
1. Descompacte esta pasta em qualquer lugar com permissão de escrita (não use Program Files).
2. Execute Frogstation.exe.
3. Clique em "Adicionar pasta de ROMs..." e escolha a pasta com seus arquivos .nes.
4. Duplo clique numa capa para jogar. Clique direito na janela do jogo abre o menu.

Se o Windows exibir o aviso do SmartScreen, clique em "Mais informações" e "Executar assim mesmo".

CONTROLES (padrão; podem ser alterados em "Controles...")
Setas = direcional | X = A | Z = B | Enter = Start | Shift = Select
F2 / F4 = salvar / carregar estado | F6 / F7 = trocar de slot
F1 = reiniciar | F11 = tela cheia | Esc = fechar o jogo
Gamepads são detectados automaticamente (1o = jogador 1, 2o = jogador 2).

Este programa não inclui ROMs. Use apenas jogos que você possui legalmente.
Código-fonte e licença: veja LICENSE (GPL-3.0).
"@

Set-Content -Path (Join-Path $pkg "THIRD-PARTY-NOTICES.txt") -Encoding UTF8 -Value @'
Frogstation é distribuído sob a GNU General Public License v3.0 (arquivo LICENSE).

Componentes de terceiros incluídos neste pacote:

- Qt 6 (DLLs na pasta do programa) - LGPL v3. Código-fonte e licença: https://www.qt.io/download-open-source
  As DLLs do Qt são ligadas dinamicamente e podem ser substituídas por outras versões compatíveis.
- SDL2 (SDL2.dll) - licença zlib. https://libsdl.org/
- Mesen (cores\mesen_libretro.dll) - GPL v3. Código-fonte: https://github.com/libretro/Mesen
- libretro.h - MIT. https://github.com/libretro/libretro-common
'@

# --- zip --------------------------------------------------------------------
$zipPath = Join-Path $dist "Frogstation-$Version-win64.zip"
if (Test-Path $zipPath) { Remove-Item $zipPath }
Write-Host "==> Criando o .zip"
Compress-Archive -Path $pkg -DestinationPath $zipPath

$mb = [math]::Round((Get-Item $zipPath).Length / 1MB, 1)
Write-Host ""
Write-Host "Pronto: $zipPath ($mb MB)"
Write-Host "Teste: descompacte em outra pasta e execute Frogstation.exe."
