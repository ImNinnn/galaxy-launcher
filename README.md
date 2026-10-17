<p align="center">
  <img alt="Galaxy Launcher" src="program_info/io.github.galaxylauncher.GalaxyLauncher_256.png" width="128">
</p>

<h1 align="center">Galaxy Launcher</h1>

<p align="center">
  A Minecraft launcher where your profiles form a galaxy you can fly through.<br>
  Built on top of <a href="https://github.com/PrismLauncher/PrismLauncher">Prism Launcher</a>.
</p>

![The galaxy](docs/galaxy/screenshots/galaxy.jpg)

Every profile is a solar system orbiting a black hole. Its sun launches the game, your mods are planets, resource
and data packs are moons, and shader packs are rings. Everything you see is read from your real installations, and
every action (installing, updating, launching, accounts) goes through Prism Launcher's own, proven code.

* **Launch with style**: the system collapses into its sun, which explodes into a nebula while you play.
* **Discover**: browse and install mods, resource packs, shader packs and data packs from Modrinth.
* **Updates at a glance**: outdated content is marked in red; update one project or the whole profile at once.
* **A classic list view** whenever you want it, with the same data.
* **Discord Rich Presence**, a space-purple theme, and all of Prism Launcher's features and settings.

## Install

### Windows

Download **[GalaxyLauncher-Setup.exe](https://github.com/ImNinnn/galaxy-launcher/releases/latest/)**
and run it. It installs for your user only (no administrator rights needed) and adds a Start menu shortcut.

The installer is not code-signed yet, so Windows SmartScreen may warn you: choose **More info › Run anyway**.

### Linux (Flatpak)

Run this in a terminal (no root needed; it also updates an existing installation):

```sh
bash -c "$(curl -fsSL https://raw.githubusercontent.com/ImNinnn/galaxy-launcher/main/install-linux.sh)"
```

Or download **[GalaxyLauncher.flatpak](https://github.com/ImNinnn/galaxy-launcher/releases/latest/)**
and install it with:

```sh
flatpak install --user GalaxyLauncher.flatpak
```

Flatpak must be set up first ([instructions for your distribution](https://flatpak.org/setup/)). To uninstall:
`flatpak uninstall --user io.github.galaxylauncher.GalaxyLauncher`.

## Screenshots

| A solar system | The launch nebula |
| --- | --- |
| ![A solar system](docs/galaxy/screenshots/solar-system.jpg) | ![Launch sequence](docs/galaxy/screenshots/launch-sequence.jpg) |
| **Discover** | **List view** |
| ![Discover](docs/galaxy/screenshots/discover.jpg) | ![List view](docs/galaxy/screenshots/list-view.jpg) |

The [user guide](docs/galaxy/README.md) explains everything you can do in the galaxy.

## Built on Prism Launcher

Galaxy Launcher is a modified version of [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher), itself a
fork of MultiMC. Prism Launcher does the real work: instances, accounts, Java, mod loaders, downloads and launching.
Galaxy Launcher replaces its main window with the galaxy and changes some of its pages and defaults. It is not
affiliated with or endorsed by the Prism Launcher project, Mojang or Microsoft.

Differences from Prism Launcher include:

* the galaxy view, the Discover window, update markers and the updates window, the launch nebula, Discord Rich
  Presence and the space theme;
* Modrinth only: CurseForge is not supported;
* its own name, icon and data folder, so it can be installed next to Prism Launcher.

## Building from source

* Linux (Flatpak): see [flatpak/README.md](flatpak/README.md).
* Windows: [`.github/workflows/galaxy-windows.yml`](.github/workflows/galaxy-windows.yml) builds the installer on
  GitHub Actions with Prism Launcher's build setup.
* How Galaxy Launcher is put together: [docs/galaxy/ARCHITECTURE.md](docs/galaxy/ARCHITECTURE.md).

Builds sign in with Galaxy Launcher's own Microsoft application. If you publish a fork, register your own; see
[Microsoft sign-in](docs/galaxy/README.md#microsoft-sign-in).

## Credits

* **Galaxy Launcher**: ImNinnn. Much of its code and documentation was written by Claude, an AI model made by
  Anthropic, under ImNinnn's direction and review.
* **Prism Launcher, MultiMC and their contributors**: everything underneath. See the credits in *Help › About*.

## License

Galaxy Launcher is free software under the GNU General Public License version 3, like Prism Launcher. See
[LICENSE](LICENSE) and [COPYING.md](COPYING.md) for the full terms and the licenses of the included components.
