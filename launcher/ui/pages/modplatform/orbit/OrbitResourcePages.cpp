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

#include "OrbitResourcePages.h"

#include "minecraft/MinecraftInstance.h"
#include "modplatform/orbit/OrbitAPI.h"
#include "ui/dialogs/ResourceDownloadDialog.h"
#include "ui/pages/modplatform/ResourceModel.h"

namespace ResourceDownload {

namespace {

/** The browser as for Modrinth, except that Orbit's files get no update metadata: Orbit updates itself. */
class OrbitModPage : public ModPage {
   public:
    using ModPage::ModPage;

   protected:
    void addResourceToPage(ModPlatform::IndexedPack::Ptr pack,
                           ModPlatform::IndexedVersion& version,
                           ResourceFolderModel* baseModel,
                           QString downloadReason,
                           QString dependentOn) override
    {
        m_model->addPack(std::move(pack), version, baseModel, false, std::move(downloadReason), std::move(dependentOn));
    }
};

}  // namespace

ModPage* Orbit::createModPage(ResourceDownloadDialog* dialog, BaseInstance& instance)
{
    ResourceProviderData provider{
        .displayName = QStringLiteral("Orbit"),
        .icon = QIcon(":/orbit/logo.png"),
        .id = Orbit::id(),
        .metaEntryBase = QStringLiteral("OrbitMods"),
        .debugName = QStringLiteral("Orbit"),
    };
    return new OrbitModPage(dialog, instance, std::move(provider), &OrbitAPI::get(),
                            ModFilterWidget::create(&static_cast<MinecraftInstance&>(instance), false));
}

}  // namespace ResourceDownload
