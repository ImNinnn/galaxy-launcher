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

#include "UpdateChecker.h"

#include <QPointer>

#include <memory>
#include <vector>

#include "Application.h"
#include "GalaxySettings.h"
#include "Version.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/DataPackFolderModel.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ResourceFolderModel.h"
#include "minecraft/mod/ResourcePackFolderModel.h"
#include "minecraft/mod/ShaderPackFolderModel.h"
#include "ResourceDownloadTask.h"
#include "modplatform/modrinth/ModrinthCheckUpdate.h"
#include "settings/SettingsObject.h"
#include "tasks/SequentialTask.h"

namespace {

/** Everything one check needs to stay alive while it runs (Prism's task keeps references to these lists). */
struct Job {
    QPointer<ResourceFolderModel> model;
    QList<Resource::Ptr> keepAlive;  // the model may replace its resources meanwhile; these stay valid
    QList<Resource*> resources;
    std::vector<Version> versions;
    shared_qobject_ptr<ModrinthCheckUpdate> task;
};

}  // namespace

UpdateChecker* UpdateChecker::instance()
{
    static auto* checker = new UpdateChecker();
    return checker;
}

UpdateChecker::UpdateChecker()
{
    // Prism's list pages ask this hook to color resources with updates (see ResourceFolderModel::ProxyModel).
    ResourceFolderModel::s_updateLookup = [](const Resource& resource) { return UpdateChecker::instance()->latestVersionFor(resource); };
}

QString UpdateChecker::keyFor(const QFileInfo& file)
{
    // Includes size and modification time: once a file is updated or replaced, the old result no longer applies.
    return QStringLiteral("%1|%2|%3").arg(file.absoluteFilePath()).arg(file.size()).arg(file.lastModified().toMSecsSinceEpoch());
}

QString UpdateChecker::latestVersionFor(const QFileInfo& file) const
{
    return m_latest.value(keyFor(file));
}

QString UpdateChecker::latestVersionFor(const Resource& resource) const
{
    return latestVersionFor(resource.fileinfo());
}

QList<UpdateChecker::PendingUpdate> UpdateChecker::updatesFor(const QString& instanceId) const
{
    QList<PendingUpdate> result;
    for (const auto& [key, update] : m_pending.value(instanceId)) {
        // Skip files that were changed, updated or removed since the check.
        if (!update.model || !update.download) {
            continue;
        }
        const auto resource = update.model->find(update.internalId);
        if (resource && keyFor(resource->fileinfo()) == key) {
            result.append(update);
        }
    }
    return result;
}

void UpdateChecker::discardPending(const QString& instanceId)
{
    if (m_pending.remove(instanceId) > 0) {
        emit updatesChanged(instanceId);
    }
}

void UpdateChecker::check(MinecraftInstance* instance, bool force)
{
    if (!instance || !APPLICATION->settings()->get(GalaxySettings::UpdateChecks).toBool() ||
        APPLICATION->settings()->get("ModMetadataDisabled").toBool()) {
        return;
    }
    const QString id = instance->id();
    if (m_running.contains(id)) {
        return;
    }
    if (!force && m_checked.contains(id) && m_checked[id].secsTo(QDateTime::currentDateTimeUtc()) < 30 * 60) {
        return;
    }
    auto* profile = instance->getPackProfile();
    auto minecraft = profile ? profile->getComponent("net.minecraft") : nullptr;
    if (!minecraft || minecraft->getVersion().isEmpty()) {
            return;
    }

    struct Source {
        ResourceFolderModel* model;
        QList<ModPlatform::ModLoaderType> loaders;
    };
    QList<Source> sources;
    sources.append(Source{ instance->loaderModList(), profile->getModLoadersList() });
    sources.append(Source{ instance->resourcePackList(), {} });
    sources.append(Source{ instance->shaderPackList(), {} });
    if (auto* dataPacks = instance->dataPackList()) {
        sources.append(Source{ dataPacks, { ModPlatform::ModLoaderType::DataPack } });
    }

    // Large folders take a while to load (with their Modrinth metadata); checking earlier would find nothing.
    for (const auto& source : sources) {
        if (source.model && source.model->isUpdating()) {
            connect(source.model, &ResourceFolderModel::updateFinished, this,
                    [this, instance = QPointer<MinecraftInstance>(instance), force] { check(instance, force); },
                    Qt::SingleShotConnection);
            return;
        }
    }

    auto jobs = std::make_shared<QList<std::shared_ptr<Job>>>();
    auto sequence = makeShared<SequentialTask>(tr("Checking for updates"));
    for (const auto& source : sources) {
        if (!source.model) {
            continue;
        }
        auto job = std::make_shared<Job>();
        job->model = source.model;
        for (auto* resource : source.model->allResources()) {
            // Only Modrinth: Galaxy Launcher does not use CurseForge. Files with locked updates (modpack content) are
            // checked too, so their updates show; updating one asks to unlock it first.
            if (!resource || !resource->metadata() || resource->type() == ResourceType::FOLDER ||
                resource->metadata()->provider != ModPlatform::ResourceProvider::MODRINTH) {
                continue;
            }
            if (auto ptr = source.model->find(resource->internalId())) {
                job->keepAlive.append(ptr);
                job->resources.append(ptr.get());
            }
        }
            if (job->resources.isEmpty()) {
            continue;
        }
        job->versions = { Version(minecraft->getVersion()) };
        job->task.reset(new ModrinthCheckUpdate(job->resources, job->versions, source.loaders, source.model));
        sequence->addTask(job->task);
        jobs->append(job);
    }
    if (jobs->isEmpty()) {
        m_checked[id] = QDateTime::currentDateTimeUtc();
        return;
    }

    m_running.insert(id, std::make_shared<Task::Ptr>(sequence));
    connect(sequence.get(), &Task::finished, this, [this, id, jobs] {
        // Release the finished task once its signal handlers have returned.
        QMetaObject::invokeMethod(this, [this, id] { m_running.remove(id); }, Qt::QueuedConnection);
        m_checked[id] = QDateTime::currentDateTimeUtc();
        auto& pending = m_pending[id];
        pending.clear();
        for (const auto& job : *jobs) {
            // Forget earlier results for these files, then record the new ones.
            for (auto* resource : job->resources) {
                m_latest.remove(keyFor(resource->fileinfo()));
            }
            for (const auto& update : job->task->getUpdates()) {
                for (auto* resource : job->resources) {
                    const auto meta = resource->metadata();
                    if (meta && (meta->hash == update.oldHash || meta->name == update.name)) {
                        const auto key = keyFor(resource->fileinfo());
                        m_latest.insert(key, update.newVersion);
                        // Without a version in the metadata, the version the file declares itself.
                        QString installed = meta->versionNumber;
                        if (const auto* mod = dynamic_cast<const Mod*>(resource); installed.isEmpty() && mod) {
                            installed = mod->details().version;
                        }
                        pending.append({ key, PendingUpdate{ job->model, resource->internalId(), update.name, installed, update.newVersion,
                                                             update.changelog, resource->lockUpdate(), resource->enabled(),
                                                             update.download } });
                        break;
                    }
                }
            }
            if (job->model && job->model->rowCount() > 0) {
                // Let open list views recolor their rows.
                emit job->model->dataChanged(job->model->index(0, 0),
                                             job->model->index(job->model->rowCount() - 1, job->model->columnCount() - 1));
            }
        }
        emit updatesChanged(id);
    });
    sequence->start();
}
