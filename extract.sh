#!/usr/bin/env bash
set -euo pipefail

REMOVE_BG=0
FORCE=0
BG_COLOR="0x000000"
SIMILARITY="0.08"
BLEND="0.03"

usage() {
    cat <<EOF
Usage: ./extract.sh [--remove-bg] [--force] [--bg-color 0xRRGGBB] [--similarity N] [--blend N]
  --remove-bg         Apply chroma-key background removal while extracting.
  --force             Re-generate frames even when output folder is not empty.
  --bg-color          Key color for removal, default: ${BG_COLOR}
  --similarity        Chroma-key similarity, default: ${SIMILARITY}
  --blend             Chroma-key blend/softness, default: ${BLEND}
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --remove-bg)
            REMOVE_BG=1
            shift
            ;;
        --force)
            FORCE=1
            shift
            ;;
        --bg-color)
            BG_COLOR="${2:-}"
            shift 2
            ;;
        --similarity)
            SIMILARITY="${2:-}"
            shift 2
            ;;
        --blend)
            BLEND="${2:-}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

mkdir -p PTSD/assets/sprites/operators/
echo "Extract mode: remove_bg=${REMOVE_BG}, force=${FORCE}"
if [[ "${REMOVE_BG}" -eq 1 ]]; then
    echo "Chroma key: color=${BG_COLOR}, similarity=${SIMILARITY}, blend=${BLEND}"
fi

for OP_DIR in tools/ark_builder/operators/*/; do
    OP_NAME=$(basename "$OP_DIR")
    PHOTO_DIR="${OP_DIR}photo"
    if [[ ! -d "$PHOTO_DIR" ]]; then
        continue
    fi

    for WEBM in "$PHOTO_DIR"/*.webm; do
        if [[ ! -f "$WEBM" ]]; then
            continue
        fi

        FILENAME=$(basename "$WEBM" .webm)
        OUT_DIR="PTSD/assets/sprites/operators/${OP_NAME}/${FILENAME}"

        if [[ -d "$OUT_DIR" ]] && [[ -n "$(ls -A "$OUT_DIR" 2>/dev/null)" ]]; then
            if [[ "${FORCE}" -eq 1 ]]; then
                rm -rf "$OUT_DIR"
            else
                echo "Skip (already exists): $OUT_DIR"
                continue
            fi
        fi

        mkdir -p "$OUT_DIR"
        echo "Extracting $WEBM -> $OUT_DIR"

        if [[ "${REMOVE_BG}" -eq 1 ]]; then
            ffmpeg -v error -i "$WEBM" \
                -vf "colorkey=${BG_COLOR}:${SIMILARITY}:${BLEND},format=rgba" \
                -c:v png "$OUT_DIR/frame-%03d.png" -y
        else
            ffmpeg -v error -i "$WEBM" -c:v png "$OUT_DIR/frame-%03d.png" -y
        fi
    done
done

echo "DONE"
