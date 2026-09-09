#!/usr/bin/env bash
# Copy patched Spell.dbc + SkillLineAbility.dbc into every common
# AzerothCore DataDir so a clean rebuild has the custom Lava Burst ranks.
#
# Usage (from anywhere):
#   bash modules/mod-shaman-tweaks/apps/install-server-data.sh
#   bash modules/mod-shaman-tweaks/apps/install-server-data.sh /path/to/azerothcore-wotlk
#   bash modules/mod-shaman-tweaks/apps/install-server-data.sh --datadir /path/to/server/data

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODULE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
DBC_DIR="${MODULE_DIR}/data/dbc"

if [[ ! -f "${DBC_DIR}/Spell.dbc" || ! -f "${DBC_DIR}/SkillLineAbility.dbc" ]]; then
    echo "error: missing ${DBC_DIR}/Spell.dbc or SkillLineAbility.dbc" >&2
    exit 1
fi

AC_ROOT=""
EXTRA_DATADIR=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --datadir)
            EXTRA_DATADIR="${2:-}"
            shift 2
            ;;
        *)
            AC_ROOT="$1"
            shift
            ;;
    esac
done

if [[ -z "${AC_ROOT}" ]]; then
    # module lives at <ac-root>/modules/mod-shaman-tweaks
    CANDIDATE="$(cd "${MODULE_DIR}/../.." && pwd)"
    if [[ -d "${CANDIDATE}/modules" ]]; then
        AC_ROOT="${CANDIDATE}"
    fi
fi

declare -a TARGETS=()

add_target() {
    local dir="$1"
    [[ -z "${dir}" ]] && return
    TARGETS+=("${dir}")
}

if [[ -n "${EXTRA_DATADIR}" ]]; then
    add_target "${EXTRA_DATADIR}/dbc"
    add_target "${EXTRA_DATADIR}"
fi

if [[ -n "${AC_ROOT}" ]]; then
    add_target "${AC_ROOT}/data/dbc"
    add_target "${AC_ROOT}/env/dist/data/dbc"
    add_target "${AC_ROOT}/env/dist/bin/data/dbc"
    add_target "${AC_ROOT}/env/dist/bin/dbc"
    add_target "${AC_ROOT}/bin/data/dbc"
    add_target "${AC_ROOT}/bin/dbc"

    # Honour DataDir from an installed worldserver.conf if present
    for conf in \
        "${AC_ROOT}/env/dist/etc/worldserver.conf" \
        "${AC_ROOT}/env/dist/bin/worldserver.conf" \
        "${AC_ROOT}/etc/worldserver.conf"
    do
        if [[ -f "${conf}" ]]; then
            DATADIR="$(grep -E '^[[:space:]]*DataDir[[:space:]]*=' "${conf}" | tail -n1 | sed -E 's/.*=[[:space:]]*//; s/[[:space:]]*$//; s/^"//; s/"$//')"
            if [[ -n "${DATADIR}" && "${DATADIR}" != "." ]]; then
                if [[ "${DATADIR}" != /* ]]; then
                    DATADIR="$(dirname "${conf}")/${DATADIR}"
                fi
                add_target "${DATADIR}/dbc"
            fi
        fi
    done
fi

if [[ ${#TARGETS[@]} -eq 0 ]]; then
    echo "error: could not find an AzerothCore tree. Pass the core path:" >&2
    echo "  $0 /home/you/azerothcore-wotlk" >&2
    exit 1
fi

# Unique targets
mapfile -t TARGETS < <(printf '%s\n' "${TARGETS[@]}" | awk 'NF && !seen[$0]++')

COPIED=0
for dest in "${TARGETS[@]}"; do
    mkdir -p "${dest}"
    cp -f "${DBC_DIR}/Spell.dbc" "${dest}/Spell.dbc"
    cp -f "${DBC_DIR}/SkillLineAbility.dbc" "${dest}/SkillLineAbility.dbc"
    echo "installed DBC -> ${dest}"
    COPIED=$((COPIED + 1))
done

echo "Shaman Tweaks: copied patched DBC into ${COPIED} location(s)."
echo "Restart worldserver so it reloads Spell.dbc (SQL is applied on startup)."
