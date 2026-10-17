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

#include <QList>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include "CelestialBody.h"

class MinecraftInstance;
class Resource;
class ResourceFolderModel;

/**
 * Live view of one profile's installed content.
 *
 * While attached, it keeps Prism's resource folder models for the instance watched (the same models the instance
 * window and the list view use), and re-reads them whenever they change. It holds no state of its own beyond the
 * latest snapshot, so the galaxy cannot drift away from what Prism (and the disk) say is installed.
 */
class SystemContent : public QObject {
    Q_OBJECT
   public:
    explicit SystemContent(MinecraftInstance* instance, QObject* parent = nullptr);
    ~SystemContent() override;

    MinecraftInstance* instance() const { return m_instance; }

    /** Current snapshot. The first entry is always the base game. */
    const QList<CelestialBody>& bodies() const { return m_bodies; }

    /** Whether any model is still scanning or parsing. */
    bool isLoading() const;

    /** Forces the underlying models to rescan their folders. */
    void refresh();

    /**
     * Prism loads a profile's components (Minecraft version, loaders) lazily. Loads them from disk (offline) the
     * first time they are needed, like the instance status bar does; never retried for a profile that failed.
     */
    static void ensureComponentsLoaded(MinecraftInstance* instance);

    /** "Minecraft 1.21.1", or "Minecraft" when the version is unknown. */
    static QString minecraftVersion(MinecraftInstance* instance);
    /** Installed loaders with versions, e.g. "Fabric 0.16.5". Empty for vanilla. */
    static QString loaderSummary(MinecraftInstance* instance);
    /** The installed loader's name ("Fabric", "NeoForge", …), or "Vanilla" without one. */
    static QString loaderName(MinecraftInstance* instance);

    /** A snapshot of one resource of 'model' (its kind follows from the model), e.g. for Prism's list pages. */
    static CelestialBody bodyFor(ResourceFolderModel* model, const Resource& resource);
    /** What the resources of a model are (mods, resource packs, …). */
    static CelestialBody::Kind kindFor(ResourceFolderModel* model);

   signals:
    /** Emitted (coalesced) whenever the snapshot changed. */
    void changed();

   private:
    void attachModels();
    void detachModels();
    void attach(ResourceFolderModel* model, CelestialBody::Kind kind);
    void scheduleRebuild();
    void rebuild();

    QPointer<MinecraftInstance> m_instance;
    struct AttachedModel {
        QPointer<ResourceFolderModel> model;
        CelestialBody::Kind kind;
    };
    QList<AttachedModel> m_models;
    QList<CelestialBody> m_bodies;
    QTimer m_rebuildTimer;
};
