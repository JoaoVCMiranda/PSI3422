# Aula 1 parte 2 — Raspberry Pi e Raspberry Pi OS

Não tem código: as duas lições são **demonstrações** ao professor.

| Lição | Vale | Pedido |
|---|---|---|
| #1/2 | 5,0 | Mostrar `ssh -X` funcionando (tela como a figura 8 da apostila) |
| #2/2 | 5,0 | Mostrar `sshfs` funcionando (terminal e/ou gerenciador de arquivos) |

Chame o professor **uma única vez**, com as duas lições prontas.

## Comandos

    # licao #1/2 — ssh -X
    computador$ ssh -X pi@192.168.15.44
    pi@raspberrypi$ geany &        # ou: xeyes &   (sudo apt install x11-apps)

    # licao #2/2 — sshfs
    computador$ mkdir -p ~/pi
    computador$ sshfs pi@192.168.15.44: /home/jvcm/pi
    computador$ ls ~/pi
    computador$ fusermount -u /home/jvcm/pi    # desmontar

## Estado do nosso setup

- Raspberry conectada à rede **ElioKit 01A** (config gravada direto no SD, em
  `/etc/wpa_supplicant/wpa_supplicant.conf`, com `Kodi` como fallback e `GDLAB`
  como último recurso).
- **Sem IP fixo.** O IP chega por notificação no celular: `notify-ip.timer`
  publica no tópico ntfy `PSI3422_JVCM` no boot e a cada mudança de IP.
- Chave SSH já instalada (`ssh pi@192.168.15.44` entra sem senha).
- `avahi-daemon` está ativo, então `raspberrypi.local` serve de plano B.

## Exercícios (sem pontuação)

- O que são IP interno (privado) e IP externo (público).
- Mostrar o VNC funcionando (figura 7).
- Como fazer VNC/ssh/sshfs funcionarem com PC e Raspberry em roteadores
  diferentes — dica: NAT e port forwarding.
- Ambiente de trabalho simultâneo PC+Raspberry (figura 11).
