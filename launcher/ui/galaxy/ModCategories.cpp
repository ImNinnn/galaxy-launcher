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

#include "ModCategories.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QtConcurrent>

#include "Application.h"
#include "FileSystem.h"
#include "modplatform/modrinth/ModrinthAPI.h"

namespace {

constexpr int BatchSize = 100;
constexpr qint64 UnknownRecheckSeconds = 14LL * 24 * 3600;
constexpr qint64 KnownRecheckSeconds = 90LL * 24 * 3600;

QString cachePath()
{
    return FS::PathCombine(APPLICATION->dataRoot(), "cache", "galaxy-mod-kinds.json");
}

QString storedName(ModKind kind)
{
    switch (kind) {
        case ModKind::Library:
            return QStringLiteral("library");
        case ModKind::Optimization:
            return QStringLiteral("optimization");
        case ModKind::Content:
            return QStringLiteral("content");
        case ModKind::Utility:
            return QStringLiteral("utility");
        case ModKind::Unknown:
            break;
    }
    return QStringLiteral("unknown");
}

ModKind kindFromStoredName(const QString& name)
{
    for (const auto kind : { ModKind::Library, ModKind::Optimization, ModKind::Content, ModKind::Utility }) {
        if (storedName(kind) == name) {
            return kind;
        }
    }
    return ModKind::Unknown;
}

QStringList toStringList(const QJsonValue& value)
{
    QStringList out;
    for (const auto& item : value.toArray()) {
        out << item.toString();
    }
    return out;
}

}  // namespace

ModCategories* ModCategories::instance()
{
    static auto* categories = new ModCategories();
    return categories;
}

ModCategories::ModCategories()
{
    m_batchTimer.setSingleShot(true);
    m_batchTimer.setInterval(800);
    connect(&m_batchTimer, &QTimer::timeout, this, &ModCategories::startBatch);
    load();
}

QString ModCategories::keyFor(const QString& fileName, qint64 size)
{
    const QString name = fileName.endsWith(".disabled") ? fileName.chopped(9) : fileName;
    return QStringLiteral("%1|%2").arg(name).arg(size);
}

ModKind ModCategories::kindOf(const QString& fileName, qint64 size) const
{
    return m_entries.value(keyFor(fileName, size)).kind;
}

ModKind ModCategories::kindFromCategories(const QStringList& categories)
{
    static const QSet<QString> content = { "adventure",  "cursed",  "decoration", "economy",    "equipment",      "food",     "game-mechanics",
                                           "magic",      "minigame", "mobs",      "storage",    "technology",     "transportation", "worldgen" };
    static const QSet<QString> utility = { "utility", "management", "social" };
    if (categories.contains("library")) {
        return ModKind::Library;
    }
    if (categories.contains("optimization")) {
        return ModKind::Optimization;
    }
    for (const auto& category : categories) {
        if (content.contains(category)) {
            return ModKind::Content;
        }
    }
    for (const auto& category : categories) {
        if (utility.contains(category)) {
            return ModKind::Utility;
        }
    }
    return ModKind::Unknown;
}

QString ModCategories::kindName(ModKind kind)
{
    switch (kind) {
        case ModKind::Library:
            return tr("API or library");
        case ModKind::Optimization:
            return tr("Optimization");
        case ModKind::Content:
            return tr("Content and world generation");
        case ModKind::Utility:
            return tr("Client and quality of life");
        case ModKind::Unknown:
            break;
    }
    return tr("Not categorized");
}

void ModCategories::request(const QList<File>& files)
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    bool added = false;
    for (const auto& file : files) {
        if (file.size < 0) {
            continue;
        }
        const auto key = keyFor(QFileInfo(file.path).fileName(), file.size);
        if (m_queued.contains(key)) {
            continue;
        }
        if (auto it = m_entries.constFind(key); it != m_entries.constEnd()) {
            if (now - it->checked < (it->kind == ModKind::Unknown ? UnknownRecheckSeconds : KnownRecheckSeconds)) {
                continue;
            }
        }
        m_queued.insert(key);
        m_queue.append(file);
        added = true;
    }
    if (added && !m_running && !m_batchTimer.isActive()) {
        m_batchTimer.start();
    }
}

void ModCategories::startBatch()
{
    if (m_running || m_queue.isEmpty()) {
        return;
    }
    const auto now = QDateTime::currentDateTimeUtc();
    if (m_pausedUntil.isValid() && now < m_pausedUntil) {
        m_batchTimer.start(static_cast<int>(now.msecsTo(m_pausedUntil)) + 100);
        return;
    }
    m_running = true;
    const QList<File> batch = m_queue.mid(0, BatchSize);
    m_queue.remove(0, batch.size());

    // Files without a project ID are found by their SHA-1, hashed off the GUI thread.
    auto* watcher = new QFutureWatcher<QHash<QString, QString>>(this);
    connect(watcher, &QFutureWatcher<QHash<QString, QString>>::finished, this, [this, watcher, batch] {
        const auto hashes = watcher->result();
        watcher->deleteLater();
        lookUpVersions(batch, hashes);
    });
    watcher->setFuture(QtConcurrent::run([batch] {
        QHash<QString, QString> hashes;  // path -> SHA-1
        for (const auto& file : batch) {
            if (!file.modrinthId.isEmpty()) {
                continue;
            }
            QFile in(file.path);
            QCryptographicHash hash(QCryptographicHash::Sha1);
            if (in.open(QIODevice::ReadOnly) && hash.addData(&in)) {
                hashes.insert(file.path, QString::fromLatin1(hash.result().toHex()));
            }
        }
        return hashes;
    }));
}

