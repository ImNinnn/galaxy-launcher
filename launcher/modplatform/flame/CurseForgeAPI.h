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

#include "modplatform/flame/FlameAPI.h"

/**
 * Galaxy Launcher's CurseForge search, so finding a project feels like on Modrinth.
 *
 * CurseForge's API has no relevance sort: Prism asked for "Featured" results (unrelated projects first) and sent the
 * search text unencoded. This searches by popularity with the text encoded, keeps only the requested kind of project
 * (never modpacks among mods), and orders the results by how well the name matches the search ("Relevance", the
 * default): an exact name or slug first, then names starting with the text, then names containing all of its words,
 * popularity breaking ties. Versions, files, dependencies and updates are CurseForge's as in Prism (FlameAPI).
 */
class CurseForgeAPI final : public FlameAPI {
   public:
    static const CurseForgeAPI& get()
    {
        static const CurseForgeAPI s_instance;
        return s_instance;
    }

    QList<ResourceAPI::SortingMethod> getSortingMethods() const override;
    Net::RPC::Spec<QList<ModPlatform::IndexedPack>> searchProjects(const SearchArgs& args) const override;

    /** The search request (public for tests). */
    static QUrl searchUrl(const SearchArgs& args);
    /** How well a project's name, slug and summary match the search text; higher is better (public for tests). */
    static int relevance(const QString& search, const QString& name, const QString& slug, const QString& summary);

    /** Index of the "Relevance" sorting; the others are CurseForge's sort fields. */
    static constexpr unsigned int RelevanceSort = 0;
    /** Results per page: more than Prism's 25, so the right project is on the first page to be ranked. */
    static constexpr int PageSize = 50;
};
