#!/bin/zsh

sftp 192.168.1.127 <<EOF
cd /share/Personale/PlatoLives/
put build/PlatoLives-console-linux-x86_64
put build/PlatoLives-windows-x86_64.exe
put PlatoLives-windows-arm64.exe
put PlatoLives-linux-desktop-install.tar.gz
put -r build/linux_desktop_install
cd /share/Container/netshoot
put build/PlatoLives-console-linux-arm64
bye
EOF
