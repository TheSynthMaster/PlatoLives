#!/bin/zsh

sftp 192.168.1.127 <<EOF
cd /share/Personale/PlatoLives
put PlatoLives-4.2.dmg
put build/PlatoLives-linux-arm64
put build/PlatoLives-linux-x86_64
put build/PlatoLives-windows-x86_64.exe
mput screenshots/*
cd /share/Container/netshoot
put build/PlatoLives-linux-arm64
bye
EOF
