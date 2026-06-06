#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# patch_libnet.sh — Aplica o weaken-symbol na libnet80211.a do arduino-esp32
#
# Este script DEVE ser executado UMA VEZ antes de compilar no Arduino IDE.
# Ele torna o símbolo ieee80211_raw_frame_sanity_check "fraco" na libnet80211.a,
# permitindo que a implementação do wsl_bypasser.c sobreponha o comportamento.
#
# USO:
#   chmod +x patch_libnet.sh
#   ./patch_libnet.sh
#
# Ou para reverter:
#   ./patch_libnet.sh --restore
#
# ─────────────────────────────────────────────────────────────────────────────

set -e

SYMBOL="ieee80211_raw_frame_sanity_check"
BACKUP_SUFFIX=".orig_backup"

# ── Detectar sistema operacional ──────────────────────────────────────────────
case "$(uname -s)" in
    Linux*)  OS="linux" ;;
    Darwin*) OS="macos" ;;
    MINGW*|MSYS*|CYGWIN*) OS="windows" ;;
    *)       OS="unknown" ;;
esac

echo "=== R4BB1T FHC — patch_libnet.sh ==="
echo "Sistema: $OS"
echo ""

# ── Localizar libnet80211.a ───────────────────────────────────────────────────
find_libnet() {
    local search_dirs=()

    if [ "$OS" = "linux" ]; then
        search_dirs=(
            "$HOME/.arduino15/packages/esp32/hardware/esp32"
            "$HOME/snap/arduino/current/.arduino15/packages/esp32/hardware/esp32"
        )
    elif [ "$OS" = "macos" ]; then
        search_dirs=(
            "$HOME/Library/Arduino15/packages/esp32/hardware/esp32"
        )
    elif [ "$OS" = "windows" ]; then
        search_dirs=(
            "$LOCALAPPDATA/Arduino15/packages/esp32/hardware/esp32"
            "$HOME/AppData/Local/Arduino15/packages/esp32/hardware/esp32"
        )
    fi

    for dir in "${search_dirs[@]}"; do
        if [ -d "$dir" ]; then
            # Pega a versão mais recente instalada
            local latest=$(ls -v "$dir" 2>/dev/null | tail -1)
            if [ -n "$latest" ]; then
                local libpath="$dir/$latest/tools/sdk/esp32/lib/libnet80211.a"
                if [ -f "$libpath" ]; then
                    echo "$libpath"
                    return 0
                fi
            fi
        fi
    done

    return 1
}

# ── Localizar objcopy ─────────────────────────────────────────────────────────
find_objcopy() {
    local search_dirs=()

    if [ "$OS" = "linux" ]; then
        search_dirs=(
            "$HOME/.arduino15/packages/esp32/tools/xtensa-esp32-elf-gcc"
            "$HOME/snap/arduino/current/.arduino15/packages/esp32/tools/xtensa-esp32-elf-gcc"
        )
    elif [ "$OS" = "macos" ]; then
        search_dirs=(
            "$HOME/Library/Arduino15/packages/esp32/tools/xtensa-esp32-elf-gcc"
        )
    elif [ "$OS" = "windows" ]; then
        search_dirs=(
            "$LOCALAPPDATA/Arduino15/packages/esp32/tools/xtensa-esp32-elf-gcc"
        )
    fi

    for dir in "${search_dirs[@]}"; do
        if [ -d "$dir" ]; then
            local latest=$(ls -v "$dir" 2>/dev/null | tail -1)
            if [ -n "$latest" ]; then
                local objcopy_path=$(find "$dir/$latest" -name "xtensa-esp32-elf-objcopy" 2>/dev/null | head -1)
                if [ -n "$objcopy_path" ]; then
                    echo "$objcopy_path"
                    return 0
                fi
            fi
        fi
    done

    # Tentar PATH direto
    if command -v xtensa-esp32-elf-objcopy &> /dev/null; then
        echo "xtensa-esp32-elf-objcopy"
        return 0
    fi

    return 1
}

# ── Modo restauração ──────────────────────────────────────────────────────────
if [ "$1" = "--restore" ]; then
    LIBPATH=$(find_libnet || true)
    if [ -z "$LIBPATH" ]; then
        echo "[ERRO] libnet80211.a não encontrada."
        exit 1
    fi
    BACKUP="${LIBPATH}${BACKUP_SUFFIX}"
    if [ -f "$BACKUP" ]; then
        cp "$BACKUP" "$LIBPATH"
        echo "[OK] Restaurado de: $BACKUP"
    else
        echo "[INFO] Nenhum backup encontrado em: $BACKUP"
    fi
    exit 0
fi

# ── Modo aplicação do patch ───────────────────────────────────────────────────
echo "Procurando libnet80211.a..."
LIBPATH=$(find_libnet || true)

if [ -z "$LIBPATH" ]; then
    echo ""
    echo "[ERRO] libnet80211.a não encontrada automaticamente."
    echo ""
    echo "Informe o caminho manualmente:"
    echo "  No Linux/macOS: ~/.arduino15/packages/esp32/hardware/esp32/X.X.X/tools/sdk/esp32/lib/libnet80211.a"
    echo "  No Windows: %LOCALAPPDATA%\\Arduino15\\packages\\esp32\\hardware\\esp32\\X.X.X\\tools\\sdk\\esp32\\lib\\libnet80211.a"
    echo ""
    echo "Depois execute:"
    echo "  ./patch_libnet.sh /caminho/completo/para/libnet80211.a"
    exit 1
fi

# Aceitar caminho manual como argumento
if [ -n "$1" ] && [ "$1" != "--restore" ]; then
    LIBPATH="$1"
fi

echo "Encontrado: $LIBPATH"

# Verificar backup
BACKUP="${LIBPATH}${BACKUP_SUFFIX}"
if [ ! -f "$BACKUP" ]; then
    echo "Criando backup: $BACKUP"
    cp "$LIBPATH" "$BACKUP"
else
    echo "Backup já existe: $BACKUP"
fi

# Localizar objcopy
echo "Procurando xtensa-esp32-elf-objcopy..."
OBJCOPY=$(find_objcopy || true)

if [ -z "$OBJCOPY" ]; then
    echo "[ERRO] xtensa-esp32-elf-objcopy não encontrado."
    echo "Instale o ESP32 board package no Arduino IDE e tente novamente."
    exit 1
fi

echo "Usando: $OBJCOPY"
echo ""
echo "Aplicando weaken-symbol em: $SYMBOL"

"$OBJCOPY" --weaken-symbol="$SYMBOL" "$LIBPATH" "$LIBPATH"

echo ""
echo "✅ Patch aplicado com sucesso!"
echo ""
echo "Verificando símbolo:"
# Verificar se o símbolo ficou fraco (w = weak)
if command -v nm &> /dev/null; then
    nm_cmd="nm"
elif command -v xtensa-esp32-elf-nm &> /dev/null; then
    nm_cmd="xtensa-esp32-elf-nm"
else
    nm_cmd=""
fi

if [ -n "$nm_cmd" ]; then
    RESULT=$("$nm_cmd" "$LIBPATH" 2>/dev/null | grep "$SYMBOL" | head -3)
    if echo "$RESULT" | grep -q "W\|w"; then
        echo "  ✅ '$SYMBOL' está WEAK (W) — bypass ativo"
    else
        echo "  ⚠️  Não foi possível confirmar (símbolo pode estar em objeto comprimido)"
    fi
    echo "  $RESULT"
fi

echo ""
echo "Agora compile e grave o r4bb1t_fhc no Arduino IDE."
echo "Para reverter: ./patch_libnet.sh --restore"
