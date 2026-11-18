# Varredura (17/11): o Cue de verdade no Wine, sob disco lento, disco que some e mensagens do sistema

Caminhos fixos (como o resto de `slow-disk`): Wine em `/home/claude/wp`, Cue em `C:\Cue`, disco lento `E:` ->
`/home/claude/slowhd`, Xvfb `:9`, `slowhd.so` e `ui_probe.exe` em `/home/claude/sim`.

- `go.sh <rótulo> <espera s> <medida s> [args do Cue]`: abre o Cue, mede CPU/memória/leituras (`stat.py`) e
  quanto a janela fica sem responder (`ui_probe.exe`). `SLOW=1`: com o disco lento. `ACT='...'`: ações durante
  a medida (xdotool, `echo 5000 > /tmp/slowhd.lag` = o disco passa a levar 5 s por operação,
  `touch /tmp/slowhd.gone` = o disco some). `NOPROBE=1`, `THREADS=1` (por linha de execução).
- `msgs.sh`: mensagens por segundo na linha da interface (relay do Wine), por tipo.
- `bt.sh`: onde a linha da interface está parada (winedbg; o próprio depurador congela o processo enquanto
  lê: não medir tempo junto). Símbolos: `mksym.sh` (liga um Cue.exe sem `-s`).
- `mkstate.py <state.json> <caminhos>`: estado com esses recentes. `sysmsg.cpp`: sono/volta, troca de tela,
  DPI, tema, dispositivos mandados à janela.
- Artefatos do Wine: `DestroyWindow` leva 2 s sem gerenciador de janelas; as trocas de contexto contadas pelo
  Linux incluem as idas ao wineserver (não são acordadas do Windows).

## 18 (17/11): o que a linha da interface pede ao disco
- `fsmap.sh <rótulo> <s> [ACT] -- <args do Cue>`: roda o Cue com símbolos (`mksym.sh`; outro: `EXE=`, `SYMS=`) sob o
  relay do Wine só das funções de disco (atributos, abrir, listar, mover, criar pasta, `_wfopen`...) e lista as
  chamadas feitas pela linha da interface (a que despacha mensagens), com quem chamou e um caminho de exemplo.
  `fsreport.py <relay> all` inclui a pasta de dados do Cue (AppData).
- `key.sh tecla... | sleep:N | click:x,y | rclick:x,y` (cliques em coordenadas da tela) e `shot.sh <png>`.
- `go.sh` aceita `SLOWHD_PREFIX` próprio (ex.: só a pasta Imagens no disco lento).
