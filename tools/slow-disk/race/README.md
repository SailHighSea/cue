# Corridas (19, 17/11): troca rápida de arquivos, cliques e teclas sem pausa

Mesmos caminhos do resto de `slow-disk` (Wine em `/home/claude/wp`, Cue em `C:\Cue`, `E:` no disco lento,
Xvfb `:9`, `slowhd.so` em `/home/claude/sim`). Vídeos de teste em `C:\m`: `c1..c4.mp4` são cores sólidas
(vermelho, verde, azul, amarelo) com um quadrado branco andando, para saber pela tela qual está tocando:

    for c in red green blue yellow; do ffmpeg -f lavfi -i "color=c=$c:s=640x360:r=30:d=60" -f lavfi -i sine=d=60 \
      -vf "drawbox=x='mod(t*80,600)':y=150:w=40:h=40:c=white:t=fill" -c:v libx264 -preset ultrafast -g 60 -c:a aac -shortest cN.mp4; done

- `sendopen.exe ms:arquivo ... ms:key:0xVK` (`send.sh`): entrega arquivos à janela do Cue como uma 2ª instância
  (WM_COPYDATA), cada um `ms` depois do anterior, com tempo exato (2ªs instâncias de verdade no Wine levam de
  0,1 a 1 s para nascer e chegam fora de ordem). `key:` posta uma tecla (0xB0 = próxima faixa).
  `x86_64-w64-mingw32-g++-posix -O2 -static -municode -o sendopen.exe sendopen.cpp`
- `fresh.sh [args]`: Cue de novo (`SLOW=1`: E: no disco lento; `WIPE=1`: sem state.json; `WAIT=s`).
- `color.sh`: título da janela e a cor no meio da imagem. `title.sh`, `open-burst.sh` (2ªs instâncias de verdade).
- Testes (PASS/FAIL): `t-slow-then-fast.sh` (um arquivo de um disco lento e, 1 s depois, um do rápido: o último
  vence), `t-open-paths.sh` (seleção do Explorer, pasta, seleção num disco lento), `t-cold-selection.sh`.
- `t-paste.sh` (Ctrl+V de um caminho em texto num disco lento; `setclip.exe` põe o texto na área de transferência),
  `t-folder-args.sh` (o Cue aberto com uma pasta), `hung.sh` + `ping.exe` (a janela responde?).
- `monkey.py`/`monkey.sh <s> <semente>` (o macaco), `soak.sh` (vários até a 1ª trava), `replay.py` (repete um registro).
- `focusdeadlock.exe <foco 0|1> <captura 0|1>`: uma janela filha de outra linha destruída com a linha da mãe bloqueada
  (como a do motor). No Wine trava sempre; no Windows, rodar para saber.
- `perf.sh <rótulo> <luz true|false> <ação> <arquivo>`: mensagens/s (msgs.sh) num estado limpo; `kinds.sh <.win> <s>`
  por tipo (8001 = avisos do motor, WM_TIMER id 2 = o poll). `sendopen.exe msg:0x112:0xF020` minimiza, `0xF120` restaura.
