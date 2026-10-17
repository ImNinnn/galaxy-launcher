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

#include <QColor>
#include <QList>

class BaseInstance;
class QWidget;

/** Profile sun colors. They are purely cosmetic and live in the launcher's instance.cfg, never in game files. */
namespace SunColor {

/** A small palette of star-like colors used for defaults and as picker presets. */
const QList<QColor>& presets();

/** The color a profile gets until the user picks one, derived from its stable instance ID. */
QColor defaultFor(const QString& instanceId);

/** The user's chosen color, or the default if none was chosen or the stored value is invalid. */
QColor get(BaseInstance* instance);

/** Whether the user has explicitly picked a color for this profile. */
bool isCustom(BaseInstance* instance);

/** Stores the color. An invalid color resets the profile to its default. */
void set(BaseInstance* instance, const QColor& color);

/**
 * Shows a color picker with a live preview and the preset palette.
 * Returns true and updates the instance if the user accepted a color (or chose to reset).
 */
bool pick(BaseInstance* instance, QWidget* parent);

}  // namespace SunColor
