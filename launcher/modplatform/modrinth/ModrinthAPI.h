// SPDX-FileCopyrightText: 2022-2023 flowln <flowlnlnln@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "BuildConfig.h"
#include "Result.h"
#include "modplatform/ModIndex.h"
#include "modplatform/ResourceAPI.h"
#include "modplatform/modrinth/ModrinthPackIndex.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <utility>

/**
 * Galaxy Launcher: not final. GravityAPI reuses it for Gravity Studios' catalogue, which has Modrinth's API v2 shapes
 * at another address: baseUrl() and adapt() are its hooks; the static helpers stay Modrinth's.
 */
class ModrinthAPI : public ResourceAPI {
   public:
    static const ModrinthAPI& get()
    {
        static const ModrinthAPI s_instance;
        return s_instance;
    }

    static std::pair<Task::Ptr, QByteArray*> currentVersion(const QString& hash, const QString& hashFormat);

    static std::pair<Task::Ptr, QByteArray*> currentVersions(const QStringList& hashes, const QString& hashFormat);

    std::pair<Task::Ptr, QByteArray*> latestVersion(
        const QString& hash,
        const QString& hashFormat,
        std::optional<std::vector<Version>> mcVersions,
        std::optional<ModPlatform::ModLoaderTypes> loaders,
        std::optional<std::vector<ModPlatform::IndexedVersionType>> releaseTypes = std::nullopt) const;

    std::pair<Task::Ptr, QByteArray*> latestVersions(
        const QStringList& hashes,
        const QString& hashFormat,
        std::optional<std::vector<Version>> mcVersions,
        std::optional<ModPlatform::ModLoaderTypes> loaders,
        std::optional<std::vector<ModPlatform::IndexedVersionType>> releaseTypes = std::nullopt) const;

    std::pair<Task::Ptr, QByteArray*> getProjects(QStringList addonIds) const override;

    static QString getModpackIdFromUrl(const QUrl& url);

    std::pair<Task::Ptr, QByteArray*> getModCategories() const override;
    static QList<ModPlatform::Category> loadCategories(const QByteArray& response, const QString& projectType);
    /** Categories of a project type, from getModCategories()' response (GravityAPI reads its display names). */
    virtual QList<ModPlatform::Category> loadCategoriesFor(const QByteArray& response, const QString& projectType) const
    {
        return loadCategories(response, projectType);
    }
    /** The API's address (Modrinth's here). */
    virtual QString baseUrl() const { return BuildConfig.MODRINTH_PROD_URL; }
    QList<ModPlatform::Category> loadModCategories(const QByteArray& response) const override;

   public:
    auto getSortingMethods() const -> QList<ResourceAPI::SortingMethod> override;

   protected:
    /** Galaxy: after a project (or search hit) was parsed as Modrinth's, the subclass reads its own fields. */
    virtual void adapt(ModPlatform::IndexedPack& /*pack*/, const QJsonObject& /*obj*/) const {}

    auto getMultipleModInfoURL(const QStringList& ids) const -> QString
    {
        return baseUrl() + QString("/projects?ids=[\"%1\"]").arg(ids.join("\",\""));
    };

    auto getVersionsURL(const VersionSearchArgs& args) const -> std::optional<QString> override;

    std::optional<QString> getDependencyURL(const DependencySearchArgs& args) const override;

    Result<ModPlatform::IndexedVersion> loadIndexedPackVersion(QJsonObject& obj, ModPlatform::ResourceType /*unused*/) const override
    {
        return Modrinth::loadIndexedPackVersion(obj);
    };

   public:
    static bool validateModLoaders(ModPlatform::ModLoaderTypes loaders);

   public slots:
    Net::RPC::Spec<ModPlatform::IndexedPack> getProject(const QString& id) const override;
    Net::RPC::Spec<QList<ModPlatform::IndexedPack>> searchProjects(const SearchArgs& args) const override;

   protected:
    QUrl searchProjectsURL(const SearchArgs& args) const;
};
