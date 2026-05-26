#!/bin/bash
set -e

BINARY="./build/tcm"
INSTALL_DIR="/usr/local/bin"
CONFIG_DIR="$HOME/.config/tcm"
LOG_DIR="$HOME/.local/share/tcm/logs"
RECIPES_DIR="$HOME/.config/tcm/recipes"

echo "Installing TCM..."

# Check binary exists
if [ ! -f "$BINARY" ]; then
    echo "Error: binary not found at $BINARY. Please build first."
    exit 1
fi

# Install binary
sudo install -m 755 "$BINARY" "$INSTALL_DIR/tcm"

# Create directories
mkdir -p "$CONFIG_DIR"
mkdir -p "$LOG_DIR"
mkdir -p "$RECIPES_DIR"

# Create default sessions.json if not present
if [ ! -f "$CONFIG_DIR/sessions.json" ]; then
    echo '[]' > "$CONFIG_DIR/sessions.json"
fi

# Copy example recipes
if [ -d "./recipes/examples" ]; then
    cp -n ./recipes/examples/*.json "$RECIPES_DIR/" 2>/dev/null || true
fi

echo "TCM installed successfully!"
echo "Run: tcm"
