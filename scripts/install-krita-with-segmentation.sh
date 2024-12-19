#!/bin/sh
set -e  # Exit on error

echo "Installing Krita to ~/.Applications/Krita (hidden in your home directory)"
INSTALL_DIR=~/.Applications/Krita
mkdir -p "$INSTALL_DIR"
cd "$INSTALL_DIR"

echo "Getting the latest download for Krita and Segmentation Tools"
KRITA_VERSION="5.2.6"
KRITA_URL="https://download.kde.org/stable/krita/${KRITA_VERSION}/krita-${KRITA_VERSION}-x86_64.appimage"
KRITA_FILENAME="krita-${KRITA_VERSION}-x86_64.appimage"

if [ ! -f "$KRITA_FILENAME" ]; then
    echo "Downloading new Krita"
    wget -O "$KRITA_FILENAME" "$KRITA_URL"
    chmod +x "$KRITA_FILENAME"
    
    echo "Extracting AppImage..."
    if [ -d "squashfs-root" ]; then
        echo "Removing existing squashfs-root directory..."
        rm -rf squashfs-root
    fi
    "./$KRITA_FILENAME" --appimage-extract
else
    echo "Latest Krita is already installed; moving on."
fi

if [ ! -d "squashfs-root" ]; then
    echo "Error: AppImage extraction failed - squashfs-root directory not found"
    exit 1
fi

echo "Updating the desktop file with correct paths"
EXTRACTED_DESKTOP_FILE="$INSTALL_DIR/squashfs-root/org.kde.krita.desktop"
ICON_PATH="$INSTALL_DIR/squashfs-root/krita.png"

if [ -f "$EXTRACTED_DESKTOP_FILE" ]; then
    echo "Pointing shortcut to Krita and tools"
    sed -i "s|^Exec=.*|Exec=env APPDIR='$INSTALL_DIR/squashfs-root' APPIMAGE=1 '$INSTALL_DIR/squashfs-root/AppRun' %U|" "$EXTRACTED_DESKTOP_FILE"
    sed -i "s|^Icon=.*|Icon=$ICON_PATH|" "$EXTRACTED_DESKTOP_FILE"
    mkdir -p ~/.local/share/applications/
    cp "$EXTRACTED_DESKTOP_FILE" ~/.local/share/applications/
else
    echo "Error: Extracted desktop file not found at $EXTRACTED_DESKTOP_FILE"
    exit 1
fi

echo "Checking if segmentation plugin is outdated"
LATEST_PLUGIN=$(curl -s https://api.github.com/repos/Acly/krita-ai-tools/releases/latest | grep browser_download_url | grep tar.gz | cut -d '"' -f 4)
PLUGIN_FILENAME=$(basename "$LATEST_PLUGIN")

if [ ! -f "$PLUGIN_FILENAME" ]; then
    echo "Downloading new plugin"
    wget -O "$PLUGIN_FILENAME" "$LATEST_PLUGIN"
    tar -xf "$PLUGIN_FILENAME" -C squashfs-root/
else
    echo "Latest segmentation plugin is already installed."
fi

echo "DONE! Installed Krita with Segmentation tools built in."
echo "For more info go here: https://github.com/Acly/krita-ai-tools"