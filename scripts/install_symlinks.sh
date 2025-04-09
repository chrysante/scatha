#!/bin/sh

# List of binaries to symlink
BINARIES="scatha scathac scathadb svm"

# Installation prefix (default to /usr/local if not specified)
PREFIX="${1:-/usr/local}"

BIN_DIR="$PREFIX/bin"
TARGET_DIR="$PREFIX/lib/scatha/bin"

echo "Creating symlinks in $BIN_DIR pointing to $TARGET_DIR..."

for binary in $BINARIES; do
  target="$TARGET_DIR/$binary"
  link="$BIN_DIR/$binary"

  if [ ! -f "$target" ]; then
    echo "Skipping $binary — target not found at $target"
    continue
  fi

  echo "$link -> $target"
  ln -sf "$target" "$link"
done

echo "Done."

