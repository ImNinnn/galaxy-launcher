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

class QWidget;

/**
 * Microsoft sign-in needs an application (client) ID registered with Microsoft. Galaxy Launcher does not ship
 * Prism Launcher's, so this explains how to get one and stores it (the same setting as Settings > APIs).
 */
namespace MicrosoftLoginSetup {

/**
 * Returns true if Microsoft sign-in can be used now: either a client ID was already configured, or the user
 * entered one in the setup dialog (applied immediately, no restart needed).
 */
bool ensureClientId(QWidget* parent);

}  // namespace MicrosoftLoginSetup
