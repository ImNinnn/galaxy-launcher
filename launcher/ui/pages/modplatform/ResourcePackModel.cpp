// SPDX-FileCopyrightText: 2023 flowln <flowlnlnln@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ResourcePackModel.h"

#include <QMessageBox>
#include <utility>

namespace ResourceDownload {

ResourcePackResourceModel::ResourcePackResourceModel(ResourceFolderModel* resourceList,
                                                     const ResourceAPI* api,
                                                     const QString& debugName,
                                                     QString metaEntryBase)
    : ResourceModel(resourceList, api), m_debugName(debugName + " (Model)"), m_metaEntryBase(std::move(metaEntryBase))
{}

/******** Make data requests ********/

ResourceAPI::SearchArgs ResourcePackResourceModel::createSearchArguments()
{
    auto sort = getCurrentSortingMethodByIndex();
    ResourceAPI::SearchArgs args{
        .type = ModPlatform::ResourceType::ResourcePack,
        .offset = m_nextSearchOffset,
        .search = m_searchTerm,
        .sorting = sort,
        .loaders = {},
        .versions = {},
        .side = {},
        .categoryIds = {},
        .openSource = {},
    };
    applyPackFilter(args);
    return args;
}

ResourceAPI::VersionSearchArgs ResourcePackResourceModel::createVersionsArguments(const QModelIndex& entry)
{
    auto pack = m_packs[entry.row()];
    return { .pack = pack, .mcVersions = packFilterVersions(), .loaders = {}, .resourceType = ModPlatform::ResourceType::ResourcePack };
}

void ResourcePackResourceModel::searchWithTerm(const QString& term, unsigned int sort, bool filterChanged)
{
    if (m_searchTerm == term && m_searchTerm.isNull() == term.isNull() && m_currentSortIndex == sort && !filterChanged) {
        return;
    }

    setSearchTerm(term);
    m_currentSortIndex = sort;

    refresh();
}

}  // namespace ResourceDownload
