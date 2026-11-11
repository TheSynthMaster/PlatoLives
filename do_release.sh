#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

fail() {
    echo "[ERROR] $*" >&2
    exit 1
}

on_error() {
    local status=$?
    echo "[ERROR] Comando fallito alla riga ${BASH_LINENO[0]} (exit ${status}): ${BASH_COMMAND}" >&2
    exit "$status"
}
trap on_error ERR

require_file() {
    [[ -f "$1" ]] || fail "File richiesto non trovato: $1"
}

require_dir() {
    [[ -d "$1" ]] || fail "Directory richiesta non trovata: $1"
}

# ---------------------------------------------------------------------------
# 0. Controlli preliminari
# ---------------------------------------------------------------------------

require_file VERSION
require_file LICENSE
require_file README.md
require_dir build/PlatoLives.app

VERSION_STR="$(tr -d ' \n\r' < VERSION)"

# Accetta sia versioni major.minor (5.7) sia major.minor.patch (5.7.0).
[[ "$VERSION_STR" =~ ^[0-9]+\.[0-9]+(\.[0-9]+)?$ ]] ||
    fail "Formato VERSION non valido: '${VERSION_STR}' (attesi, ad esempio, 5.7 oppure 5.7.0)"

TAG="v${VERSION_STR}"
DMG_NAME="PlatoLives-${VERSION_STR}-macOS.dmg"
DMG_PATH="build/${DMG_NAME}"

command -v git >/dev/null 2>&1 || fail "Git non installato."
command -v hdiutil >/dev/null 2>&1 || fail "hdiutil non disponibile: la creazione DMG richiede macOS."
command -v gh >/dev/null 2>&1 || fail "GitHub CLI (gh) non installata."
git rev-parse --is-inside-work-tree >/dev/null 2>&1 || fail "La directory corrente non è un repository Git."
git remote get-url origin >/dev/null 2>&1 || fail "Remote Git 'origin' non configurato."
gh auth status >/dev/null 2>&1 || fail "GitHub CLI non autenticata. Esegui gh auth login."

echo "================================================================="
echo ">>> PLATOLIVES - RELEASE ${TAG}"
echo "================================================================="

# Verifica tutti gli artefatti prima di modificare Git o pubblicare.
ASSETS=(
    "$DMG_PATH"
    "build/PlatoLives-windows-x86_64.exe"
    "build/PlatoLives-windows-arm64.exe"
    "build/PlatoLives-linux-gui-arm64"
    "build/PlatoLives-linux-gui-x86_64"
    "build/PlatoLives-linux-desktop-install.tar.gz"
    "build/PlatoLives-console-linux-arm64"
    "build/PlatoLives-console-linux-x86_64"
)

# Il DMG viene creato più avanti; tutti gli altri artefatti devono già esistere.
for asset in "${ASSETS[@]:1}"; do
    require_file "$asset"
done

# Non sovrascrivere mai una release/tag esistente.
if git rev-parse -q --verify "refs/tags/${TAG}" >/dev/null; then
    fail "Il tag locale ${TAG} esiste già. Nessun tag verrà sovrascritto."
fi

if git ls-remote --exit-code --tags origin "refs/tags/${TAG}" >/dev/null 2>&1; then
    fail "Il tag ${TAG} esiste già su origin. Nessun tag verrà sovrascritto."
fi

if gh release view "$TAG" >/dev/null 2>&1; then
    fail "La GitHub Release ${TAG} esiste già. Nessuna release verrà sovrascritta."
fi

# ---------------------------------------------------------------------------
# 1. Creazione bundle DMG macOS
# ---------------------------------------------------------------------------

echo ""
echo "=== Creazione ${DMG_NAME} ==="

# Non cancellare o sovrascrivere un DMG preesistente: potrebbe contenere
# un artefatto valido di una precedente esecuzione.
[[ ! -e "$DMG_PATH" ]] || fail "Il file ${DMG_PATH} esiste già. Verificalo o spostalo manualmente prima di procedere."

