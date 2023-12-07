#!/bin/sh
echo "Installing Krita to ~/.Applications/Krita (hidden in your home directory)"
INSTALL_DIR=~/.Applications/Krita
mkdir -p "$INSTALL_DIR"
cd "$INSTALL_DIR"

echo "Getting the latest download for Krita and Segmentation Tools"
BASE_KRITA_URL="https://binary-factory.kde.org/job/Krita_Stable_Appimage_Build/lastSuccessfulBuild/artifact/"
KRITA_PAGE=$(curl -s $BASE_KRITA_URL)
LATEST_KRITA_APPIMAGE=$(echo "$KRITA_PAGE" | grep -o 'krita-.*\.appimage"' | head -1 | sed 's/"$//')
LATEST_PLUGIN=$(curl -s https://api.github.com/repos/Acly/krita-ai-tools/releases/latest | grep browser_download_url | grep tar.gz | cut -d '"' -f 4)

echo "Checking to see if your version of Krita is up to date?"
if [ ! -f "$LATEST_KRITA_APPIMAGE" ]; then
    echo "Downloading new Krita"
    wget -O "$LATEST_KRITA_APPIMAGE" "${BASE_KRITA_URL}${LATEST_KRITA_APPIMAGE}"
    chmod +x "$LATEST_KRITA_APPIMAGE"
    "./$LATEST_KRITA_APPIMAGE" --appimage-extract
else
    echo "Latest Krita is already installed; moving on."
fi

echo "Updating the desktop file with correct paths"
EXTRACTED_DESKTOP_FILE="$INSTALL_DIR/squashfs-root/org.kde.krita.desktop"
ICON_PATH="$INSTALL_DIR/squashfs-root/krita.png"
if [ -f "$EXTRACTED_DESKTOP_FILE" ]; then
    echo "Pointing shortcut to Krita and tools"
    sed -i 's|^Exec=.*|Exec=env APPDIR='$INSTALL_DIR'/squashfs-root APPIMAGE=1 '$INSTALL_DIR'/squashfs-root/AppRun %U|' "$EXTRACTED_DESKTOP_FILE"
    sed -i 's|^Icon=.*|Icon='$ICON_PATH'|' "$EXTRACTED_DESKTOP_FILE"
    cp "$EXTRACTED_DESKTOP_FILE" ~/.local/share/applications/
else
    echo "Extracted desktop file not found, it could have been moved."
fi

echo "Checking if segmentation plugin is outdated"
if [ ! -f "$(basename "$LATEST_PLUGIN")" ]; then
    echo "Downloading new plugin"
    wget -O "$(basename "$LATEST_PLUGIN")" "$LATEST_PLUGIN"
    tar -xf "$(basename "$LATEST_PLUGIN")" -C squashfs-root/
else
    echo "Latest segmentation plugin is already installed."
fi

echo "DONE! Installed Krita with Segmentation tools built in."
echo "For more info go here: https://github.com/Acly/krita-ai-tools"
