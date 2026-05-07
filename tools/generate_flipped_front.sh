#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# generate_flipped_front.sh
#
# For each operator that has a front/ animation directory, generate a
# front_flip/ directory containing horizontally-flipped copies of every
# WebM file.  This avoids expensive real-time UV flipping.
#
# Usage:
#   ./tools/generate_flipped_front.sh [operators_dir]
#
# If operators_dir is not specified, defaults to ./data/operators
# ─────────────────────────────────────────────────────────────────────────────

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
OPERATORS_DIR="${1:-${PROJECT_DIR}/data/operators}"

if [ ! -d "$OPERATORS_DIR" ]; then
    echo "ERROR: Operators directory not found: $OPERATORS_DIR"
    exit 1
fi

echo "=== Generating flipped front animations ==="
echo "Operators dir: $OPERATORS_DIR"
echo ""

TOTAL=0
SKIPPED=0
GENERATED=0
FAILED=0

for op_dir in "$OPERATORS_DIR"/*/; do
    op_name="$(basename "$op_dir")"
    front_dir="${op_dir}photo/front"
    flip_dir="${op_dir}photo/front_flip"

    if [ ! -d "$front_dir" ]; then
        echo "[$op_name] No front/ directory found, skipping."
        continue
    fi

    mkdir -p "$flip_dir"

    for webm_file in "$front_dir"/*.webm; do
        [ -f "$webm_file" ] || continue
        TOTAL=$((TOTAL + 1))

        filename="$(basename "$webm_file")"
        output_file="${flip_dir}/${filename}"

        # Skip if output already exists and is newer than the source
        if [ -f "$output_file" ] && [ "$output_file" -nt "$webm_file" ]; then
            echo "  [$op_name] SKIP (up-to-date): $filename"
            SKIPPED=$((SKIPPED + 1))
            continue
        fi

        echo "  [$op_name] Flipping: $filename"
        if ffmpeg -y -i "$webm_file" -vf hflip -c:v libvpx-vp9 -crf 30 -b:v 0 \
                  -an "$output_file" </dev/null 2>/dev/null; then
            GENERATED=$((GENERATED + 1))
        else
            echo "  [$op_name] FAILED: $filename"
            FAILED=$((FAILED + 1))
            rm -f "$output_file"
        fi
    done
done

echo ""
echo "=== Summary ==="
echo "Total:     $TOTAL"
echo "Generated: $GENERATED"
echo "Skipped:   $SKIPPED"
echo "Failed:    $FAILED"
echo "Done."
