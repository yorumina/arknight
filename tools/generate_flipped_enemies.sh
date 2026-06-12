#!/usr/bin/env bash
# Generate horizontally-flipped APNG enemy animations.
#
# Usage:
#   ./tools/generate_flipped_enemies.sh [enemy_dir]
#
# If enemy_dir is not specified, defaults to ./data/enemy.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
ENEMY_DIR="${1:-${PROJECT_DIR}/data/enemy}"

if [ ! -d "$ENEMY_DIR" ]; then
    echo "ERROR: Enemy directory not found: $ENEMY_DIR"
    exit 1
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "ERROR: ffmpeg is required to generate flipped APNG files"
    exit 1
fi

echo "=== Generating flipped enemy APNG animations ==="
echo "Enemy dir: $ENEMY_DIR"
echo ""

TOTAL=0
SKIPPED=0
GENERATED=0
FAILED=0

for enemy_subdir in "$ENEMY_DIR"/*/; do
    [ -d "$enemy_subdir" ] || continue
    enemy_name="$(basename "$enemy_subdir")"

    for apng_file in "$enemy_subdir"/*.apng; do
        [ -f "$apng_file" ] || continue
        stem="$(basename "$apng_file" .apng)"
        if [[ "$stem" == *_flip || "$stem" == *_flipped ]]; then
            continue
        fi

        TOTAL=$((TOTAL + 1))
        output_file="${enemy_subdir}${stem}_flip.apng"

        if [ -f "$output_file" ] && [ "$output_file" -nt "$apng_file" ]; then
            echo "  [$enemy_name] SKIP (up-to-date): $(basename "$output_file")"
            SKIPPED=$((SKIPPED + 1))
            continue
        fi

        echo "  [$enemy_name] Flipping: $(basename "$apng_file")"
        if ffmpeg -y -i "$apng_file" -vf hflip -plays 0 "$output_file" </dev/null >/dev/null 2>&1; then
            GENERATED=$((GENERATED + 1))
        else
            echo "  [$enemy_name] FAILED: $(basename "$apng_file")"
            rm -f "$output_file"
            FAILED=$((FAILED + 1))
        fi
    done
done

echo ""
echo "=== Summary ==="
echo "Total:     $TOTAL"
echo "Generated: $GENERATED"
echo "Skipped:   $SKIPPED"
echo "Failed:    $FAILED"

if [ "$FAILED" -gt 0 ]; then
    exit 1
fi

echo "Done."
