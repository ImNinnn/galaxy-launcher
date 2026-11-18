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

#include "modplatform/modrinth/ModrinthAPI.h"

/**
 * Gravity Studios' catalogue (Orbit, its Modrinth/CurseForge build, and the studio's packs, shaders and data packs),
 * served by the Orbit backend with Modrinth's API v2 shapes under /v1/catalog. So this is ModrinthAPI at another
 * address, with three additions read here: projects can be "self_updating" (Galaxy then keeps no update metadata),
 * dependencies name their "provider" (Gravity, Modrinth or CurseForge), and categories have display names.
 * The address is GALAXY_GRAVITY_API_URL when set (development), otherwise the production catalogue.
 */
class GravityAPI final : public ModrinthAPI {
   public:
    static const GravityAPI& get()
    {
        static const GravityAPI s_instance;
        return s_instance;
    }

    static constexpr auto ProductionUrl = "https://api.orbitcosmetics.dev/v1/catalog";

    QString baseUrl() const override;
    QList<ResourceAPI::SortingMethod> getSortingMethods() const override;
    QList<ModPlatform::Category> loadCategoriesFor(const QByteArray& response, const QString& projectType) const override;
    Result<ModPlatform::IndexedVersion> loadIndexedPackVersion(QJsonObject& obj, ModPlatform::ResourceType type) const override;

    /** A version as Gravity sends it, with its dependencies' providers (public for tests). */
    static Result<ModPlatform::IndexedVersion> parseVersion(const QJsonObject& obj);
    /** A project or search hit as Gravity sends it (public for tests). */
    static Result<ModPlatform::IndexedPack> parseProject(const QJsonObject& obj);

   protected:
    void adapt(ModPlatform::IndexedPack& pack, const QJsonObject& obj) const override;
    QString debugName() const override { return "Gravity Studios API"; }
};
