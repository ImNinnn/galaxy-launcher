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

#include <QObject>
#include <QString>
#include <QUrl>

class QWidget;

/**
 * Checks the Galaxy Launcher GitHub releases for a newer version than this one (BuildConfig.GALAXY_VERSION).
 *
 * Preview releases count too; drafts never do. Nothing is downloaded or installed automatically: when a newer
 * release exists, the main window shows an updates button, and show() explains how to get it (the installer on
 * Windows, the one-line Flatpak command on Linux).
 */
class LauncherUpdate : public QObject {
    Q_OBJECT
   public:
    struct Release {
        QString version;  ///< Without the leading "v".
        QString name;
        QString notes;  ///< Markdown.
        QUrl page;
        QUrl installer;  ///< GalaxyLauncher-Setup.exe, if the release has it.
    };

    explicit LauncherUpdate(QObject* parent = nullptr);

    /** Asks GitHub once (unless disabled in Settings > Galaxy or the build has no releases repository). */
    void check();

    /** The version this launcher reports (GALAXY_DEV_FAKE_VERSION overrides it for testing). */
    static QString currentVersion();

    /** Explains the update and offers the download (Windows) or the update command (Linux). */
    static void show(QWidget* parent, const Release& release);

   signals:
    void available(const LauncherUpdate::Release& release);
};