void ModCategories::lookUpVersions(QList<File> batch, QHash<QString, QString> hashes)
{
    QHash<QString, QString> projects;  // path -> project ID
    for (const auto& file : batch) {
        if (!file.modrinthId.isEmpty()) {
            projects.insert(file.path, file.modrinthId);
        }
    }
    if (hashes.isEmpty()) {
        lookUpProjects(batch, projects);
        return;
    }
    auto [task, response] = ModrinthAPI::currentVersions(hashes.values(), "sha1");
    m_task = task;
    auto* raw = task.get();
    connect(raw, &Task::finished, this, [this, raw, response, batch, hashes, projects]() mutable {
        if (!raw->wasSuccessful()) {
            finishBatch(batch, {}, true);
            return;
        }
        const auto versions = QJsonDocument::fromJson(*response).object();
        for (auto it = hashes.constBegin(); it != hashes.constEnd(); ++it) {
            const auto project = versions.value(it.value()).toObject().value("project_id").toString();
            if (!project.isEmpty()) {
                projects.insert(it.key(), project);
            }
        }
        lookUpProjects(batch, projects);
    });
    task->start();
}

void ModCategories::lookUpProjects(QList<File> batch, QHash<QString, QString> projects)
{
    QStringList ids;
    for (const auto& id : std::as_const(projects)) {
        if (!ids.contains(id)) {
            ids << id;
        }
    }
    if (ids.isEmpty()) {
        finishBatch(batch, {}, false);
        return;
    }
    auto [task, response] = ModrinthAPI::get().getProjects(ids);
    m_task = task;
    auto* raw = task.get();
    connect(raw, &Task::finished, this, [this, raw, response, batch, projects] {
        if (!raw->wasSuccessful()) {
            finishBatch(batch, {}, true);
            return;
        }
        QHash<QString, ModKind> byProject;
        for (const auto& value : QJsonDocument::fromJson(*response).array()) {
            const auto project = value.toObject();
            ModKind kind = kindFromCategories(toStringList(project.value("categories")));
            if (kind == ModKind::Unknown) {
                kind = kindFromCategories(toStringList(project.value("additional_categories")));
            }
            byProject.insert(project.value("id").toString(), kind);
        }
        QHash<QString, ModKind> kinds;  // path -> kind
        for (auto it = projects.constBegin(); it != projects.constEnd(); ++it) {
            kinds.insert(it.key(), byProject.value(it.value(), ModKind::Unknown));
        }
        finishBatch(batch, kinds, false);
    });
    task->start();
}

void ModCategories::finishBatch(const QList<File>& batch, const QHash<QString, ModKind>& kinds, bool failed)
{
    m_task.reset();
    m_running = false;
    if (failed) {
        // Offline or refused: try again later, waiting longer after each failure (10 minutes up to 6 hours).
        m_failures = std::min(m_failures + 1, 6);
        m_pausedUntil = QDateTime::currentDateTimeUtc().addSecs(std::min<qint64>(600LL << (m_failures - 1), 6 * 3600));
        m_queue = batch + m_queue;
        m_batchTimer.start(static_cast<int>(QDateTime::currentDateTimeUtc().msecsTo(m_pausedUntil)) + 100);
        return;
    }
    m_failures = 0;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (const auto& file : batch) {
        const auto key = keyFor(QFileInfo(file.path).fileName(), file.size);
        m_entries.insert(key, Entry{ kinds.value(file.path, ModKind::Unknown), now });
        m_queued.remove(key);
    }
    save();
    emit changed();
    if (!m_queue.isEmpty()) {
        m_batchTimer.start();
    }
}

void ModCategories::load()
{
    QFile file(cachePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const auto files = QJsonDocument::fromJson(file.readAll()).object().value("files").toObject();
    for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
        const auto entry = it.value().toObject();
        m_entries.insert(it.key(), Entry{ kindFromStoredName(entry.value("kind").toString()),
                                          static_cast<qint64>(entry.value("checked").toDouble()) });
    }
}

void ModCategories::save() const
{
    QJsonObject files;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        files.insert(it.key(), QJsonObject{ { "kind", storedName(it->kind) }, { "checked", static_cast<double>(it->checked) } });
    }
    QDir().mkpath(QFileInfo(cachePath()).absolutePath());
    QSaveFile out(cachePath());
    if (out.open(QIODevice::WriteOnly)) {
        out.write(QJsonDocument(QJsonObject{ { "version", 1 }, { "files", files } }).toJson(QJsonDocument::Compact));
        out.commit();
    }
}
