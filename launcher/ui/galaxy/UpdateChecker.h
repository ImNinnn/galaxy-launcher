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
#include <QFileInfo>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>

#include <memory>

#include "QObjectPtr.h"

class MinecraftInstance;
class Resource;
class ResourceDownloadTask;
class ResourceFolderModel;

/**
 * Background update checks for installed content, so the galaxy and the list view can mark what can be updated.
 *
 * Uses Prism's Modrinth update check (the same request the "Check for updates" button makes) for every mod, resource
 * pack, shader pack and data pack that has Modrinth metadata. Nothing is downloaded or changed: results only mark
 * content. A profile is checked when it is opened, at most every 30 minutes.
 */
class UpdateChecker : public QObject {
    Q_OBJECT
   public:
    static UpdateChecker* instance();

    /** Checks the profile's content unless it was checked recently (or force is set). */
    void check(MinecraftInstance* instance, bool force = false);

    /** The newer version for this installed file, or an empty string. */
    QString latestVersionFor(const QFileInfo& file) const;
    QString latestVersionFor(const Resource& resource) const;

    /** One available update, ready to install. */
    struct PendingUpdate {
        QPointer<ResourceFolderModel> model;
        QString internalId;  ///< The installed file (Resource::internalId()).
        QString name;
        QString oldVersion;
        QString newVersion;
        QString changelog;  ///< Markdown, from Modrinth.
        bool locked = false;  ///< Came with a modpack; installing the update unlocks it.
        bool enabled = true;  ///< Disabled files stay disabled after the update.
        shared_qobject_ptr<ResourceDownloadTask> download;  ///< Replaces the old file; runs once.
    };

    /** The updates found for a profile whose files have not changed since. */
    QList<PendingUpdate> updatesFor(const QString& instanceId) const;
    /** Forgets a profile's pending updates (their download tasks were used); a forced check finds them again. */
    void discardPending(const QString& instanceId);

   signals:
    /** Results for the profile changed. */
    void updatesChanged(const QString& instanceId);

   private:
    UpdateChecker();
    static QString keyFor(const QFileInfo& file);

    QHash<QString, QString> m_latest;     ///< file key -> newer version
    QHash<QString, QList<std::pair<QString, PendingUpdate>>> m_pending;  ///< instance id -> (file key, update)
    QHash<QString, QDateTime> m_checked;  ///< instance id -> last check
    QHash<QString, std::shared_ptr<void>> m_running;  ///< instance id -> the running check (kept alive)
};
