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

#include <QPointer>
#include <QString>
#include <QStringList>

#include <cstdint>

class ResourceFolderModel;

/**
 * A snapshot of one piece of installed content, as read from Prism's resource models.
 *
 * Bodies never own or cache the underlying files. Actions on a body look the resource up again in its model
 * by internal ID, so a stale snapshot can at worst fail to find its resource; it can never act on the wrong file.
 */
struct CelestialBody {
    enum class Kind : std::uint8_t {
        BaseGame,      ///< The profile's Minecraft version and loader. Real, but never a mod. Hosts packs and shaders.
        Mod,           ///< Planet.
        ResourcePack,  ///< Moon.
        TexturePack,   ///< Moon (legacy versions).
        DataPack,      ///< Moon (Prism's global data packs folder).
        ShaderPack,    ///< Ring.
    };

    Kind kind = Kind::Mod;
    QString key;         ///< Unique within a system: kind + internal ID.
    QString internalId;  ///< Prism's Resource::internalId() (the file name).

    QString name;  ///< Display name from the file's own metadata, else the file name.
    QString fileName;
    QString filePath;
    QString version;  ///< Empty when unknown.
    QStringList authors;
    QString description;

    QString provider;    ///< "Modrinth" when Prism has download metadata for the file; empty otherwise.
    QString projectUrl;  ///< Source platform page, built only from the project ID in Prism's metadata.
    QString homepage;    ///< A homepage declared inside the file itself (e.g. fabric.mod.json), if any.

    qint64 bytes = -1;  ///< On-disk size; -1 when it cannot be measured (yet).
    bool isFolder = false;
    bool enabled = true;
    bool hasMetadata = false;
    bool isShaderLoader = false;  ///< Iris / Oculus / OptiFine: shader rings attach to this planet.
    bool updateAvailable = false;  ///< Modrinth has a newer compatible version (see UpdateChecker).
    QString latestVersion;         ///< The newer version, when updateAvailable.
    QStringList issues;

    QPointer<ResourceFolderModel> model;  ///< The model the resource came from. Null for the base game.

    bool isPlanet() const { return kind == Kind::Mod || kind == Kind::BaseGame; }
    bool isMoon() const { return kind == Kind::ResourcePack || kind == Kind::TexturePack || kind == Kind::DataPack; }
    bool isRing() const { return kind == Kind::ShaderPack; }

    static QString kindName(Kind kind);
};
