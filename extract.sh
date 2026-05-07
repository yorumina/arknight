#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<EOF
Usage: ./extract.sh

This project now reads operator WebM files directly from data/operators.
No PNG frames or GIFs are generated.
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

if [[ $# -gt 0 ]]; then
    echo "extract.sh no longer accepts conversion options." >&2
    usage >&2
    exit 1
fi

OPERATOR_ROOTS=("tools/ark_builder/operators" "data/operators")
WEBM_COUNT=0

echo "WebM runtime mode: no extraction will be performed."

for OP_ROOT in "${OPERATOR_ROOTS[@]}"; do
    if [[ ! -d "$OP_ROOT" ]]; then
        continue
    fi

    while IFS= read -r -d '' WEBM; do
        WEBM_COUNT=$((WEBM_COUNT + 1))
        echo "Found: $WEBM"
    done < <(find "$OP_ROOT" -path '*/photo/*' -type f -name '*.webm' -print0)
done

if [[ "$WEBM_COUNT" -eq 0 ]]; then
    echo "No operator WebM files found under data/operators or tools/ark_builder/operators." >&2
    exit 1
fi

echo "DONE: found ${WEBM_COUNT} WebM files."
