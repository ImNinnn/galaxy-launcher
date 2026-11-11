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

#include "OrbitAPI.h"

#include <QJsonDocument>
#include <QUrlQuery>

#include "Application.h"
#include "net/ApiRequest.h"
#include "net/NetJob.h"

namespace {

/** The newest Minecraft version Orbit is built for, when a search does not name one. */
constexpr auto DefaultTarget = "26.3";

Result<QJsonObject> parseAnswer(const QByteArray& response)
{
    QJsonParseError error{};
    const auto document = QJsonDocument::fromJson(response, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::unexpected(QStringLiteral("Orbit's server sent an unreadable answer: %1").arg(error.errorString()));
    }
    return document.object();
}

}  // namespace

QString OrbitAPI::targetFor(const QString& minecraftVersion)
{
    const auto parts = minecraftVersion.split('.');
    return parts.size() >= 2 ? parts[0] + '.' + parts[1] : minecraftVersion;
}

QUrl OrbitAPI::releaseUrl(const QString& target)
{
    QUrl url(QStringLiteral("%1/v1/mod/update").arg(BaseUrl));
    QUrlQuery query;
    query.addQueryItem("target", target);
    query.addQueryItem("version", "0.0.0");
    url.setQuery(query);
    return url;
}

ModPlatform::IndexedPack OrbitAPI::project(const QJsonObject& answer, const QString& target, const QString& minecraftVersion)
{
    ModPlatform::IndexedPack pack;
    pack.addonId = QString::fromLatin1(ProjectId);
    pack.provider = ModPlatform::ResourceProvider::ORBIT;
    pack.resourceType = ModPlatform::ResourceType::Mod;
    pack.name = QStringLiteral("Orbit");
    pack.slug = QStringLiteral("orbit");
    pack.authors = { ModPlatform::ModpackAuthor{ QStringLiteral("ImNinnn"), QStringLiteral("https://github.com/ImNinnn") } };
    pack.logoName = QStringLiteral("orbit-icon.png");
    pack.logoUrl = QStringLiteral("https://orbitcosmetics.dev/assets/img/icon-180.png");
    pack.websiteUrl = QStringLiteral("https://orbitcosmetics.dev");
    pack.side = ModPlatform::SideType::ClientSide;
    pack.description = QObject::tr(
        "Wear your own Blockbench cosmetics, emotes and body parts that every Orbit player sees on any server, chat with friends "
        "(end-to-end encrypted), host your worlds for them, manage and edit your screenshots, and zoom.");
    const auto update = answer.value("update").toObject();
    pack.extraData.body = QObject::tr(
                              "<p>%1</p><p>Orbit updates itself, so it comes straight from Orbit's own server instead of a mod site. "
                              "It needs Fabric and Fabric API (added for you from Modrinth).</p>%2")
                              .arg(pack.description.toHtmlEscaped(),
                                   update.isEmpty() ? QObject::tr("<p><b>No release for Minecraft %1 yet.</b> Orbit is made for "
                                                                  "Minecraft 26.1 to 26.3.</p>")
                                                          .arg(minecraftVersion.toHtmlEscaped())
                                                    : QObject::tr("<p>Newest release for Minecraft %1: <b>%2</b>.</p>")
                                                          .arg(target.toHtmlEscaped(), update.value("version").toString().toHtmlEscaped()));
    pack.extraDataLoaded = true;
    pack.versionsLoaded = true;
    if (!update.isEmpty()) {
        auto copy = update;
        copy.insert("target", target);
        copy.insert("minecraft", minecraftVersion);
        if (auto version = OrbitAPI::get().loadIndexedPackVersion(copy, ModPlatform::ResourceType::Mod); version) {
            pack.versions = { *version };
        }
    }
    return pack;
}

QList<ResourceAPI::SortingMethod> OrbitAPI::getSortingMethods() const
{
    return { { .index = 0, .name = "default", .readableName = QObject::tr("Orbit") } };
}

std::pair<Task::Ptr, QByteArray*> OrbitAPI::getProjects(QStringList /*addonIds*/) const
{
    auto job = makeShared<NetJob>(QStringLiteral("Orbit::GetProjects"), APPLICATION->network());
    auto [action, response] = Net::ApiRequest::makeByteArray(releaseUrl(DefaultTarget));
    job->addNetAction(action);
    return { job, response };
}

