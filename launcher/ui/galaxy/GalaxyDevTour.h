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

class MainWindow;

/**
 * Developer tooling for headless checks (QT_QPA_PLATFORM=offscreen). Only active when GALAXY_DEV_SNAPSHOTS is set.
 *
 *  GALAXY_DEV_SNAPSHOTS=<dir>     scripted tour of the main views; screenshots are written to <dir>
 *  GALAXY_DEV_PROFILE=<id>        profile to enter (default: the first one)
 *  GALAXY_DEV_WINDOW_SIZE=WxH     window size for the screenshots
 *  GALAXY_DEV_LAUNCH=1            afterwards, launch the profile by clicking its sun, answer Prism's prompts for a
 *                                 demo launch (no account needed), wait for the game process, log its output and
 *                                 stop it again. Needs a display for the game (e.g. a nested compositor).
 *
 * The tour navigates, hovers and plays the cosmetic launch animation. Only the optional launch test starts the game.
 */
class GalaxyDevTour {
   public:
    static void start(MainWindow* window, const QString& directory);
};
