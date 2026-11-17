# Frogstation

Frontend de NES com biblioteca de capas (estilo DuckStation), em C++17 + Qt 6,
usando o núcleo **Mesen** via **libretro**, com gamepad (SDL2), remapeamento e save states.

## Requisitos (Windows)
- Visual Studio 2022 (carga "Desenvolvimento para desktop com C++") + CMake 3.21+
- Qt 6.5+ (MSVC 64-bit; módulos Widgets, Sql, Network e **Multimedia**)
- `mesen_libretro.dll` (x86_64): https://buildbot.libretro.com/nightly/windows/x86_64/latest/
- SDL2: **baixado automaticamente** pelo CMake (pacote oficial `SDL2-devel-*-VC.zip`).
  Para usar outro SDL2, passe `-DSDL2_DIR=<pasta com sdl2-config.cmake>`.

## Compilar
```bat
cd /d D:\Git\Frogstation
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:\Qt\6.12.0\msvc2022_64
cmake --build build --config Release
C:\Qt\6.12.0\msvc2022_64\bin\windeployqt.exe build\Release\Frogstation.exe
```
A `SDL2.dll` é copiada automaticamente para `build\Release\`.
Coloque o `mesen_libretro.dll` em `build\Release\cores\`.

## Uso
1. "Adicionar pasta de ROMs…" -> varre `*.nes` e identifica pelo CRC32 (DAT No-Intro baixado na 1ª vez).
2. As capas vêm do libretro-thumbnails e ficam em cache em `%APPDATA%\Frogstation\Frogstation\covers`.
3. Duplo clique abre o jogo. Clique direito na janela do jogo abre o menu (estados, slot, controles).

### Controles
Abra **Controles…** (barra de ferramentas ou menu do jogo): clique na célula de teclado ou de controle
e pressione a tecla/botão desejado. `Delete` limpa, `Esc` cancela. Vale para os botões do NES e para os atalhos.
O analógico esquerdo funciona como direcional; LT/RT podem ser mapeados. Gamepads podem ser
conectados a qualquer momento: o 1º controle é o jogador 1 (junto com o teclado) e o 2º é o jogador 2.

| Padrão (teclado) | Ação | Padrão (controle) |
|---|---|---|
| Setas | Direcional | D-Pad |
| X / Z | A / B | botão da direita / de baixo |
| Enter / Shift | Start / Select | Start / Back |
| F2 / F4 | Salvar / carregar estado | — |
| F6 / F7 | Slot anterior / próximo | LB / RB |
| F1 / F11 | Reiniciar / tela cheia | — |
| Esc | Fechar o jogo (fixo) | — |

### Save states
10 slots por jogo, com miniatura. Ficam em `%APPDATA%\Frogstation\Frogstation\states\`
(`<jogo>.stateN` e `.stateN.png`). A SRAM de bateria é gravada ao fechar, em `saves\`.
Um estado só é garantido para a mesma versão do núcleo (`mesen_libretro.dll`) que o criou.

## Estrutura
- `Core.*`         wrapper libretro (sem Qt), inclui serialização para estados
- `GameWindow.*`   vídeo, áudio, entrada, estados e menu de contexto
- `InputConfig.*`  mapeamentos de teclado/gamepad (persistidos em `settings.ini`)
- `Gamepad.*`      SDL2 GameController: hot-plug, 2 jogadores, analógico e gatilhos
- `InputDialog.*`  tela de remapeamento
- `Library.*`      SQLite + scanner com hash
- `NoIntroDat.*`   identificação por CRC32
- `CoverManager.*` download e cache de capas
- `MainWindow.*`   grade de capas, busca, zoom

## Próximos passos
Shaders/CRT e filtros, ROMs em `.zip`, opções do núcleo (palette, overscan), favoritos,
filtro por ano/gênero, auto-save ao sair, rebobinar.

## Licença

Frogstation é software livre, distribuído sob a **GNU General Public License v3.0** (veja [LICENSE](LICENSE)).

Dependências (não incluídas neste repositório):
- [Qt 6](https://www.qt.io/) — LGPL v3 (ligação dinâmica)
- [SDL2](https://libsdl.org/) — zlib (baixado pelo CMake durante o build)
- [libretro.h](https://github.com/libretro/libretro-common) — MIT (incluído em `src/`)
- Núcleo [Mesen](https://github.com/libretro/Mesen) — GPL v3 (carregado em tempo de execução; baixe separadamente)

Este projeto não inclui nem distribui ROMs. Use apenas jogos que você possui legalmente.
