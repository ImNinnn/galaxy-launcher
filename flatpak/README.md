# Galaxy Launcher Flatpak

App ID: `io.github.galaxylauncher.GalaxyLauncher` (change `Launcher_AppID` in `program_info/CMakeLists.txt` and the
manifest's `id` together if you publish under another ID).

## Build and install

Requirements on the host: `flatpak`, Flathub as a remote, and these installed (user installs are fine):

```sh
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install --user flathub org.flatpak.Builder org.kde.Sdk//6.10 org.freedesktop.Sdk.Extension.openjdk17//25.08
git submodule update --init libraries/libnbtplusplus
```

Release-style build, installed for the current user:

```sh
cd flatpak
flatpak run org.flatpak.Builder --user --install --force-clean release-build io.github.galaxylauncher.GalaxyLauncher.yml
flatpak run io.github.galaxylauncher.GalaxyLauncher
```

Uninstall (add `--delete-data` to also remove profiles, accounts and settings):

```sh
flatpak uninstall --user io.github.galaxylauncher.GalaxyLauncher
```

For development, `dev.sh` builds incrementally inside the same SDK (`./dev.sh deps`, `configure`, `build`, `test`,
`run`, `snapshots`, `launch-test`; see the header of the script).

## Microsoft sign-in credentials

Galaxy Launcher signs in with its own Microsoft Entra application; its client ID is the default of
`Launcher_MSA_CLIENT_ID` in `CMakeLists.txt`. Sign-in opens the browser, and Microsoft sends it back to a page served
by the launcher on `http://localhost:<port>/` (listening on 127.0.0.1 only).

Galaxy Launcher uses **Modrinth only**: CurseForge is disabled everywhere (no pages, searches, update checks or API
calls), even if a CurseForge key were configured.

A fork should register its own application and provide its client ID, either at build time (the `-DLauncher_...`
lines in the manifest's `config-opts`):

```yaml
      - -DLauncher_MSA_CLIENT_ID=<Azure application (client) ID>
```

or at run time: *Add Microsoft* in the account manager walks through it the first time, and it can be changed in
**Settings › APIs** (takes effect immediately).

An MSA client ID comes from registering an application in Microsoft Entra ID (personal Microsoft accounts, platform
*Mobile and desktop applications* with the redirect URI `http://localhost`, public client flows enabled) and
requesting Minecraft API access from Mojang for it.

## Permissions

| Permission | Why |
| --- | --- |
| `--socket=wayland`, `--socket=x11`, `--share=ipc` | Launcher UI on Wayland or X11. Minecraft's bundled GLFW uses X11 (XWayland) even on Wayland sessions, so X11 is required, not a fallback. |
| `--device=dri` | GPU access for the game (also exposes NVIDIA device nodes). |
| `--device=input` | Game controllers for controller mods. |
| `--socket=pulseaudio` | Game audio and voice chat mods. |
| `--share=network` | Accounts, version metadata, libraries and assets, Java downloads, Modrinth, multiplayer, Discord's app icon. |
| `--filesystem=xdg-run/discord-ipc-0`, `--filesystem=xdg-run/app/com.discordapp.Discord:create` | Discord Rich Presence: the socket of the native Discord app and of the Discord Flatpak. Can be turned off in Settings › Galaxy. |

Not requested: home directory access, host executables, the host's Java, or session bus names beyond what the KDE
runtime adds for theming (`org.kde.KGlobalSettings`, `org.kde.kconfig.notify`, `com.canonical.AppMenu.Registrar`)
and read-only access to `kdeglobals`. Other files (modpack imports, adding a mod file, exporting) go through the
desktop's file chooser portal, and "Open folder" actions through the OpenURI portal.

## Where data lives

Everything is inside the sandbox's XDG data directory, so it persists across updates and is removed with
`--delete-data`:

```
~/.var/app/io.github.galaxylauncher.GalaxyLauncher/data/GalaxyLauncher/
├── galaxylauncher.cfg   launcher settings (including Galaxy settings)
├── accounts.json        accounts
├── instances/           profiles; each instance.cfg also holds its sun color (GalaxySunColor)
├── java/                Java runtimes downloaded by the launcher (Mojang/Adoptium/Azul builds)
├── libraries/ assets/   shared game files
└── cache/ metacache/    download caches
```

Galaxy Launcher keeps Prism's single-folder layout (configuration and caches next to the profiles) so profiles and
whole data folders stay portable between Prism and Galaxy Launcher. It is separate from an installed Prism Launcher
(`org.prismlauncher.PrismLauncher`); to bring profiles over, use *Import profile* with an exported instance zip, or
copy instance folders into `instances/` while both launchers are closed.

## Java

Java is not bundled. With *Settings › Java › Auto-download Java* (the default on first run), the launcher downloads
the runtime each Minecraft version needs into `java/` inside the sandbox and uses it automatically. Host Java
installations are not visible to the sandbox by design.

## Bundled modules

| Module | Why |
| --- | --- |
| cmark | Markdown rendering (changelogs, descriptions); not in the KDE runtime. |
| toml++ | Mod metadata parsing; not in the KDE runtime. |
| flite | Text to speech for Minecraft's narrator. |
| xrandr | Minecraft before 1.13 (LWJGL 2) queries display modes through the `xrandr` binary. |

## What was verified

On Fedora 44 (KDE Plasma, Wayland, NVIDIA), with this manifest:

* Built with `org.flatpak.Builder` and installed as a user Flatpak; permissions match the table above.
* Runs inside the sandbox; data goes to the location above.
* Galaxy, system, list and empty-galaxy views, hover, filter and the launch animation (headless screenshot tour).
* Real launches by clicking a sun, in demo mode (no account): Minecraft 1.21.1 (Java 21 auto-downloaded, LWJGL 3,
  OpenGL textures, sound) and rd-132211 (Java 8, LWJGL 2, xrandr); the game ran until stopped and Prism's kill
  confirmation, console and process handling worked. These ran on a private nested compositor
  (`./dev.sh launch-test`).
* Discord Rich Presence against a stand-in Discord IPC server: handshake, "In the launcher", and "Playing <profile>" /
  "<loader> <version>" during a real launch.
* Not verifiable without credentials: Microsoft sign-in and launching with an owned account.
