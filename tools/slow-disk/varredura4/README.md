# Varredura 4 (17/11 noite): macaco longo, vazamentos, timers, o player de música em todos os tamanhos

Wine em `/home/claude/wp` (o macaco, `:9`) e uma cópia em `/home/claude/wp2` (`:10`) para medir ao mesmo tempo.

- `soak2.sh <semente> <quantas> <s>`: como `race/soak.sh`, mas uma trava não para a rodada: guarda as pilhas de todas
  as linhas (`/tmp/hang-<semente>.txt`, resumo por `hangmap.py` com os símbolos de `/tmp/syms.txt`) e segue.
  `soak3.sh`: o mesmo, guardando o `cue.log` de cada semente (`/tmp/s3-<semente>.log`).
- `leak.sh <rodadas> <rótulo>` / `leak2.sh`: o Cue em `wp2` trocando 15 coisas por rodada (vídeos, músicas, pasta,
  tela inicial, próxima); handles por tipo (`prc.cpp`, NtQuerySystemInformation) e memória separada em lado Windows
  (Cue, motor, ffmpeg) e lado nativo do Wine (llvmpipe/GL) por `memsplit.py`; `sampler.sh` amostra a cada 40 s.
  `htypes.cpp`: o nome de cada tipo de handle e do objeto (achou os pipes `OLE_<pid><tid>` do COM do Wine).
- `msgs2.sh <rótulo> <espera> <medida> <ação> [args]`: mensagens/s da linha da interface por tipo, em `wp2`.
- `realsize.sh <dpi> <pasta> <música>` / `realshot.sh <exe> <dpi> <tamanho> <WxH> <música> <png>`: o Cue de verdade
  em vários tamanhos de janela (capturas); `livedrag.sh`: grava o arrastar da borda (30 qps) para achar quadros
  vazios; `whitewin.cpp`: uma janela GDI simples, para comparar (não pisca no Wine; o Cue, que desenha tudo pelo
  motor, mostra quadros pretos isolados durante o arrastar no Wine).
- O player de música sem motor: `tools/audit_music.cpp` (grade, varreduras de largura/altura, uma janela; checa
  tinta fora da janela, camadas sobrepostas, palavras sobre a capa, capa cortada, saltos).
