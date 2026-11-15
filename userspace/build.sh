#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

ASM="nasm"
LD="ld"

echo "MaXOS Userspace Builder"
echo "Project root: $PROJECT_ROOT"
echo ""

for src in "$SCRIPT_DIR"/*.asm; do
    [ -f "$src" ] || continue

    name=$(basename "$src" .asm)
    echo "INF starting build: $name.max"

    $ASM -f elf64 -I "$SCRIPT_DIR" "$src" -o "$SCRIPT_DIR/${name}.o"
    echo "ASM build done!"

    $LD -Ttext 0x0 -o "$SCRIPT_DIR/${name}.bin" "$SCRIPT_DIR/${name}.o"
    $LD --oformat binary -Ttext 0x0 -o "$SCRIPT_DIR/${name}.raw" "$SCRIPT_DIR/${name}.o"
    echo "LD  linking done!"

    size=$(stat -c%s "$SCRIPT_DIR/${name}.raw")
    echo "INF adding some salt..."
    # ---'.max' header (12B): magic "MAX1" | entry (u32 LE) | size (u32 LE)---
    out="$SCRIPT_DIR/${name}.max"
    printf 'MAX1' > "$out"
    printf '\x00\x00\x00\x00' >> "$out"
    size_esc=$(printf '\\x%02x\\x%02x\\x%02x\\x%02x' \
        $((size & 0xFF)) \
        $(((size >> 8) & 0xFF)) \
        $(((size >> 16) & 0xFF)) \
        $(((size >> 24) & 0xFF)))
    printf "%b" "$size_esc" >> "$out"

    cat "$SCRIPT_DIR/${name}.raw" >> "$out"

    rm -f "$SCRIPT_DIR/${name}.o" "$SCRIPT_DIR/${name}.bin" "$SCRIPT_DIR/${name}.raw"

    echo "Created $name.max (code: $size bytes, total: $((size + 12)) bytes)!"
    #echo ""
done

echo "Copy $name.max to 'initramfs/' dir."
if command -v cpio >/dev/null 2>&1; then
    echo "INF copying *.max into initramfs/..."
    for f in "$SCRIPT_DIR"/*.max; do
        [ -f "$f" ] || continue
        cp "$f" "$PROJECT_ROOT/initramfs/"
    done
    echo "INF repacking initramfs.cpio..."
    ( cd "$PROJECT_ROOT/initramfs" && \
      find . | LC_ALL=C sort | cpio -o -H newc --quiet > "$PROJECT_ROOT/initramfs.cpio" )
    echo "INF initramfs.cpio: $(stat -c%s "$PROJECT_ROOT/initramfs.cpio") bytes"
fi

echo "Build complete"
