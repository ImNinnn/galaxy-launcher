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

#include "SystemContent.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHash>
#include <QMutex>
#include <QRegularExpression>
#include <QSet>
#include <QtConcurrent>

#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/DataPack.h"
#include "minecraft/mod/DataPackFolderModel.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ResourcePackFolderModel.h"
#include "minecraft/mod/ShaderPackFolderModel.h"
#include "minecraft/mod/TexturePackFolderModel.h"
#include "ModCategories.h"
#include "UpdateChecker.h"
#include "modplatform/ModIndex.h"

QString CelestialBody::kindName(Kind kind)
{
    switch (kind) {
        case Kind::BaseGame:
            return QObject::tr("Base game");
        case Kind::Mod:
            return QObject::tr("Mod");
        case Kind::ResourcePack:
            return QObject::tr("Resource pack");
        case Kind::TexturePack:
            return QObject::tr("Texture pack");
        case Kind::DataPack:
            return QObject::tr("Data pack");
        case Kind::ShaderPack:
            return QObject::tr("Shader pack");
    }
    return {};
}

namespace {

/**
 * Prism reports the size of unpacked (folder) resources as an item count, which is useless for sizing celestial
 * bodies. This measures folders in the background and caches the result per path and modification time.
 */
class FolderSizeCache : public QObject {
    Q_OBJECT
   public:
    static FolderSizeCache* instance()
    {
        static auto* cache = new FolderSizeCache();
        return cache;
    }

    /** Returns the cached size, or -1 and starts measuring the folder. */
    qint64 sizeOf(const QFileInfo& info)
    {
        const auto path = info.absoluteFilePath();
        const auto stamp = info.lastModified().toMSecsSinceEpoch();
        {
            QMutexLocker lock(&m_mutex);
            auto it = m_sizes.constFind(path);
            if (it != m_sizes.constEnd() && it->first == stamp) {
                return it->second;
            }
            if (m_pending.contains(path)) {
                return -1;
            }
            m_pending.insert(path);
        }
        auto* watcher = new QFutureWatcher<qint64>(this);
        connect(watcher, &QFutureWatcher<qint64>::finished, this, [this, watcher, path, stamp] {
            const qint64 size = watcher->result();
            {
                QMutexLocker lock(&m_mutex);
                m_pending.remove(path);
                m_sizes.insert(path, { stamp, size });
            }
            watcher->deleteLater();
            emit measured(path);
        });
        watcher->setFuture(QtConcurrent::run(&FolderSizeCache::measure, path));
        return -1;
    }

   signals:
    void measured(const QString& path);

   private:
    static qint64 measure(const QString& path)
    {
        // Cap the walk so a pathological folder cannot keep a worker thread busy for long.
        constexpr int maxEntries = 50000;
        qint64 total = 0;
        int entries = 0;
        QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (it.hasNext() && entries < maxEntries) {
            it.next();
            total += it.fileInfo().size();
            entries++;
        }
        return total;
    }

