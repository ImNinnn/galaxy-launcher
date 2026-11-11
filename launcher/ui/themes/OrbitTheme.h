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

#include "FusionTheme.h"

/**
 * Orbit: the Orbit Cosmetics identity, grey, black and red. The main window turns into pixel art (see GalaxyLook);
 * dialogs and settings pages only take the colors and the square shapes, without pixel fonts, icons or textures.
 */
class OrbitTheme : public FusionTheme {
   public:
    ~OrbitTheme() override = default;

    QString id() override;
    QString name() override;
    QString tooltip() override;
    bool hasStyleSheet() override;
    QString appStyleSheet() override;
    QPalette colorScheme() override;
    double fadeAmount() override;
    QColor fadeColor() override;

    /** The main window's chrome with the Orbit look (GalaxyTheme::chromeStyleSheet() picks it). */
    static QString chromeStyleSheet();
};
