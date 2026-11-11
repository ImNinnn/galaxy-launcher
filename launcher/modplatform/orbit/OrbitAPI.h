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

#include <QJsonObject>

#include "modplatform/ModIndex.h"
#include "modplatform/ResourceAPI.h"

/**
 * Orbit, from Orbit Cosmetics' own server: a self-updating Fabric mod that mod sites cannot list, so Discover offers
 * it as a source of its own. The server publishes the newest release for each Minecraft version ("target", such as
 * 26.3): GET /v1/mod/update?target=26.3&version=0.0.0 answers with its version, download path, size and SHA-256.
 *
 * There is one project. Its release for the profile's Minecraft version is part of the search result, so no other
 * request is needed; the download is checked against the SHA-256. Orbit needs Fabric API, which comes from Modrinth
 * through the usual dependency step. Installed files carry no update metadata: the mod updates itself.
 */
class OrbitAPI final : public ResourceAPI {
   public:
    static const OrbitAPI& get()
    {
        static const OrbitAPI s_instance;
        return s_instance;
    }

    static constexpr auto BaseUrl = "https://api.orbitcosmetics.dev";
    static constexpr auto ProjectId = "orbit";
    /** Fabric API's Modrinth project, which Orbit needs. */
    static constexpr auto FabricApiModrinthId = "P7dR8mSH";

    /** The release target for a Minecraft version: its year and drop ("26.3.1" is 26.3). */
    static QString targetFor(const QString& minecraftVersion);
    /** The update check for a target (version 0.0.0, so the newest release is always offered). */
    static QUrl releaseUrl(const QString& target);
    /**
     * The project, with the server's answer for @p target as its one version (none when there is no release), marked
     * as made for @p minecraftVersion (the profile's exact version).
     */
    static ModPlatform::IndexedPack project(const QJsonObject& answer, const QString& target, const QString& minecraftVersion);

    QList<ResourceAPI::SortingMethod> getSortingMethods() const override;
    std::pair<Task::Ptr, QByteArray*> getProjects(QStringList addonIds) const override;
    Net::RPC::Spec<ModPlatform::IndexedPack> getProject(const QString& id) const override;
    Net::RPC::Spec<QList<ModPlatform::IndexedPack>> searchProjects(const SearchArgs& args) const override;

    std::optional<QString> getVersionsURL(const VersionSearchArgs& args) const override;
    std::optional<QString> getDependencyURL(const DependencySearchArgs& args) const override;
    Result<ModPlatform::IndexedVersion> loadIndexedPackVersion(QJsonObject& obj, ModPlatform::ResourceType type) const override;
    std::pair<Task::Ptr, QByteArray*> getModCategories() const override;
    QList<ModPlatform::Category> loadModCategories(const QByteArray& response) const override;

   protected:
    QString debugName() const override { return "Orbit API"; }
};
