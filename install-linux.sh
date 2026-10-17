#!/usr/bin/env bash
# Galaxy Launcher installer for Linux.
#
# Installs (or updates) Galaxy Launcher from the latest GitHub release as a Flatpak for the current user.
# No root access is needed. Your profiles and settings are kept when updating.
#
#   bash -c "$(curl -fsSL https://raw.githubusercontent.com/ImNinnn/galaxy-launcher/main/install-linux.sh)"
#
# Uninstall with: flatpak uninstall --user io.github.galaxylauncher.GalaxyLauncher
set -euo pipefail

REPO="ImNinnn/galaxy-launcher"
APP_ID="io.github.galaxylauncher.GalaxyLauncher"
BUNDLE_URL="https://github.com/${REPO}/releases/latest/download/GalaxyLauncher.flatpak"

if ! command -v flatpak >/dev/null 2>&1; then
    echo "Galaxy Launcher needs Flatpak. Set it up for your distribution first: https://flatpak.org/setup/" >&2
    exit 1
fi
if ! command -v curl >/dev/null 2>&1; then
    echo "This installer needs curl." >&2
    exit 1
fi

# The app runs on the KDE runtime from Flathub; Flatpak downloads it during the install if needed.
echo "Making sure Flathub is available..."
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

echo "Downloading the latest Galaxy Launcher..."
curl -fL --progress-bar -o "$tmp/GalaxyLauncher.flatpak" "$BUNDLE_URL"

echo "Installing..."
flatpak install --user --noninteractive --reinstall "$tmp/GalaxyLauncher.flatpak"

echo
echo "Galaxy Launcher is installed. Start it from your applications menu, or run:"
echo "    flatpak run ${APP_ID}"
