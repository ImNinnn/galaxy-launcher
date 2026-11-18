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

#include "GravityAPI.h"

#include "Json.h"

namespace {

std::optional<ModPlatform::ResourceProvider> providerNamed(const QString& name)
{
    if (name == QLatin1String("modrinth")) {
        return ModPlatform::ResourceProvider::MODRINTH;
    }
    if (name == QLatin1String("curseforge")) {
        return ModPlatform::ResourceProvider::FLAME;
    }
    if (name == QLatin1String("gravity")) {
        return ModPlatform::ResourceProvider::GRAVITY;
    }
    return std::nullopt;
}

void readGravityFields(ModPlatform::IndexedPack& pack, const QJsonObject& obj)
{
    pack.provider = ModPlatform::ResourceProvider::GRAVITY;
    pack.selfUpdating = obj.value("self_updating").toBool(false);
}

}  // namespace

QString GravityAPI::baseUrl() const
{
    static const QString url = [] {
        auto override = qEnvironmentVariable("GALAXY_GRAVITY_API_URL");
        while (override.endsWith('/')) {
            override.chop(1);
        }
        return override.isEmpty() ? QString::fromLatin1(ProductionUrl) : override;
    }();
    return url;
}

QList<ResourceAPI::SortingMethod> GravityAPI::getSortingMethods() const
{
    return { { .index = 1, .name = "relevance", .readableName = QObject::tr("Sort by Relevance") },
             { .index = 2, .name = "downloads", .readableName = QObject::tr("Sort by Downloads") },
             { .index = 3, .name = "updated", .readableName = QObject::tr("Sort by Last Updated") },
             { .index = 4, .name = "newest", .readableName = QObject::tr("Sort by Newest") },
             { .index = 5, .name = "name", .readableName = QObject::tr("Sort by Name") } };
}

QList<ModPlatform::Category> GravityAPI::loadCategoriesFor(const QByteArray& response, const QString& projectType) const
{
    QList<ModPlatform::Category> categories;
    for (const auto& value : QJsonDocument::fromJson(response).array()) {
        const auto category = value.toObject();
        if (category.value("project_type").toString() != projectType) {
            continue;
        }
        const auto id = category.value("name").toString();
        categories.push_back({ .name = category.value("display_name").toString(id), .id = id });
    }
    return categories;
}

Result<ModPlatform::IndexedVersion> GravityAPI::parseVersion(const QJsonObject& obj)
{
    // Gravity sends "changelog": null when asked without changelogs; Modrinth leaves it out (and Prism wants a string).
    auto copy = obj;
    for (const auto* field : { "changelog", "changelog_url" }) {
        if (copy.value(field).isNull()) {
            copy.remove(field);
        }
    }
    TRY_INTO(auto version, Modrinth::loadIndexedPackVersion(copy))
    // Prism reads the dependencies in order; add where each one lives.
    const auto dependencies = obj.value("dependencies").toArray();
    for (qsizetype i = 0; i < version.dependencies.size() && i < dependencies.size(); i++) {
        const auto name = dependencies[i].toObject().value("provider").toString(QStringLiteral("gravity"));
        version.dependencies[i].provider = providerNamed(name).value_or(ModPlatform::ResourceProvider::GRAVITY);
    }
    return version;
}

Result<ModPlatform::IndexedPack> GravityAPI::parseProject(const QJsonObject& obj)
{
    ModPlatform::IndexedPack pack;
    TRY(Modrinth::Parse::loadIndexedPack(pack, obj))
    readGravityFields(pack, obj);
    return pack;
}

Result<ModPlatform::IndexedVersion> GravityAPI::loadIndexedPackVersion(QJsonObject& obj, ModPlatform::ResourceType /*type*/) const
{
    return parseVersion(obj);
}

void GravityAPI::adapt(ModPlatform::IndexedPack& pack, const QJsonObject& obj) const
{
    readGravityFields(pack, obj);
}
