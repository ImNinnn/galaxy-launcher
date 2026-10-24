# Galaxy Launcher architecture

Galaxy Launcher is Prism Launcher (12.0.0-dev, `develop` branch) with a different main window. All instance
management, authentication, downloads, version resolution and launching are Prism's unchanged code. This document
describes what was added and how it connects to Prism.

## Foundation (inspected in Phase 1)

| Area | Prism component used |
| --- | --- |
| Language / UI | C++23, Qt 6 Widgets (QPainter), CMake + Ninja |
| Profiles | `InstanceList` (model of `MinecraftInstance`), `InstanceProxyModel` for sorting |
| Installed content | `ResourceFolderModel` subclasses per instance: `loaderModList()`, `coreModList()`, `resourcePackList()`, `texturePackList()`, `shaderPackList()`, `dataPackList()` |
| Source metadata | Packwiz-style `.index/*.pw.toml` files read into `Resource::metadata()` (provider + project ID) |
| Search / install / change version | `ResourceDownload::ResourceDownloadDialog` (Modrinth pages; CurseForge is disabled in Galaxy Launcher, dependency resolution, version selection) |
| Launch | `Application::launch()` → `LaunchController` (account selection, Java, JVM args, libraries, assets) |
| Accounts / settings | `AccountList`, global settings dialog (`GenericPageProvider`) |
| Theming | `ThemeManager` + `ITheme` |

## What was added

All new code lives in `launcher/ui/galaxy/` unless noted.

