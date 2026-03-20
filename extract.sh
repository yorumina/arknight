#!/bin/bash
mkdir -p PTSD/assets/sprites/operators/
for OP_DIR in tools/ark_builder/operators/*/; do
    OP_NAME=$(basename "$OP_DIR")
    PHOTO_DIR="${OP_DIR}photo"
    if [ -d "$PHOTO_DIR" ]; then
        for WEBM in "$PHOTO_DIR"/*.webm; do
            if [ -f "$WEBM" ]; then
                FILENAME=$(basename "$WEBM" .webm)
                OUT_DIR="PTSD/assets/sprites/operators/${OP_NAME}/${FILENAME}"
                mkdir -p "$OUT_DIR"
                if [ -z "$(ls -A "$OUT_DIR" 2>/dev/null)" ]; then
                    echo "Extracting $WEBM to $OUT_DIR..."
                    ffmpeg -v error -i "$WEBM" -c:v png "$OUT_DIR/frame-%03d.png" -y
                fi
            fi
        done
    fi
done
echo "DONE"
