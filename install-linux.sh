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
BUNDLE_NAME="GalaxyLauncher.flatpak"

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

# The newest release that has a Flatpak bundle, preview releases included (GitHub's "latest" skips those).
echo "Looking for the newest release..."
bundle_url="$(curl -fsSL -H "Accept: application/vnd.github+json" "https://api.github.com/repos/${REPO}/releases?per_page=20" |
    grep -o "\"browser_download_url\": *\"[^\"]*/${BUNDLE_NAME}\"" | head -n 1 | sed 's/.*"\(https:[^"]*\)"$/\1/' || true)"
if [ -z "$bundle_url" ]; then
    echo "Could not find a Galaxy Launcher release with a Flatpak bundle at https://github.com/${REPO}/releases" >&2
    exit 1
fi

echo "Downloading ${bundle_url##*/download/}..."
curl -fL --progress-bar -o "$tmp/${BUNDLE_NAME}" "$bundle_url"

echo "Installing..."
flatpak install --user --noninteractive --reinstall "$tmp/${BUNDLE_NAME}"

echo
echo "Galaxy Launcher is installed. Start it from your applications menu, or run:"
echo "    flatpak run ${APP_ID}"
