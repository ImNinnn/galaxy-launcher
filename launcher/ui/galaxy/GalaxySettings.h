// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Galaxy Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QString>

class SettingsObject;

/** Galaxy-specific launcher settings. Everything else is configured through Prism's existing settings. */
namespace GalaxySettings {

inline const QString LaunchAnimation = QStringLiteral("GalaxyLaunchAnimation");  // bool
inline const QString ReducedMotion = QStringLiteral("GalaxyReducedMotion");      // bool
inline const QString OrbitAnimation = QStringLiteral("GalaxyOrbitAnimation");    // bool: planets move along their orbits
inline const QString ShowOrbits = QStringLiteral("GalaxyShowOrbitPaths");        // bool: draw orbit paths (off by default)
inline const QString BackgroundMotion = QStringLiteral("GalaxyBackgroundMotion");  // bool: drifting, parallax star field
inline const QString RenderQuality = QStringLiteral("GalaxyRenderQuality");      // int: 0 = simple, 1 = shaded, 2 = detailed
inline const QString StarfieldQuality = QStringLiteral("GalaxyStarfieldQuality");  // int: 0 = off, 1 = sparse, 2 = full
inline const QString ObjectScale = QStringLiteral("GalaxyObjectScale");          // int: percent applied to body sizes
inline const QString Spacing = QStringLiteral("GalaxySpacing");                  // int: percent applied to distances between systems
inline const QString ZoomSpeed = QStringLiteral("GalaxyZoomSpeed");              // int: percent applied to mouse wheel zooming
inline const QString DefaultView = QStringLiteral("GalaxyDefaultView");          // QString: "galaxy", "list" or "last"
inline const QString LastView = QStringLiteral("GalaxyLastView");                // QString: "galaxy" or "list"
inline const QString SidebarCollapsed = QStringLiteral("GalaxySidebarCollapsed");  // bool
inline const QString UpdateChecks = QStringLiteral("GalaxyUpdateChecks");        // bool: check Modrinth for updates in the background
inline const QString SystemTitleBar = QStringLiteral("GalaxySystemTitleBar");    // bool: use the window manager's title bar
inline const QString DiscordPresence = QStringLiteral("GalaxyDiscordPresence");  // bool: Discord Rich Presence
inline const QString BlackHoleName = QStringLiteral("GalaxyBlackHoleName");      // QString: name shown under the black hole
inline const QString BlackHoleProfile = QStringLiteral("GalaxyBlackHoleProfile");  // QString: profile the black hole launches; empty = last played
inline const QString DiscoverGeometry = QStringLiteral("GalaxyDiscoverGeometry");  // QString: saved Discover window geometry

/** Per-instance setting (stored in instance.cfg) holding the sun color as #RRGGBB, or empty for the default. */
inline const QString SunColor = QStringLiteral("GalaxySunColor");

void registerSettings(SettingsObject* settings);

/** Returns "galaxy" or "list" depending on the default/last view settings. */
QString initialView(SettingsObject* settings);

}  // namespace GalaxySettings
