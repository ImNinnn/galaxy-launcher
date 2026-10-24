# Galaxy Launcher

Galaxy Launcher is Prism Launcher with a different front end: your Minecraft profiles form a galaxy you can fly
through. Everything shown is read from your real installations, and every action uses Prism's own code.

![The galaxy](screenshots/galaxy.jpg)

| In the galaxy | Stands for |
| --- | --- |
| Solar system | A profile (Prism instance) |
| Sun | The profile itself: click to launch, right-click to manage |
| Blue-green planet | The profile's Minecraft version and mod loader (the base game, not a mod) |
| Planet | An installed mod; its size follows the file size and its color what the mod is for (below) |
| Moon | A resource, texture or data pack; packs orbit the base game planet |
| Ring | A shader pack; rings sit around the shader loader mod (Iris, Oculus, OptiFine), else around the base game planet. A disabled shader's ring is dashed |

Your profiles orbit a black hole at the centre of the galaxy. Click it to launch your last played profile: it sends
a beam to that system, then the usual launch plays. Right-click it to choose another profile to launch (also in
Settings › Galaxy), rename it or centre the view. Inside a system, the black hole or another of your profiles now and
then drifts by far in the background, drawn the way the galaxy shows it. Whatever you point at stops moving (a planet, a moon or ring with its planet, or a profile while the rest of
the galaxy keeps turning), so it is easy to read and click; let go and it slingshots back to its place. Every orbit
turns at its own pace. When content is added, removed or updated, the planets glide to their new orbits.

In the sidebar, a profile shows its icon only when it has one (new profiles start without; right-click › *Remove
Icon* takes one away), and a playing profile's sun fades into a sparkle of its color.

At most three planets share an orbit, so planets stay well apart; large profiles get more orbits and open framed on
their inner orbits (zoom out to see all of it).

Planet colors follow the mod's categories on Modrinth:

| Color | Mods |
| --- | --- |
| Red and orange | APIs and libraries |
| Green | Optimization |
| Blue and purple | Content and world generation |
| Yellow and gold | Client side and quality of life |
| Grey and silver | Unknown (not on Modrinth) |

Each mod gets its own shade within its color (a library can be a little pink, an optimization mod a little teal), so
together they use every color without being too dark or too bright. Each mod is looked up once and remembered, so
Modrinth is not asked again on every start.

A dashed outline marks disabled content, a "!" badge marks content with problems, and a green ring plus a "Running"
badge marks a profile that is playing.

Profiles can be put into categories: create one with the tag button next to *New profile* in the sidebar (or
right-click empty space in the galaxy › *New category…*), choosing which profiles go in it, or right-click a profile ›
*Change Category…*. The sidebar lists each category under a header that collapses and expands it (right-click the
header to rename or delete the category). Drag profiles in the sidebar to arrange them freely: onto a category's
header or between its profiles puts them in it, anywhere else (above a header, at the bottom) takes them out. Drag a
header to move a whole category. The galaxy follows the same order. In the galaxy the systems of a category gather in one area with the
category's name and a dashed outline (*Outline categories* in the galaxy's right-click menu or Settings › Galaxy turns
the outline off). Without categories, nothing changes.

## Using it

| To | Do |
| --- | --- |
| Enter a solar system | Click it, or click the profile in the sidebar |
| Go back to the galaxy | Back in the top bar, Esc, Backspace, or the mouse's back button |
| Launch a profile | Click its sun (or Launch in the top bar, or double-click it in the sidebar) |
| Manage a profile | Right-click its sun or its sidebar entry: rename, sun color, icon, category, edit, folder, duplicate, export, shortcut, delete |
| See what a body is | Hover it |
| See project details | Click it (the details link to its Modrinth page when the launcher knows the source) |
| Manage content | Right-click a planet, moon or ring: update, change version, enable/disable, show in folder, details, delete |
| See available updates | Content with a newer version on Modrinth gets a red outline (dashed when it is also disabled) and a red "!" (its name is red in the list view); right-click › *Update to …*. Content that came with a modpack has its updates locked; updating it asks to unlock it first |
| Update everything | When a profile has updates, a red download arrow appears next to the search bar (and "Update All" in the list view): it lists every update with its changelog and installs the chosen ones together. Disabled content stays disabled |
| Add content | **Add content** (bottom bar, Ctrl+Shift+D) opens Discover beside the galaxy (drag the divider to resize it; ✕ or the button again closes it): browse Modrinth by type (mods, resource packs, shader packs, data packs), use the filters (Minecraft version, categories, release type and more for every type; loader and environment for mods; shader loader for shaders), pick versions, then *Review and confirm*. Dependencies are added for you and nothing installs before you confirm. Right-clicking the base game planet opens it on a specific type. |
| Find installed content | Type in the bottom search bar: matches are highlighted (in the galaxy, profiles containing a match), the count is shown, and Enter jumps from match to match |
| Sign in with Microsoft | Sidebar › account button › Manage Accounts › Add Microsoft: your browser opens Microsoft's sign-in page, then returns to a Galaxy Launcher page on your computer |
| Switch views | The button at the bottom right (Ctrl+L) |
| Settings, profile settings and logs | They open as pages of the main window (Back, *Done* / *Close*, or the same button again returns to the galaxy or the list); opening one closes Discover |
| Zoom and pan | Mouse wheel / + and −; drag, or arrow keys. Right-click empty space › Reset view. Settings › Galaxy › Accessibility can show zoom and reset buttons at the bottom left |
| See play time | Hover a profile (or its sun): total and last session. Hover the black hole for the total of all profiles |
| Use the keyboard | Tab and Shift+Tab move between objects, Enter activates, the Menu key opens the context menu, Ctrl+F jumps to search |