    QMutex m_mutex;
    QHash<QString, std::pair<qint64, qint64>> m_sizes;
    QSet<QString> m_pending;
};

QString projectUrl(const Resource& res)
{
    auto meta = res.metadata();
    if (!meta || meta->projectId.isNull() || meta->projectId.toString().isEmpty()) {
        return {};
    }
    const auto id = meta->projectId.toString();
    switch (meta->provider) {
        case ModPlatform::ResourceProvider::MODRINTH:
            // /project/ resolves every Modrinth project type (mod, shader, resource pack, data pack).
            return QStringLiteral("https://modrinth.com/project/%1").arg(id);
        case ModPlatform::ResourceProvider::FLAME:
            return QStringLiteral("https://www.curseforge.com/projects/%1").arg(id);
    }
    return {};
}

bool looksLikeShaderLoader(const Mod& mod)
{
    static const QSet<QString> ids = { "iris", "oculus", "optifine", "optifabric", "irisshaders" };
    if (ids.contains(mod.details().mod_id.toLower())) {
        return true;
    }
    static const QRegularExpression fileName(QStringLiteral("^(iris|oculus|optifine|optifabric)[-_ ]"),
                                             QRegularExpression::CaseInsensitiveOption);
    return fileName.match(mod.fileinfo().fileName()).hasMatch();
}

CelestialBody fromResource(const Resource& res, CelestialBody::Kind kind, ResourceFolderModel* model)
{
    CelestialBody body;
    body.kind = kind;
    body.internalId = res.internalId();
    body.key = QString::number(static_cast<int>(kind)) + ':' + res.internalId();
    body.name = res.name();
    body.fileName = res.fileinfo().fileName();
    body.filePath = res.fileinfo().absoluteFilePath();
    body.enabled = res.enabled();
    body.isFolder = res.type() == ResourceType::FOLDER;
    body.issues = res.issues();
    body.model = model;

    if (auto meta = res.metadata(); meta && !meta->projectId.isNull()) {
        body.hasMetadata = true;
        body.provider = ModPlatform::ProviderCapabilities::readableName(meta->provider);
        body.projectUrl = projectUrl(res);
        body.version = meta->versionNumber;
        if (meta->provider == ModPlatform::ResourceProvider::MODRINTH) {
            body.modrinthId = meta->projectId.toString();
        }
    }

    body.latestVersion = UpdateChecker::instance()->latestVersionFor(res);
    body.updateAvailable = !body.latestVersion.isEmpty();

    if (body.isFolder) {
        body.bytes = FolderSizeCache::instance()->sizeOf(res.fileinfo());
    } else {
        body.bytes = res.fileinfo().exists() ? res.fileinfo().size() : -1;
    }

    if (const auto* mod = dynamic_cast<const Mod*>(&res)) {
        if (body.version.isEmpty()) {
            body.version = mod->details().version;
        }
        body.authors = mod->authors();
        body.description = mod->description();
        body.homepage = mod->details().homeurl;
        body.isShaderLoader = looksLikeShaderLoader(*mod);
        body.modKind = ModCategories::instance()->kindOf(body.fileName, body.bytes);
    } else if (const auto* pack = dynamic_cast<const DataPack*>(&res)) {
        // Resource packs derive from DataPack.
        body.description = pack->description();
    }
    return body;
}

}  // namespace

QString SystemContent::loaderSummary(MinecraftInstance* instance)
{
    auto* profile = instance->getPackProfile();
    if (!profile) {
        return {};
    }
    struct Loader {
        const char* uid;
        const char* name;
    };
    static const Loader loaders[] = {
        { "net.fabricmc.fabric-loader", "Fabric" },   { "org.quiltmc.quilt-loader", "Quilt" },
        { "net.neoforged", "NeoForge" },              { "net.minecraftforge", "Forge" },
        { "com.mumfrey.liteloader", "LiteLoader" },   { "net.legacyfabric.legacy-fabric-loader", "Legacy Fabric" },
        { "net.ornithemc.ornithe-loader", "Ornithe" }, { "org.babricmc.babric-loader", "Babric" },
    };
    QStringList parts;
    for (const auto& loader : loaders) {
        const auto version = profile->getComponentVersion(loader.uid);
        if (!version.isEmpty()) {
            parts << QStringLiteral("%1 %2").arg(loader.name, version);
        }
    }
    return parts.join(", ");
}

void SystemContent::ensureComponentsLoaded(MinecraftInstance* instance)
{
    static QSet<QString> attempted;
    auto* profile = instance ? instance->getPackProfile() : nullptr;
    if (!profile || !profile->getComponentVersion("net.minecraft").isEmpty() || attempted.contains(instance->id())) {
        return;
    }
    attempted.insert(instance->id());
    if (auto result = profile->reload(Net::Mode::Offline); !result) {
        qWarning() << "Galaxy: could not load components of" << instance->id() << ":" << result.error();
    }
}

