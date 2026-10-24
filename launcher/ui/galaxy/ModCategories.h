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

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include "CelestialBody.h"
#include "tasks/Task.h"

/**
 * What kind of mod each planet stands for, so planets can be colored by kind (CelestialPainter::styleForMod).
 *
 * The kind comes from the project's Modrinth categories. A file is found on Modrinth by its project ID when Prism has
 * metadata for it, otherwise by its SHA-1. Results are saved in the launcher's cache folder, keyed by file name and
 * size, so each file is looked up once rather than on every start. Lookups run in batches of up to 100 files, two
 * requests per batch; files Modrinth does not know are asked about again after two weeks.
 */
class ModCategories : public QObject {
    Q_OBJECT
   public:
    static ModCategories* instance();

    /** The kind of a mod file (a ".disabled" suffix is ignored); Unknown until it has been looked up. */
    ModKind kindOf(const QString& fileName, qint64 size) const;

    struct File {
        QString path;
        qint64 size = -1;
        QString modrinthId;  ///< Saves hashing the file when known.
    };
    /** Looks up, in the background, the files that have not been looked up yet. */
    void request(const QList<File>& files);

    /** Modrinth categories to a kind. Library wins, then optimization, content and quality of life. */
    static ModKind kindFromCategories(const QStringList& categories);
    static QString kindName(ModKind kind);

   signals:
    /** New kinds are known; planets should be redrawn. */
    void changed();

   private:
    ModCategories();
    static QString keyFor(const QString& fileName, qint64 size);
    void load();
    void save() const;
    void startBatch();
    void lookUpVersions(QList<File> batch, QHash<QString, QString> hashes);
    void lookUpProjects(QList<File> batch, QHash<QString, QString> projects);
    void finishBatch(const QList<File>& batch, const QHash<QString, ModKind>& kinds, bool failed);

    struct Entry {
        ModKind kind = ModKind::Unknown;
        qint64 checked = 0;  ///< Seconds since the epoch.
    };
    QHash<QString, Entry> m_entries;  ///< keyFor() -> result
    QList<File> m_queue;
    QSet<QString> m_queued;  ///< keys in m_queue or the running batch
    QTimer m_batchTimer;
    Task::Ptr m_task;
    bool m_running = false;
    QDateTime m_pausedUntil;  ///< After a failed request, nothing is sent for a while.
    int m_failures = 0;
};