![A solar system](screenshots/solar-system.jpg)

![Discover](screenshots/discover.jpg)

Launching plays a short sequence: the system collapses into its sun, and the sun explodes into a nebula in which
your mods, packs and shaders float as sparkles while you play (their names fade out when you press Launch; point
at a sparkle to see which one it is). Disabled content stays out of it: it moves away from the sun to one shared
orbit further out and circles there while you play. A disabled shader goes around a disabled planet there, or
circles on its own when there is none. When the game stops, the sparkles fly back to their orbits, the nebula
condenses into the sun again and the names fade back in. It is purely cosmetic: it never touches your files and the game
starts right away through Prism's normal launch (accounts, Java, mod loaders, errors). Searching while a profile is a
nebula turns the matching sparkles back into their planet, moon or ring for as long as the search lasts.

Notifications slide in at the top right, and every progress window (creating or copying a profile, changing a
skin, downloads) shows there instead. A launch shows its progress there (with *Skip* while it downloads), and you
get one when the game starts, stops (with how long you played) or crashes (with *Show log*), when downloads and
updates finish, when a profile has updates and when a new Galaxy Launcher is out. While the launcher is in the
background, the ones about games and downloads also appear as desktop notifications (Settings › Galaxy can turn
those off).

The window has its own minimize, maximize and close buttons in the top bar (drag empty top bar space to move it,
double-click to maximize, drag an edge to resize). Switching profiles from the sidebar flies straight from one
system to the next. With Discord running, your status shows "In the launcher" or "Playing <profile>"
with the loader and Minecraft version, the launcher's icon and your account's head.

![Launch sequence](screenshots/launch-sequence.jpg)

Large collections spread over more orbits instead of shrinking; every body stays selectable.

![A 180 mod profile](screenshots/large-modpack.jpg)

The list view shows the profile selected in the sidebar as Prism's familiar Mods, Resource Packs, Shader Packs and
Data Packs pages. Both views show the same data; a change in one appears in the other. Right-clicking a project there
opens the same menu as in the galaxy, and *Update* brings the selected project to its latest version.

![List view](screenshots/list-view.jpg)

A new installation starts with an empty galaxy and buttons to create or import a profile.

![Empty galaxy](screenshots/empty-galaxy.jpg)

## Updating Galaxy Launcher

At startup, Galaxy Launcher checks its GitHub releases. When a newer version exists, a red download arrow appears
right of the help (?) button: it shows what is new and how to update (the new installer on Windows, a one-line
command on Linux). Your profiles, accounts and settings are kept. The check can be turned off in Settings › Galaxy.

## Settings

*Settings › Galaxy* has the launch animation, reduce motion, orbit animation, background animation (the star field
drifts and moves in depth as you pan, zoom and travel), orbit paths (off by default), Discord Rich Presence,
background update checks, the system title bar instead of the launcher's window buttons, celestial quality (use
*Simple* on slow machines), star field, object size, spacing between systems, zoom speed and the startup view.
All other Prism settings, accounts and themes are where Prism has them; the *Galaxy* theme is the default and every
other theme remains available.

## Microsoft sign-in

Galaxy Launcher signs in with its own Microsoft application. Your browser opens Microsoft's sign-in page; afterwards
Microsoft sends it back to a small page the launcher serves on `http://localhost` (only reachable from your computer),
which tells you whether it worked. Accounts added with an earlier build that used another application ID need to be
added again.

Forks should use their own application: register it in Microsoft Entra (personal Microsoft accounts, platform
*Mobile and desktop applications* with the redirect URI `http://localhost`, *Allow public client flows* on), request
Minecraft API access for it from Mojang, then build with `-DLauncher_MSA_CLIENT_ID=<client ID>` or enter it in
Settings › APIs.

## For developers

* [Architecture and design decisions](ARCHITECTURE.md)
* [Flatpak packaging, permissions and API keys](../../flatpak/README.md)