CelestialBody::Kind SystemContent::kindFor(ResourceFolderModel* model)
{
    if (qobject_cast<ShaderPackFolderModel*>(model)) {
        return CelestialBody::Kind::ShaderPack;
    }
    if (qobject_cast<ResourcePackFolderModel*>(model)) {
        return CelestialBody::Kind::ResourcePack;
    }
    if (qobject_cast<TexturePackFolderModel*>(model)) {
        return CelestialBody::Kind::TexturePack;
    }
    if (qobject_cast<DataPackFolderModel*>(model)) {
        return CelestialBody::Kind::DataPack;
    }
    return CelestialBody::Kind::Mod;
}

CelestialBody SystemContent::bodyFor(ResourceFolderModel* model, const Resource& resource)
{
    return fromResource(resource, kindFor(model), model);
}

QString SystemContent::loaderName(MinecraftInstance* instance)
{
    const auto summary = loaderSummary(instance);
    if (summary.isEmpty()) {
        return tr("Vanilla");
    }
    // "Fabric 0.16.5" -> "Fabric" (the first loader; versions are dropped).
    auto first = summary.section(", ", 0, 0);
    return first.left(first.lastIndexOf(' '));
}

QString SystemContent::minecraftVersion(MinecraftInstance* instance)
{
    auto* profile = instance ? instance->getPackProfile() : nullptr;
    const auto version = profile ? profile->getComponentVersion("net.minecraft") : QString();
    return version.isEmpty() ? tr("Minecraft") : tr("Minecraft %1").arg(version);
}

SystemContent::SystemContent(MinecraftInstance* instance, QObject* parent) : QObject(parent), m_instance(instance)
{
    m_rebuildTimer.setSingleShot(true);
    m_rebuildTimer.setInterval(60);
    connect(&m_rebuildTimer, &QTimer::timeout, this, &SystemContent::rebuild);
    connect(FolderSizeCache::instance(), &FolderSizeCache::measured, this, [this](const QString& path) {
        for (const auto& body : std::as_const(m_bodies)) {
            if (body.filePath == path) {
                scheduleRebuild();
                return;
            }
        }
    });
    if (m_instance) {
        connect(m_instance, &BaseInstance::propertiesChanged, this, &SystemContent::scheduleRebuild);
    }
    attachModels();
    rebuild();
    connect(ModCategories::instance(), &ModCategories::changed, this, &SystemContent::scheduleRebuild);
    connect(UpdateChecker::instance(), &UpdateChecker::updatesChanged, this, [this](const QString& id) {
        if (m_instance && id == m_instance->id()) {
            scheduleRebuild();
        }
    });
    // Look for updates in the background once the folders have been read.
    QTimer::singleShot(1500, this, [this] { UpdateChecker::instance()->check(m_instance); });
}

SystemContent::~SystemContent()
{
    detachModels();
}

bool SystemContent::isLoading() const
{
    for (const auto& attached : m_models) {
        if (attached.model && attached.model->hasPendingParseTasks()) {
            return true;
        }
    }
    return false;
}

void SystemContent::refresh()
{
    // The data pack model is recreated by Prism when its settings change; pick up the current one.
    detachModels();
    attachModels();
    for (const auto& attached : std::as_const(m_models)) {
        if (attached.model) {
            attached.model->update();
        }
    }
    scheduleRebuild();
}

void SystemContent::attachModels()
{
    if (!m_instance) {
        return;
    }
    using Kind = CelestialBody::Kind;
    attach(m_instance->loaderModList(), Kind::Mod);
    // Only touch the legacy folders when they exist: creating their models would create the folders.
    if (QDir(m_instance->coreModsDir()).exists()) {
        attach(m_instance->coreModList(), Kind::Mod);
    }
    attach(m_instance->resourcePackList(), Kind::ResourcePack);
    if (QDir(m_instance->texturePacksDir()).exists()) {
        attach(m_instance->texturePackList(), Kind::TexturePack);
    }
    attach(m_instance->shaderPackList(), Kind::ShaderPack);
    attach(m_instance->dataPackList(), Kind::DataPack);  // null unless global data packs are enabled for the profile
}