Net::RPC::Spec<ModPlatform::IndexedPack> OrbitAPI::getProject(const QString& /*id*/) const
{
    const QString target = DefaultTarget;
    return { { .url = releaseUrl(target) }, [target](const QByteArray& response) -> Result<ModPlatform::IndexedPack> {
                TRY_INTO(auto answer, parseAnswer(response))
                return project(answer, target, target);
            } };
}

Net::RPC::Spec<QList<ModPlatform::IndexedPack>> OrbitAPI::searchProjects(const SearchArgs& args) const
{
    const QString minecraft = args.versions.has_value() && !args.versions->empty() ? args.versions->front().toString() : QString(DefaultTarget);
    const QString target = targetFor(minecraft);
    const bool fabric = !args.loaders.has_value() || args.loaders->testFlag(ModPlatform::Fabric);
    const int offset = args.offset;
    return { { .url = releaseUrl(target) },
             [target, minecraft, fabric, offset](const QByteArray& response) -> Result<QList<ModPlatform::IndexedPack>> {
                 if (offset > 0) {
                     return QList<ModPlatform::IndexedPack>{};  // one project: there is no next page
                 }
                 TRY_INTO(auto answer, parseAnswer(response))
                 auto pack = project(answer, target, minecraft);
                 if (!fabric) {
                     pack.versions.clear();
                     pack.extraData.body += QObject::tr("<p><b>Orbit is a Fabric mod:</b> install Fabric in this profile to use it.</p>");
                 }
                 return QList<ModPlatform::IndexedPack>{ pack };
             } };
}

std::optional<QString> OrbitAPI::getVersionsURL(const VersionSearchArgs& /*args*/) const
{
    return {};  // the versions come with the project
}

std::optional<QString> OrbitAPI::getDependencyURL(const DependencySearchArgs& /*args*/) const
{
    return {};  // Fabric API is resolved through Modrinth
}

Result<ModPlatform::IndexedVersion> OrbitAPI::loadIndexedPackVersion(QJsonObject& obj, ModPlatform::ResourceType /*type*/) const
{
    const auto version = obj.value("version").toString();
    const auto path = obj.value("url").toString();
    const auto target = obj.value("target").toString();
    if (version.isEmpty() || !path.startsWith('/')) {
        return std::unexpected(QStringLiteral("Orbit release without a version or download path"));
    }
    ModPlatform::IndexedVersion out;
    out.addonId = QString::fromLatin1(ProjectId);
    out.fileId = QStringLiteral("%1/%2").arg(version, target);
    out.version = QStringLiteral("Orbit %1").arg(version);
    out.versionNumber = version;
    out.versionType = ModPlatform::IndexedVersionType(ModPlatform::IndexedVersionType::Release);
    // The release's target ("26.3") and the profile's exact version ("26.3.1"), which the version filter compares.
    out.mcVersion = { target };
    if (const auto minecraft = obj.value("minecraft").toString(); !minecraft.isEmpty() && minecraft != target) {
        out.mcVersion.prepend(minecraft);
    }
    out.downloadUrl = QString::fromLatin1(BaseUrl) + path;
    out.fileName = QStringLiteral("orbit-%1+%2.jar").arg(version, target);
    out.loaders = ModPlatform::Fabric;
    out.hashType = QStringLiteral("sha256");
    out.hash = obj.value("sha256").toString();
    out.side = ModPlatform::SideType::ClientSide;
    out.dependencies = { ModPlatform::Dependency{ .addonId = QString::fromLatin1(FabricApiModrinthId),
                                                  .type = ModPlatform::DependencyType(ModPlatform::DependencyType::REQUIRED),
                                                  .version = QString() } };
    return out;
}

std::pair<Task::Ptr, QByteArray*> OrbitAPI::getModCategories() const
{
    // Orbit has no categories; the request only completes the page's usual start-up.
    return getProjects({});
}

QList<ModPlatform::Category> OrbitAPI::loadModCategories(const QByteArray& /*response*/) const
{
    return {};
}
