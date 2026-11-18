#pragma once

#include "modplatform/CheckUpdateTask.h"

class ModrinthAPI;

class ModrinthCheckUpdate : public CheckUpdateTask {
    Q_OBJECT

   public:
    ModrinthCheckUpdate(QList<Resource*>& resources,
                        std::vector<Version>& mcVersions,
                        QList<ModPlatform::ModLoaderType> loadersList,
                        ResourceFolderModel* resourceModel,
                        std::vector<ModPlatform::IndexedVersionType> releaseTypes = {},
                        // Galaxy Launcher: also Gravity Studios' catalogue (GravityAPI, provider GRAVITY), which has Modrinth's shapes.
                        const ModrinthAPI* api = nullptr,
                        ModPlatform::ResourceProvider provider = ModPlatform::ResourceProvider::MODRINTH);

   public slots:
    bool abort() override;

   protected slots:
    void executeTask() override;
    void getUpdateModsForLoader(std::optional<ModPlatform::ModLoaderTypes> loader = {}, bool forceModLoaderCheck = false);
    void checkVersionsResponse(QByteArray* response, std::optional<ModPlatform::ModLoaderTypes> loader);
    void checkNextLoader();

   private:
    Task::Ptr m_job = nullptr;
    QHash<QString, Resource*> m_mappings;
    QString m_hashType;
    const ModrinthAPI* m_api;
    ModPlatform::ResourceProvider m_provider;
    int m_loaderIdx = 0;
    qsizetype m_initialSize = 0;
};
