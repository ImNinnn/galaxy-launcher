#!/bin/bash
# Galaxy Launcher development helper: incremental builds inside the Flatpak SDK.
#
#   ./dev.sh deps        build the dependency modules once (cmark, toml++, xrandr) into build-dir/
#   ./dev.sh configure   configure the launcher into dev-build/ (installs into build-dir/files, i.e. /app)
#   ./dev.sh build       compile and install incrementally
#   ./dev.sh test        run the unit tests inside the SDK
#   ./dev.sh run [args]  run the launcher from build-dir with the manifest's sandbox permissions
#   ./dev.sh snapshots DATA_DIR OUT_DIR [WxH]
#                        headless scripted tour of the UI (offscreen) using DATA_DIR as the launcher data
#                        directory; screenshots are written to OUT_DIR (OFFLINE=1: without network access)
#   (snapshots and launch-test use the dev build; INSTALLED=1 runs the installed Flatpak instead)
#   ./dev.sh launch-test DATA_DIR OUT_DIR PROFILE_ID
#                        the tour, then a real demo-mode launch of PROFILE_ID by clicking its sun; the game
#                        runs on a private headless compositor (kwin_wayland --virtual), not on your desktop
#
# For a release-style package use: flatpak run org.flatpak.Builder --user --install --force-clean \
#   build-dir io.github.galaxylauncher.GalaxyLauncher.yml
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$(dirname "$HERE")"
MANIFEST="$HERE/io.github.galaxylauncher.GalaxyLauncher.yml"
BUILD_DIR="$HERE/build-dir"
DEV_BUILD="$HERE/dev-build"
JOBS="${JOBS:-8}"
APP_ID=io.github.galaxylauncher.GalaxyLauncher

sdk() {
    flatpak build --filesystem="$SRC" \
        --env=JAVA_HOME=/usr/lib/sdk/openjdk17/jvm/openjdk-17 \
        --env=PATH=/app/bin:/usr/bin:/usr/lib/sdk/openjdk17/bin \
        "$BUILD_DIR" "$@"
}

cmd="${1:-build}"
[ $# -gt 0 ] && shift

# Tests of the installed app never reach the user's real Discord (it would show their status).
nodiscord=(--nofilesystem=xdg-run/discord-ipc-0 --nofilesystem=xdg-run/app/com.discordapp.Discord)

case "$cmd" in
    deps)
        cd "$HERE"
        flatpak run --user org.flatpak.Builder --user --jobs="$JOBS" --force-clean --stop-at=galaxylauncher \
            "$BUILD_DIR" "$MANIFEST"
        ;;
    configure)
        sdk cmake -S "$SRC" -B "$DEV_BUILD" -G Ninja \
            -DCMAKE_INSTALL_PREFIX=/app \
            -DCMAKE_BUILD_TYPE=RelWithDebInfo \
            -DLauncher_BUILD_PLATFORM=flatpak \
            -DLauncher_ENABLE_JAVA_DOWNLOADER=ON \
            -DBUILD_TESTING=ON "$@"
        ;;
    build)
        sdk ninja -C "$DEV_BUILD" -j "$JOBS" "$@"
        sdk ninja -C "$DEV_BUILD" install > /dev/null
        ;;
    test)
        sdk sh -c "cd '$DEV_BUILD' && ctest --output-on-failure $*"
        ;;
    run)
        cd "$HERE"
        flatpak run --user org.flatpak.Builder --run "$BUILD_DIR" "$MANIFEST" galaxylauncher "$@"
        ;;
    snapshots)
        data="$(realpath "$1")"
        out="$(realpath -m "$2")"
        size="${3:-1400x900}"
        mkdir -p "$out"
        network=--share=network
        [ "${OFFLINE:-0}" = 1 ] && network=--unshare=network
        envs=(--env=QT_QPA_PLATFORM=offscreen "--env=GALAXYLAUNCHER_DATA_DIR=$data" "--env=GALAXY_DEV_SNAPSHOTS=$out"
              "--env=GALAXY_DEV_WINDOW_SIZE=$size" "--env=QT_LOGGING_RULES=${QT_LOGGING_RULES:-}" "--env=GALAXY_DEV_PROFILE=${GALAXY_DEV_PROFILE:-}"
              "--env=GALAXY_DEV_PASSER=${GALAXY_DEV_PASSER:-}" "--env=GALAXY_DEV_APPLY_UPDATES=${GALAXY_DEV_APPLY_UPDATES:-}"
              "--env=GALAXY_DEV_LOGIN_TEST=${GALAXY_DEV_LOGIN_TEST:-}")
        if [ "${INSTALLED:-0}" = 1 ]; then
            # The installed app, with exactly the manifest's permissions plus access to the two test folders.
            flatpak run --user "${nodiscord[@]}" "--filesystem=$data" "--filesystem=$out" "$network" "${envs[@]}" "$APP_ID"
        else
            flatpak build "--filesystem=$SRC" "--filesystem=$data" "--filesystem=$out" "$network" "${envs[@]}" \
                "$BUILD_DIR" galaxylauncher
        fi
        ;;
    launch-test)
        data="$(realpath "$1")"
        out="$(realpath -m "$2")"
        mkdir -p "$out"
        envs=(--env=QT_QPA_PLATFORM=offscreen "--env=GALAXYLAUNCHER_DATA_DIR=$data" "--env=GALAXY_DEV_SNAPSHOTS=$out"
              "--env=GALAXY_DEV_PROFILE=$3" --env=GALAXY_DEV_LAUNCH=1 "--env=GALAXY_DEV_LAUNCH_VIA=${GALAXY_DEV_LAUNCH_VIA:-}")
        if [ "${INSTALLED:-0}" = 1 ]; then
            command=(flatpak run --user "${nodiscord[@]}" "--filesystem=$data" "--filesystem=$out" "${envs[@]}" "$APP_ID")
        else
            command=(flatpak build "--filesystem=$SRC" "--filesystem=$data" "--filesystem=$out" --share=network --share=ipc
                     --socket=x11 --socket=pulseaudio --device=dri "${envs[@]}" "$BUILD_DIR" galaxylauncher)
        fi
        # kwin takes the session command as one string; quote every argument for its shell.
        kwin_wayland --virtual --xwayland --width 1280 --height 800 --socket galaxy-launch-test --exit-with-session \
            "$(printf '%q ' "${command[@]}")"
        ;;
    *)
        echo "unknown command: $cmd" >&2
        exit 1
        ;;
esac
