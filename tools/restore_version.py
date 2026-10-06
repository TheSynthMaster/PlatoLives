#!/usr/bin/env python3
import os
import sys
import shutil
import time
import filecmp

# Configurazione cartelle: VERSIONS è allo stesso livello di PlatoLives
REPO_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
VERSIONS_ROOT = os.path.abspath(os.path.join(REPO_DIR, "..", "VERSIONS"))
OLD_DIR = os.path.join(REPO_DIR, "old")
SUBDIRS = ["src", "include", "platolives-win", "platolives-mac"]

def list_versions():
    if not os.path.exists(VERSIONS_ROOT):
        return []
    entries = []
    for d in os.listdir(VERSIONS_ROOT):
        p = os.path.join(VERSIONS_ROOT, d)
        if os.path.isdir(p) and not d.startswith("."):
            mtime = os.path.getmtime(p)
            entries.append((d, mtime))
    # Ordina dal più recente al più vecchio
    entries.sort(key=lambda x: x[1], reverse=True)
    return [e[0] for e in entries]

def main():
    print("=======================================================")
    print(" PLATOLIVES VERSION RESTORE & TEST UTILITY")
    print("=======================================================\n")

    if not os.path.exists(VERSIONS_ROOT):
        print(f"[ERRORE] Cartella VERSIONS non trovata in: {VERSIONS_ROOT}")
        sys.exit(1)

    versions = list_versions()
    if not versions:
        print(f"[ERRORE] Nessuna versione trovata in: {VERSIONS_ROOT}")
        sys.exit(1)

    selected_ver = None
    if len(sys.argv) > 1:
        arg = sys.argv[1].strip()
        name = os.path.basename(arg.rstrip("/"))
        if name in versions:
            selected_ver = name
        else:
            matches = [v for v in versions if name.lower() in v.lower()]
            if len(matches) == 1:
                selected_ver = matches[0]
            else:
                print(f"[ERRORE] Versione '{arg}' non trovata in {VERSIONS_ROOT}.")
                print(f"Versioni disponibili ({len(versions)}): {', '.join(versions)}")
                sys.exit(1)
    else:
        print(f"Cartella sorgente: {VERSIONS_ROOT}")
        print(f"Versioni disponibili (dalla più recente alla più vecchia):")
        for i, v in enumerate(versions):
            print(f"  [{i+1:2d}] {v}")
        print()
        choice = input(f"Seleziona numero [1-{len(versions)}] o nome versione: ").strip()
        if choice.isdigit() and 1 <= int(choice) <= len(versions):
            selected_ver = versions[int(choice) - 1]
        elif choice in versions:
            selected_ver = choice
        else:
            matches = [v for v in versions if choice.lower() in v.lower()]
            if len(matches) == 1:
                selected_ver = matches[0]
            else:
                print("[ANNULLATO] Selezione non valida.")
                sys.exit(1)

    src_root = os.path.join(VERSIONS_ROOT, selected_ver)
    print(f"\n[INFO] Ripristino da: {src_root}")
    print(f"[INFO] Destinazione:  {REPO_DIR}\n")

    os.makedirs(OLD_DIR, exist_ok=True)
    timestamp = int(time.time())

    restored_files = []
    identical_files = 0

    for sub in SUBDIRS:
        src_sub = os.path.join(src_root, sub)
        dst_sub = os.path.join(REPO_DIR, sub)
        if not os.path.exists(src_sub):
            continue

        for root, dirs, files in os.walk(src_sub):
            for f in files:
                if f.startswith(".") or f.endswith((".old", ".bak", ".dmp")):
                    continue
                src_file = os.path.join(root, f)
                rel_path = os.path.relpath(src_file, src_root)
                dst_file = os.path.join(REPO_DIR, rel_path)

                # Se il file esiste ed è identico, non lo tocchiamo
                if os.path.exists(dst_file) and filecmp.cmp(src_file, dst_file, shallow=False):
                    identical_files += 1
                    continue

                # Backup del file corrente prima di sovrascriverlo
                if os.path.exists(dst_file):
                    bak_name = f"{os.path.basename(dst_file)}.{timestamp}.old"
                    shutil.copyfile(dst_file, os.path.join(OLD_DIR, bak_name))

                # Ripristino
                os.makedirs(os.path.dirname(dst_file), exist_ok=True)
                shutil.copyfile(src_file, dst_file)
                restored_files.append(rel_path)

    print("=======================================================")
    print(f" ESITO RIPRISTINO: {selected_ver}")
    print("=======================================================")
    if not restored_files:
        print("[INFO] Tutti i file sono già identici alla versione selezionata. Nessuna modifica.")
    else:
        print(f"File ripristinati ({len(restored_files)}):")
        for rf in restored_files:
            print(f"  <- [RIPRISTINATO] {rf}")
    print(f"File invariati/identici: {identical_files}\n")

    print("[BUILD] Avvio compilazione automatica ./build_all.sh...\n")
    os.chdir(REPO_DIR)
    os.system("./build_all.sh")

if __name__ == "__main__":
    main()
