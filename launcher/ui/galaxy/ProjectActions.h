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

#include <QPoint>
#include <QString>

#include <cstdint>

#include "CelestialBody.h"

class MinecraftInstance;
class QWidget;
class ResourceFolderModel;

namespace ResourceDownload {
class ResourceDownloadDialog;
}

/**
 * Operations on installed content, triggered from the galaxy.
 *
 * Every operation goes through Prism's resource models and download dialogs, exactly like the instance window's
 * pages, so the files on disk, Prism's metadata index and every open view stay consistent.
 */
namespace ProjectActions {

/** What to search for and install. Order matches the search bar's category box. */
enum class Category : std::uint8_t { Mods, ResourcePacks, ShaderPacks, DataPacks };

/** Left click: the details dialog (which links to the project's Modrinth page). Never installs anything. */
void activate(QWidget* parent, MinecraftInstance* instance, const CelestialBody& body);

/** Right click menu with only the actions that apply to the body's kind. */
void showContextMenu(QWidget* parent, MinecraftInstance* instance, const CelestialBody& body, const QPoint& globalPos);

void showDetails(QWidget* parent, MinecraftInstance* instance, const CelestialBody& body);
void toggleEnabled(QWidget* parent, MinecraftInstance* instance, const CelestialBody& body);
void remove(QWidget* parent, MinecraftInstance* instance, const CelestialBody& body);
void changeVersion(QWidget* parent, MinecraftInstance* instance, const CelestialBody& body);
/** Updates one project through Prism's update dialog (review, dependencies, confirmation). */
void update(QWidget* parent, MinecraftInstance* instance, const CelestialBody& body);
/** Shows every update found for the profile (UpdatesDialog) and installs the chosen ones together. */
void updateAll(QWidget* parent, MinecraftInstance* instance);

/**
 * Opens the Discover window (Modrinth browser) for the profile and category, optionally pre-filled with
 * a search. Results are only installed after the user selects versions and confirms.
 */
void openDownloadDialog(QWidget* parent, MinecraftInstance* instance, Category category, const QString& searchTerm = QString());

/** Runs the downloads of an accepted download dialog with a progress dialog, then rescans the model. */
void runDownloads(QWidget* parent, ResourceDownload::ResourceDownloadDialog* dialog, ResourceFolderModel* model);

}  // namespace ProjectActions
