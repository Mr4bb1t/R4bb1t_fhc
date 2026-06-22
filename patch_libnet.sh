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

BACKUP_SUFFIX=".orig_backup"

SYMBOLS=(
    "ieee80211_raw_frame_sanity_check"
    "ieee80211_is_tx_allowed"
)

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
            "$HOME/.arduino15/packages/esp32/tools/esp32-libs"
            "$HOME/snap/arduino/current/.arduino15/packages/esp32/tools/esp32-libs"
        )
    elif [ "$OS" = "macos" ]; then
        search_dirs=(
            "$HOME/Library/Arduino15/packages/esp32/tools/esp32-libs"
        )
    elif [ "$OS" = "windows" ]; then
        search_dirs=(
            "$LOCALAPPDATA/Arduino15/packages/esp32/tools/esp32-libs"
            "$HOME/AppData/Local/Arduino15/packages/esp32/tools/esp32-libs"
        )
    fi

    for dir in "${search_dirs[@]}"; do
        if [ -d "$dir" ]; then
            # Procura em todos os subdiretórios por libnet80211.a
            local libpath=$(find "$dir" -name "libnet80211.a" 2>/dev/null | head -1)
            if [ -n "$libpath" ]; then
                echo "$libpath"
                return 0
            fi
        fi
    done

    # Fallback genérico em packages/esp32
    if [ "$OS" = "linux" ]; then
        local fallback_dir="$HOME/.arduino15/packages/esp32"
    elif [ "$OS" = "macos" ]; then
        local fallback_dir="$HOME/Library/Arduino15/packages/esp32"
    elif [ "$OS" = "windows" ]; then
        local fallback_dir="$LOCALAPPDATA/Arduino15/packages/esp32"
    fi
    
    if [ -d "$fallback_dir" ]; then
        local libpath=$(find "$fallback_dir" -name "libnet80211.a" 2>/dev/null | head -1)
        if [ -n "$libpath" ]; then
            echo "$libpath"
            return 0
        fi
    fi

    return 1
}

# ── Localizar objcopy ─────────────────────────────────────────────────────────
find_objcopy() {
    local search_dirs=()

    if [ "$OS" = "linux" ]; then
        search_dirs=(
            "$HOME/.arduino15/packages/esp32/tools"
            "$HOME/snap/arduino/current/.arduino15/packages/esp32/tools"
        )
    elif [ "$OS" = "macos" ]; then
        search_dirs=(
            "$HOME/Library/Arduino15/packages/esp32/tools"
        )
    elif [ "$OS" = "windows" ]; then
        search_dirs=(
            "$LOCALAPPDATA/Arduino15/packages/esp32/tools"
        )
    fi

    for dir in "${search_dirs[@]}"; do
        if [ -d "$dir" ]; then
            # IDF 5.x / arduino-esp32 3.x: toolchain chama-se esp-x32, binario = xtensa-esp-elf-objcopy
            local objcopy_path=$(find "$dir/esp-x32" -name "xtensa-esp-elf-objcopy*" 2>/dev/null | head -1)
            if [ -n "$objcopy_path" ]; then
                echo "$objcopy_path"
                return 0
            fi
            
            # Fallback geral
            local objcopy_path=$(find "$dir" -name "*objcopy" -o -name "*objcopy.exe" 2>/dev/null | grep -i "xtensa" | head -1)
            if [ -n "$objcopy_path" ]; then
                echo "$objcopy_path"
                return 0
            fi
        fi
    done

    # Tentar PATH direto
    if command -v xtensa-esp-elf-objcopy &> /dev/null; then
        echo "xtensa-esp-elf-objcopy"
        return 0
    elif command -v xtensa-esp32-elf-objcopy &> /dev/null; then
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

# ── Aplicar weaken-symbol em todos os símbolos ────────────────────────────────
echo "Aplicando patches..."
echo ""

SUCCESS_COUNT=0
TOTAL=${#SYMBOLS[@]}

for i in "${!SYMBOLS[@]}"; do
    SYM="${SYMBOLS[$i]}"
    NUM=$((i + 1))
    echo "[$NUM/$TOTAL] Weakening: $SYM"

    "$OBJCOPY" --weaken-symbol="$SYM" "$LIBPATH" "$LIBPATH"

    if [ $? -ne 0 ]; then
        echo "  [ERRO] Falha ao enfraquecer $SYM!"
    else
        echo "  OK"
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    fi
done

echo ""
if [ "$SUCCESS_COUNT" -eq "$TOTAL" ]; then
    echo "✅ Todos os $TOTAL patches aplicados com sucesso!"
else
    echo "⚠️  $SUCCESS_COUNT/$TOTAL patches aplicados."
fi

echo ""
echo "Verificando símbolos:"
if command -v nm &> /dev/null; then
    nm_cmd="nm"
elif command -v xtensa-esp32-elf-nm &> /dev/null; then
    nm_cmd="xtensa-esp32-elf-nm"
else
    nm_cmd=""
fi

if [ -n "$nm_cmd" ]; then
    for SYM in "${SYMBOLS[@]}"; do
        RESULT=$("$nm_cmd" "$LIBPATH" 2>/dev/null | grep "$SYM" | head -3)
        if echo "$RESULT" | grep -q "W\|w"; then
            echo "  ✅ '$SYM' está WEAK — bypass ativo"
        else
            echo "  ⚠️  '$SYM' — símbolo pode estar em objeto comprimido"
        fi
        echo "  $RESULT"
    done
fi

echo ""
echo "Símbolos enfraquecidos:"
echo "  - ieee80211_raw_frame_sanity_check (management frames)"
echo "  - ieee80211_is_tx_allowed (control frames / TX permission)"
echo ""
echo "Agora compile e grave o r4bb1t_fhc no Arduino IDE."
echo "Para reverter: ./patch_libnet.sh --restore"
