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

#include "GalaxySettings.h"

#include "settings/SettingsObject.h"

namespace GalaxySettings {

void registerSettings(SettingsObject* settings)
{
    settings->registerSetting(LaunchAnimation, true);
    settings->registerSetting(ReducedMotion, false);
    settings->registerSetting(OrbitAnimation, true);
    settings->registerSetting(ShowOrbits, false);
    settings->registerSetting(BackgroundMotion, true);
    settings->registerSetting(RenderQuality, 2);
    settings->registerSetting(StarfieldQuality, 2);
    settings->registerSetting(ObjectScale, 100);
    settings->registerSetting(Spacing, 100);
    settings->registerSetting(ZoomSpeed, 100);
    settings->registerSetting(DefaultView, QStringLiteral("last"));
    settings->registerSetting(LastView, QStringLiteral("galaxy"));
    settings->registerSetting(SidebarCollapsed, false);
    settings->registerSetting(DiscoverGeometry, QString());
    settings->registerSetting(BlackHoleName, QString());
    settings->registerSetting(BlackHoleProfile, QString());
    settings->registerSetting(UpdateChecks, true);
    settings->registerSetting(SystemTitleBar, false);
    settings->registerSetting(DiscordPresence, true);
}

QString initialView(SettingsObject* settings)
{
    auto view = settings->get(DefaultView).toString();
    if (view == "last") {
        view = settings->get(LastView).toString();
    }
    return view == "list" ? QStringLiteral("list") : QStringLiteral("galaxy");
}

}  // namespace GalaxySettings
