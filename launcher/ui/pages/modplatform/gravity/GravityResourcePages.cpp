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

#include "GravityResourcePages.h"

#include <QMessageBox>

#include "Application.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/mod/ModFolderModel.h"
#include "modplatform/gravity/GravityAPI.h"
#include "settings/SettingsObject.h"
#include "ui/dialogs/ResourceDownloadDialog.h"
#include "ui/pages/modplatform/ResourceModel.h"

namespace ResourceDownload {

namespace {

ResourceProviderData prepareGravity()
{
    return {
        .displayName = QStringLiteral("Gravity Studios"),
        .icon = QIcon(":/galaxy/gravity-studios.png"),
        .id = Gravity::id(),
        .metaEntryBase = QStringLiteral("GravityPacks"),
        .debugName = QStringLiteral("Gravity"),
    };
}

/** Orbit's mod ID: the self-updating build and the store build are the same mod, and Fabric refuses both at once. */
constexpr auto OrbitModId = "orbit_cosmetic";

/**
 * The mod page, with two Gravity rules: a self-updating project (Orbit) gets no update metadata, so Galaxy never
 * manages its updates; and Orbit is not added twice under its two projects.
 */
class GravityModPage : public ModPage {
   public:
    using ModPage::ModPage;

   protected:
    void addResourceToPage(ModPlatform::IndexedPack::Ptr pack,
                           ModPlatform::IndexedVersion& version,
                           ResourceFolderModel* baseModel,
                           QString downloadReason,
                           QString dependentOn) override
    {
        if (pack->slug.startsWith(QLatin1String("orbit")) && !confirmSecondOrbit(*pack)) {
            return;
        }
        const bool indexed = !pack->selfUpdating && !APPLICATION->settings()->get("ModMetadataDisabled").toBool();
        m_model->addPack(std::move(pack), version, baseModel, indexed, std::move(downloadReason), std::move(dependentOn));
    }

   private:
    bool confirmSecondOrbit(const ModPlatform::IndexedPack& pack)
    {
        auto* instance = dynamic_cast<MinecraftInstance*>(&m_baseInstance);
        if (!instance) {
            return true;
        }
        QString installed;
        for (auto* resource : instance->loaderModList()->allResources()) {
            if (auto* mod = dynamic_cast<Mod*>(resource); mod && mod->details().mod_id == QLatin1String(OrbitModId)) {
                installed = mod->fileinfo().fileName();
            }
        }
        for (const auto& task : m_model->selectedPacks()) {
            if (task->getPack()->slug.startsWith(QLatin1String("orbit")) && task->getPack()->addonId != pack.addonId) {
                installed = task->getName();
            }
        }
        if (installed.isEmpty()) {
            return true;
        }
        return QMessageBox::warning(this, tr("Orbit is already there"),
                                    tr("This profile already has Orbit (%1). %2 is the same mod, and Fabric does not start with both: "
                                       "remove the other one first. Add %2 anyway?")
                                        .arg(installed, pack.name),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
    }
};

}  // namespace

ModPage* Gravity::createModPage(ResourceDownloadDialog* dialog, BaseInstance& instance)
{
    return new GravityModPage(dialog, instance, prepareGravity(), &GravityAPI::get(),
                              ModFilterWidget::create(&static_cast<MinecraftInstance&>(instance), true));
}

ResourcePackResourcePage* Gravity::createResourcePackResourcePage(ResourceDownloadDialog* dialog, BaseInstance& instance)
{
    auto* page = new ResourcePackResourcePage(dialog, instance, prepareGravity(), &GravityAPI::get());
    page->enablePackFilter(ModFilterWidget::create(&static_cast<MinecraftInstance&>(instance), true), "resourcepack");
    return page;
}

ShaderPackResourcePage* Gravity::createShaderPackResourcePage(ResourceDownloadDialog* dialog, BaseInstance& instance)
{
    auto* page = new ShaderPackResourcePage(dialog, instance, prepareGravity(), &GravityAPI::get());
    page->enablePackFilter(ModFilterWidget::create(&static_cast<MinecraftInstance&>(instance), true), "shader",
                           { { .name = "Iris", .id = "iris" }, { .name = "OptiFine", .id = "optifine" }, { .name = "Canvas", .id = "canvas" } });
    return page;
}

DataPackResourcePage* Gravity::createDataPackResourcePage(ResourceDownloadDialog* dialog, BaseInstance& instance)
{
    auto* page = new DataPackResourcePage(dialog, instance, prepareGravity(), &GravityAPI::get());
    page->enablePackFilter(ModFilterWidget::create(&static_cast<MinecraftInstance&>(instance), true), "datapack");
    return page;
}

}  // namespace ResourceDownload