void SystemContent::attach(ResourceFolderModel* model, CelestialBody::Kind kind)
{
    if (!model) {
        return;
    }
    m_models.append({ model, kind });
    // Watching is reference counted, so this coexists with instance windows and the list view.
    model->startWatching();
    connect(model, &QAbstractItemModel::rowsInserted, this, &SystemContent::scheduleRebuild);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &SystemContent::scheduleRebuild);
    connect(model, &QAbstractItemModel::modelReset, this, &SystemContent::scheduleRebuild);
    connect(model, &QAbstractItemModel::layoutChanged, this, &SystemContent::scheduleRebuild);
    connect(model, &QAbstractItemModel::dataChanged, this, &SystemContent::scheduleRebuild);
    connect(model, &ResourceFolderModel::updateFinished, this, &SystemContent::scheduleRebuild);
    connect(model, &ResourceFolderModel::parseFinished, this, &SystemContent::scheduleRebuild);
    connect(model, &QObject::destroyed, this, &SystemContent::scheduleRebuild);
}

void SystemContent::detachModels()
{
    for (const auto& attached : std::as_const(m_models)) {
        if (attached.model) {
            disconnect(attached.model, nullptr, this, nullptr);
            attached.model->stopWatching();
        }
    }
    m_models.clear();
}

void SystemContent::scheduleRebuild()
{
    if (!m_rebuildTimer.isActive()) {
        m_rebuildTimer.start();
    }
}

void SystemContent::rebuild()
{
    QList<CelestialBody> bodies;
    if (!m_instance) {
        m_bodies = bodies;
        emit changed();
        return;
    }

    // A model can be destroyed under us (Prism resets the data pack model on settings changes); reattach then.
    bool lostModel = false;
    for (const auto& attached : std::as_const(m_models)) {
        lostModel |= attached.model.isNull();
    }
    if (lostModel) {
        detachModels();
        attachModels();
    }

    ensureComponentsLoaded(m_instance);
    CelestialBody base;
    base.kind = CelestialBody::Kind::BaseGame;
    base.key = QStringLiteral("base");
    auto* profile = m_instance->getPackProfile();
    base.name = minecraftVersion(m_instance);
    base.version = profile ? profile->getComponentVersion("net.minecraft") : QString();
    base.description = loaderSummary(m_instance);
    base.filePath = m_instance->gameRoot();
    bodies.append(base);

    auto addAll = [&bodies](ResourceFolderModel* model, CelestialBody::Kind kind) {
        if (!model) {
            return;
        }
        QList<CelestialBody> fromModel;
        for (auto* res : model->allResources()) {
            if (res) {
                fromModel.append(fromResource(*res, kind, model));
            }
        }
        // Stable order: file names do not change while Prism resolves display names in the background.
        std::sort(fromModel.begin(), fromModel.end(), [](const CelestialBody& a, const CelestialBody& b) {
            return QString::compare(a.fileName, b.fileName, Qt::CaseInsensitive) < 0;
        });
        bodies.append(fromModel);
    };

    for (const auto& attached : std::as_const(m_models)) {
        addAll(attached.model, attached.kind);
    }

    // Planets are colored by what each mod is for; look up the mods not seen before.
    QList<ModCategories::File> mods;
    for (const auto& body : std::as_const(bodies)) {
        if (body.kind == CelestialBody::Kind::Mod && !body.isFolder) {
            mods.append({ body.filePath, body.bytes, body.modrinthId });
        }
    }
    ModCategories::instance()->request(mods);

    m_bodies = bodies;
    emit changed();
}

#include "SystemContent.moc"
