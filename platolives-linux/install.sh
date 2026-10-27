#!/bin/bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARCH="$(uname -m)"

echo "[*] Rilevata architettura di sistema: $ARCH"

if [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
    BIN_SRC="$DIR/PlatoLives-linux-gui-arm64"
elif [ "$ARCH" = "x86_64" ]; then
    BIN_SRC="$DIR/PlatoLives-linux-gui-x86_64"
else
    echo "[!] Architettura non supportata: $ARCH"
    exit 1
fi

if [ ! -f "$BIN_SRC" ]; then
    echo "[!] File binario $BIN_SRC non trovato!"
    exit 1
fi

# 1. Installazione binario utente (~/.local/bin)
mkdir -p ~/.local/bin
cp "$BIN_SRC" ~/.local/bin/platolives
chmod +x ~/.local/bin/platolives

# 2. Installazione icona ufficiale Mac
mkdir -p ~/.local/share/icons
cp "$DIR/platolives.png" ~/.local/share/icons/platolives.png

# 3. Creazione launcher .desktop per menu Applicazioni e Desktop
mkdir -p ~/.local/share/applications
cat << DESKTOP_EOF > ~/.local/share/applications/PlatoLives.desktop
[Desktop Entry]
Version=1.0
Type=Application
Name=PlatoLives
GenericName=PLATO Terminal
Comment=CDC PLATO IV / Cyber1 Network Terminal Emulator
Exec=/home/$USER/.local/bin/platolives %U
Icon=/home/$USER/.local/share/icons/platolives.png
Terminal=false
Categories=Network;Emulator;Utility;
Keywords=PLATO;Cyber1;CDC;Terminal;Retro;
StartupWMClass=PlatoLives
DESKTOP_EOF

sed -i "s/\$USER/$USER/g" ~/.local/share/applications/PlatoLives.desktop
chmod +x ~/.local/share/applications/PlatoLives.desktop

# 4. Aggiornamento cache desktop e icone
update-desktop-database ~/.local/share/applications/ 2>/dev/null || true
gtk-update-icon-cache ~/.local/share/icons/ 2>/dev/null || true

echo ""
echo "[+] Installazione completata con successo!"
echo "    - Eseguibile installato in: ~/.local/bin/platolives"
echo "    - Icona ufficiale in:      ~/.local/share/icons/platolives.png"
echo "    - Voce Menu creata in:      ~/.local/share/applications/PlatoLives.desktop"
echo "[+] PlatoLives e ora disponibile nel menu Applicazioni con la sua icona ufficiale!"