DMG_TEMP="$(mktemp -d "${TMPDIR:-/tmp}/platolives-release.XXXXXX")"
cleanup() {
    rm -rf "$DMG_TEMP"
}
trap cleanup EXIT

cp -R build/PlatoLives.app "$DMG_TEMP/"
cp LICENSE "$DMG_TEMP/LICENSE.txt"
cp README.md "$DMG_TEMP/README.md"
cp README.md "$DMG_TEMP/README.txt"
cp VERSION "$DMG_TEMP/VERSION"
cp VERSION "$DMG_TEMP/VERSION.txt"
ln -s /Applications "$DMG_TEMP/Applications"

hdiutil create \
    -volname "PlatoLives ${VERSION_STR}" \
    -srcfolder "$DMG_TEMP" \
    -ov \
    -format UDZO \
    "$DMG_PATH"

require_file "$DMG_PATH"
hdiutil verify "$DMG_PATH"
echo "[OK] DMG creato e verificato: ${DMG_PATH}"

# Controllo finale di tutti gli asset, incluso il DMG appena generato.
for asset in "${ASSETS[@]}"; do
    require_file "$asset"
done

# ---------------------------------------------------------------------------
# 2. Aggiornamento RELEASE
# ---------------------------------------------------------------------------

echo ""
echo "=== Aggiornamento RELEASE ==="

# RELEASE viene mantenuto nel repository e usato anche come corpo della
# GitHub Release. Va quindi aggiornato PRIMA del commit e del push.
cat > RELEASE <<EOF
# PlatoLives ${VERSION_STR}

## What's new in PlatoLives ${VERSION_STR}

- **C64 TurboLoad Effect**: optional TurboLoad effect with stripes along the screen border in Commodore 64 mode. The effect can be enabled or disabled.
- **Integer Window Size Presets**: predefined 1×, 2×, 3× and 4× window scales. The 1× preset corresponds to the native 512 × 512 resolution.
- **Script Keyboard Control**: new \`keyboard [on/off]\` scripting command to enable or disable keyboard input listening while a script is running.

See [README.md](https://github.com/TheSynthMaster/PlatoLives/blob/${TAG}/README.md) for the project overview and documentation.
EOF

require_file RELEASE
echo "[OK] RELEASE aggiornato."

# ---------------------------------------------------------------------------
# 3. Commit e tag Git
# ---------------------------------------------------------------------------

echo ""
echo "=== Git Commit & Tag ${TAG} ==="

git add -A

if ! git diff --cached --quiet; then
    git commit -m "Release ${TAG}: C64 TurboLoad, integer window scaling & script keyboard control"
else
    echo "[INFO] Nessuna modifica da committare; verrà taggato il commit corrente."
fi

# Il tag è creato senza -f: non può sovrascrivere un tag esistente.
git tag -a "$TAG" \
    -m "PlatoLives ${VERSION_STR} - C64 TurboLoad, integer window scaling and script keyboard control"

echo "[OK] Commit e tag locale pronti."

# ---------------------------------------------------------------------------
# 3. Push Git e tag
# ---------------------------------------------------------------------------

echo ""
echo "=== Push su origin ==="

git push origin HEAD
git push origin "refs/tags/${TAG}"

echo "[OK] Commit e tag ${TAG} inviati su origin."

# ---------------------------------------------------------------------------
# 4. Pubblicazione GitHub Release
# ---------------------------------------------------------------------------

echo ""
echo "=== Pubblicazione GitHub Release ${TAG} ==="

gh release create "$TAG" \
    "${ASSETS[@]}" \
    --title "PlatoLives ${TAG}" \
    --notes-file RELEASE

echo ""
echo "================================================================="
echo ">>> RELEASE ${TAG} PUBBLICATA CON SUCCESSO!"
echo "================================================================="
gh release view "$TAG" --web