| File | Responsibility |
| --- | --- |
| `GalaxyView` | The canvas: projection, camera, transitions, painting, hit testing, tooltips, keyboard focus, launch sequence. Emits intent signals only; never changes files. |
| `GalaxyLayout` | Pure geometry: size curves, orbit packing, galaxy spiral. Unit tested in `tests/GalaxyLayout_test.cpp`. |
| `SystemContent` | Live snapshot of one profile's content, read from Prism's resource models while the system is open. |
| `CelestialBody` | The snapshot record (name, file, size, enabled, source metadata, model pointer). |
| `CelestialPainter` | Procedural drawing: shaded spheres lit from the sun, sun corona, rings, orbits, star field. |
| `ProjectActions` | Context menus and operations on installed content, delegating to Prism's models and dialogs. |
| `DiscoverWindow` | Non-modal "Discover" window: one tab per content type, each hosting Prism's `ResourceDownloadDialog` embedded as a widget (providers, search, filters, versions, dependencies, review), plus a profile picker. |
| `ModCategories` | What each mod is for (library, optimization, content, quality of life), from its Modrinth categories, for the planet colors (`CelestialPainter::styleForMod`). Files are found by Prism's project ID or by SHA-1 (`ModrinthAPI::currentVersions`, then `getProjects`), in batches of up to 100, and saved to `cache/galaxy-mod-kinds.json` keyed by file name and size; unknown files are asked about again after two weeks, failures back off from 10 minutes to 6 hours. |
| `ProfileOrganizer` | The user's profile order (dragged in the sidebar), the order of the sidebar's top level (`GalaxySidebarOrder`: profiles outside categories and categories, freely mixed) and categories created before any profile is in them, stored as Galaxy settings (`GalaxyProfileOrder`, `GalaxyEmptyCategories`, JSON arrays); the *New category* dialog. The sidebar and the galaxy sort profiles with `sort()`. |
| `Notifications`, `NotificationArea` | Notifications: a list with progress, actions and timeouts (`track()` follows a task, `waitFor()` replaces a progress window), shown as cards at the top right of the main column that slide in and out (`QPropertyAnimation`; the area is masked to its cards). Ones marked `system` also go to the desktop while the launcher is not active: the notification portal in the Flatpak, `org.freedesktop.Notifications` on other Linux systems, a temporary tray icon elsewhere; never from the developer tour. |
| `UpdateChecker` | Background Modrinth update checks (Prism's `ModrinthCheckUpdate`) when a profile is opened, once its folders have loaded; includes files with locked updates (modpack content). Marks content in the galaxy and, through a hook in `ResourceFolderModel::ProxyModel`, in Prism's list pages. Keeps each update's ready download task (`updatesFor()`), valid while the file is unchanged. |
| `LauncherUpdate` | Startup check of the GitHub releases (`BuildConfig.GALAXY_RELEASES_REPO`, previews included) against `BuildConfig.GALAXY_VERSION`; the main window shows a red arrow and `show()` explains the update. Galaxy Launcher's version is separate from the Prism Launcher version it is built on. |
| `UpdatesDialog` | Every update of a profile with changelogs; `ProjectActions::updateAll()` runs the chosen download tasks together and re-disables files that were disabled. |
| `DiscordPresence` | Discord Rich Presence over Discord's local IPC socket (handshake, `SET_ACTIVITY`, reconnects). |
| `MicrosoftLoginSetup` | Explains how to get a Microsoft application (client) ID and stores it in Prism's `MSAClientIDOverride` setting. |
| `SunColor` | Per-profile sun colors (stored in `instance.cfg` as `GalaxySunColor`) and the color picker. |
| `GalaxySidebar`, `GalaxyTopBar`, `GalaxySearchBar` | Window chrome. Top bar buttons are bound to MainWindow's existing `QAction`s. The sidebar's `SidebarCategoryModel` lists profiles under a header per category (Prism's instance groups and their collapsed state); it keeps its own selection model in step with Prism's, and its drag and drop (`dropMimeData`) reorders profiles and moves them between categories. |
| `ProfileContentsPanel` | List view: Prism's own Mods / Resource Packs / Shader Packs / Data Packs pages for the selected profile. |
| `GalaxySettings` | Setting keys; registered in `Application.cpp`. |
| `ui/themes/GalaxyTheme` | The default "space purple" theme (other Prism themes remain selectable). |
| `ui/pages/global/GalaxyPage` | Settings > Galaxy. |

### Changes to existing Prism code (kept small)

* `ResourceFolderModel::startWatching/stopWatching` are reference counted. Before, a second watcher (e.g. the galaxy
  and an open instance window) would toggle the state and stop the other's updates. Only the first start arms the
  orphan-metadata cleanup; armed by an extra watcher it fired mid-update and deleted the new metadata.
* `ResourcePage::enablePackFilter()` + `ResourceModel::applyPackFilter()`: resource, shader and data pack pages reuse
  `ModFilterWidget` in pack mode (no loader or environment group) with their own Modrinth categories (shaders also
  list the shader loaders); the mod pages keep Prism's own filter code.
* The list view hides Prism's profile grid (the sidebar picks the profile); it stays as the selection model.
* `ExternalResourcesPage` (list view and instance window pages): `applyGalaxyActions()` trims the toolbar (no update
  checks or locks), adds *Update* and *Update All*, and right-clicking one project shows `ProjectActions`' menu.
* `BaseInstance` registers the cosmetic `GalaxySunColor` setting.
* `NewInstanceDialog::selectPage()` so "Import profile" opens on the import page.
* `Application::updateCapabilities()` never enables CurseForge; `ResourceUpdateDialog` skips CurseForge resources
  and never falls back to CurseForge; the CurseForge key field, the CurseForge export and the `curseforge://` handler
  are hidden or removed.
* `MSAStep` always signs in through the browser with a local redirect page: `QOAuthHttpServerReplyHandler` listens on
  127.0.0.1 only, advertises `http://localhost:<port>/` (the redirect URI registered for Galaxy Launcher's Entra
  application) and serves `resources/galaxy/login-callback.html`, which shows success or Microsoft's error.
* Fewer sign-in requests, so an account is not rate limited:
  * `MinecraftAccount::shouldRefresh()` reuses a token loaded from disk until it is within 12 hours of expiring.
    Such an account loads as *Online*, so restarting the launcher or starting a game makes no sign-in request.
  * The auth steps turn off `Request`'s rate-limit retry and set `NetJob::setRetryRefused(false)`: a 4xx response
    fails at once instead of being sent three times. `Request::errorString()` tells the user to wait after a 429.
  * Skin and cape changes update the account with `MinecraftAccount::refreshProfile()` (`AuthFlow::Action::Profile`:
    the profile and the skin image only) instead of a full sign-in.
  * After a refresh fails (the account is *Errored*), a launch uses the saved Minecraft token while it has more than
    10 minutes left, with a notification, instead of asking to sign in again; `shouldRefresh()` does not retry in the
    background until the token has less than an hour left (the accounts page's Refresh still does).
  * `AuthFlow::microsoftSignedIn()` lets `MSALoginDialog` stop the other flow once the browser or the code has
    signed in, so the device code is no longer polled. Closing the dialog stops both flows.
    `MSADeviceCodeStep::abort()` marks the step aborted before aborting its request; before, the aborted poll
    asked Microsoft for a new code. `GALAXY_DEV_LOGIN_TEST=2` checks that closing the dialog stops the polling.
* `ResourceFolderModel::s_updateLookup` + `ProxyModel::data()` color the names of resources with updates;
  `ResourceFolderModel::isUpdating()` lets the update check wait until a folder has loaded.
* `MainWindow` uses a frameless window: `GalaxyTopBar` has window buttons and starts a system move on drag; an
  application event filter starts system resizes near the edges (Settings › Galaxy can restore the system title bar).
* `AccountListPage` keeps *Add Microsoft* visible and runs `MicrosoftLoginSetup` when no client ID is set;
  `Application` recomputes its capabilities when settings are applied, so API keys work without a restart.
* `MainWindow`: "Change Group" is *Change Category…*, with category wording in its dialogs and in rename/delete
  (opened from a sidebar header's context menu; empty categories are renamed and deleted in `ProfileOrganizer`).
* `LaunchController`: the launch is a notification instead of progress windows (the account refresh waits in a local
  event loop; the launch task's downloads are tracked with *Skip*); the end of a game is a notification (stopped with
  the session's play time, crashed or failed with *Show log*) instead of opening the log window.
* `ProgressDialog::execWithTask()`: once the main window shows notifications, it calls `Notifications::waitFor()`
  instead of opening (same blocking call and result), so every progress window becomes a notification.
* Settings and profile windows are pages of the main window: `Application::ShowGlobalSettings()` calls
  `MainWindow::showSettingsPanel()` (a `PageContainer` with the settings pages, applied on *Done*, Back or leaving),
  and `Application::showInstanceWindow()` creates the `InstanceWindow` as a child of `MainWindow::instanceHost()`
  (`Qt::Widget`, property `galaxyEmbedded`, not counted as an open window). While a dialog is open over the main
  window they still open as windows. Closing the main window closes (and saves) them first.
* `DiscoverWindow::setHost()`: Discover opens in a splitter beside the views (`m_discoverSplit`) instead of a window.
* `MainWindow`: no status bar (`setStatusBarVisibility()` always hides it); play time is in the galaxy's tooltips.
  Installing, updating and *Update All* run in the background with notifications (`ProjectActions::runWithNotification`).
* `MainWindow` hosts the chrome, routes launches through `launchInstance()` (adds the cosmetic animation) and
  refactors the list view's rename handling into `applyInstanceRename()` so the galaxy can reuse it.
* Branding: app ID `io.github.galaxylauncher.GalaxyLauncher`, binary `galaxylauncher`, data folder `GalaxyLauncher`,
  default theme, logo. Prism's API credentials and updater repository are not reused (see `flatpak/README.md`).

## Data flow

```
InstanceList ──rows/data changed──▶ GalaxyView::rebuildSystems()  (names, colors, order; debounced 50 ms)
             └──────────────────────▶ GalaxySidebar (shares the proxy model and selection model with the list view)

per-profile folders ──background scan──▶ overview summary (counts, largest mods; galaxy view only)

ResourceFolderModels ──signals──▶ SystemContent::rebuild() (debounced 60 ms) ──changed──▶ GalaxyView::relayoutSystem()
        ▲                                                                                      (animated, per system)
        └── ProjectActions / list view pages / instance window all modify content through the same models
```

* The galaxy keeps no installed-project state of its own. `CelestialBody` is a snapshot; every action looks the
  resource up again in its model by internal ID, so a stale snapshot can only fail to find a file, never act on the
  wrong one.
* The overview (all systems) uses a cheap directory listing per profile in a worker thread instead of loading every
  profile's models, which would parse every jar of every profile at startup.
* Unpacked (folder) resource and shader packs are measured in a worker thread (`FolderSizeCache`), capped at 50,000
  files.

## Visual rules

* **Projection**: orthographic at a 45° elevation. A point on the orbit plane `(x, z)` at height `y` maps to screen
  `(x, z·sin45° − y·cos45°)·zoom`; depth is `z·cos45° + y·sin45°`. Spheres stay circles and keep their size at a
  given zoom; orbits become ellipses. Objects are drawn back to front and hit-tested front to back from the same
  shapes recorded while painting, so hit testing, tooltips and visuals cannot drift apart.
* **Metaphor**: sun = profile, planet = mod, moon = resource/texture/data pack, ring = shader pack.
* **Hosts**: packs are not tied to any particular mod, so their moons orbit a *base game planet* that represents the
  profile's real Minecraft version and loader (it is labelled "Base game (not a mod)"). Shader rings attach to the
  first enabled shader-loader mod (Iris, Oculus, OptiFine) when one is installed, otherwise to the base game planet.
* **Sizes** (log scale, local units where the sun's radius is 1):

  | Body | Smallest | Largest | Size range mapped |
  | --- | --- | --- | --- |
  | Planet radius | 0.17 | 0.60 | 16 KiB – 64 MiB |
  | Moon radius | 0.06 | 0.20, and at most 55 % of its planet | 16 KiB – 256 MiB |
  | Ring band width | 0.035 | 0.14 | 16 KiB – 64 MiB |

  The base game planet has a fixed radius (0.5). **Fallback**: if a size cannot be measured (unreadable file, folder
  still being measured), the body is drawn at 35 % of the range and its tooltip says "Size unknown".
  Settings > Galaxy > Celestial object size scales all bodies; relative sizes are kept.
* **Orbits** hold at most three planets (`MaxPlanetsPerOrbit`), spread evenly; large collections get more orbits
  (spacing proportional to each planet's extent including moons and rings), so bodies never overlap or shrink below
  their minimum. Systems with more than eight orbits open framed on the inner eight (`viewExtent`); zooming out
  reaches the whole system. Planet names are shown wherever they fit; users can zoom (wheel, +/−) and pan (drag,
  arrow keys).
* **Non-color cues**: disabled content is desaturated *and* has a dashed outline and "Disabled" in its tooltip;
  problems show a "!" badge; selection uses an outline plus a side bar in the sidebar; running profiles have a pulsing
  ring plus a "Running" text badge; search matches get an outline and a label.

## Black hole, galactic rotation and the nebula

* Systems are laid out on the sunflower spiral, pushed out by `BlackHoleClearance` so the black hole at the origin has
  room. The galaxy turns as a whole (`galacticSpeed`, one turn about every five minutes); a rigid rotation keeps the
  no-overlap guarantee that differential rotation would break.
* Hovering pauses motion per object: planets and moons keep a per-node paused time (a moon or ring pauses its
  planet too), capped so a stopped body never reaches a neighbour on its orbit; once the cursor leaves, an
  underdamped spring (`springBack()`) slingshots it back, a little past its place, and settles it. Each orbit's speed
  is scaled by `GalaxyLayout::orbitSpeedFactor()` (stable per profile and orbit; also the galaxy's mini systems). After
  a relayout, bodies glide to their new orbits on a critically damped spring (`glide()`) that keeps its speed when
  relayouts follow each other, and hover lag carries over. Disabled content on its shared orbit pauses the same
  way, and that orbit is drawn (with orbit paths on) while the system is a nebula; a hovered profile pauses its own mini planets and falls behind the galaxy's rotation
  (`SystemNode::lag`), slingshotting back afterwards.
* Switching between entered systems is a direct flight (`GalaxyView::Switch`, `switchCamera()`): the camera moves from
  one system to the other at the systems' scale (barely zooming out, no galaxy labels or category areas on the way),
  bending around the black hole when the straight line would cross it; farther systems take a little longer. The old
  system fades out in the first half and the new one (loaded when the flight starts) in during the second. Because the rotation is rigid, only the lagging profile moves
  relative to the others, and `systemLagAllowed()` refuses any lag that would bring it too close to another one.
* `GalaxyView::syncNebulae()` follows each profile's running state. When the launch pull-in completes (or a game is
  started elsewhere), a `NebulaState` records when the nebula formed (exploding out of the sun or from the orbits);
  when the game stops, it reforms over 1.8 s and is removed. Bodies blend between their orbit position and a fixed,
  hashed sparkle position; the sun fades into, and condenses out of, the clouds.
* Project names follow `m_labelFade`, which eases towards `labelTarget()`: hidden from the moment Launch is pressed
  while the profile launches or runs, shown again once it reforms.
* Sparkle hit shapes have `priority`: `hitTest()` picks the nearest sparkle under the cursor before testing anything
  else, so the sun's larger hit area never hides a sparkle.
* A black hole click (`launchFromBlackHole()`) draws a beam to `blackHoleTarget()` (the chosen profile, else the last
  played) and emits `launchRequested` when it arrives, so the normal launch and its animation follow.
* Disabled content is exiled during a launch and the nebula: instead of the pull-in and the sparkles it moves to one
  shared orbit at 1.3× the system's extent and returns on reform. `exilePlan()` gives disabled planets and moons a
  place on that orbit; a ring goes with its planet when that is disabled, a disabled ring otherwise goes around
  another disabled planet (or moon), keeping its gap to the surface, and orbits alone when there is none.
* Profile categories: `GalaxyLayout::layoutGalaxyClusters()` lays out each category as a small sunflower of its own
  and places the categories (larger first) and the systems without one on the galaxy's spiral, first fit without
  overlap. `paintClusters()` draws each category's area and name, and the dashed outline unless
  `GalaxyCategoryOutlines` is off. Without categories it is `layoutGalaxy()`.
* `m_passer`: in system view, every 45–150 s another profile or the black hole crosses the background (purely
  cosmetic, not clickable), drawn with `paintMiniSystem()`, the galaxy view's mini system. It is painted with the system layer's opacity, so it fades out when leaving the system.

## Animation and performance

* One `QTimer` drives frames, only while something moves: transitions, tweens and the launch sequence at ~60 fps,
  slow orbital motion at ~30 fps. Orbital motion and running-profile pulses stop when the window is inactive or minimized (no background
  animation while playing) and are disabled entirely with *Reduce motion*.
* Animation uses elapsed time, not frame counts, and clamps long stalls.
* The background is three cached layers: sky and nebulae (rendered with a margin), and two seamlessly tileable
  star layers. They shift by different amounts with the camera (parallax: panning, zooming, flying into systems)
  and drift slowly on their own; a faint shooting star crosses every ~37 s. *Animate the background* and *Reduce
  motion* control it.
* Frames run only while the launcher is in front. Resuming after the window regains focus listens to the window's
  activation signal and the application state, with a 1 s watchdog as a safety net (activation events are not
  delivered reliably to child widgets on every platform).
* Content changes relayout only the affected system; bodies glide from their old position (`NodeState` offsets that
  decay to zero) and new content fades in.
* Shaded spheres are rendered once per (style, half-pixel radius, 7.5° light direction, lighting level) into a sprite
  cache and blitted afterwards. A 180-mod system paints in about 5 ms per frame at 1150×780 with software rendering
  (about 30 ms without the cache).
* Quality levels: 0 = flat shapes (fallback for slow machines), 1 = shaded, 2 = surface detail and contact shadows.

## Launch sequence

`MainWindow::launchInstance()` calls `Application::launch()` first: Prism's pipeline starts immediately and handles
accounts, Java, errors and prompts. Only if Prism accepted the launch does `GalaxyView::playLaunchAnimation()` play
(1.6 s: the sun brightens, bodies are pulled in with trails, the sun flashes, the system reassembles). With
*Reduce motion* it is a short sun pulse; it can be turned off. The animation never touches files and never delays or
gates the launch; if launching fails, Prism's error dialogs appear and the galaxy is already back to normal.

## Developer workflow

See `flatpak/README.md`. Short version:

```sh
cd flatpak
./dev.sh deps        # once
./dev.sh configure   # once
./dev.sh build       # incremental
./dev.sh test        # unit tests
./dev.sh snapshots dev-data/testdata /tmp/shots   # headless UI tour with screenshots
```

`GALAXY_DEV_SNAPSHOTS=<dir>` (optional `GALAXY_DEV_PROFILE=<instance id>`, `GALAXY_DEV_WINDOW_SIZE=WxH`) runs a
scripted tour through the main views, menus and dialogs and writes screenshots (`ui/galaxy/GalaxyDevTour`). It only
navigates, hovers, opens and dismisses dialogs, and plays the cosmetic animation. With `GALAXY_DEV_LAUNCH=1`
(`./dev.sh launch-test`) it then launches the profile for real by clicking its sun, answers Prism's prompts for a
demo launch, waits for the game process, prints evidence from the game log, and stops the game. The game runs on a
private `kwin_wayland --virtual` compositor, not on the desktop. `INSTALLED=1` runs both against the installed
Flatpak instead of the development build.

## Known limitations

* Data packs are per world in Minecraft. The galaxy shows Prism's optional profile-wide data pack folder (enabled per
  profile on its Data Packs page); per-world data packs are managed from the Worlds page.
* The overview shows up to 11 mods per system (the largest) as a summary; the full content appears when entering.
* Clicking a body opens its project page only when Prism has source metadata (installed through the launcher or
  matched by Prism's metadata tools); local files show their details instead.
