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

#include "ProfileContentsPanel.h"

#include <QLabel>
#include <QVBoxLayout>

#include "Application.h"
#include "InstanceList.h"
#include "minecraft/MinecraftInstance.h"
#include "ui/pages/BasePageProvider.h"
#include "ui/pages/instance/DataPackPage.h"
#include "ui/pages/instance/ModFolderPage.h"
#include "ui/pages/instance/ResourcePackPage.h"
#include "ui/pages/instance/ShaderPackPage.h"
#include "ui/pages/instance/TexturePackPage.h"
#include "ui/widgets/PageContainer.h"

#include <QTimer>

#include "UpdateChecker.h"

namespace {

class ContentPageProvider : public BasePageProvider {
   public:
    explicit ContentPageProvider(MinecraftInstance* instance) : m_instance(instance) {}

    QList<BasePage*> getPages() override
    {
        QList<BasePage*> pages;
        auto* mods = new ModFolderPage(m_instance, m_instance->loaderModList());
        mods->setFilter("%1 (*.zip *.jar *.litemod *.nilmod)");
        pages.append(mods);
        pages.append(new ResourcePackPage(m_instance, m_instance->resourcePackList()));
        pages.append(new ShaderPackPage(m_instance, m_instance->shaderPackList()));
        pages.append(new GlobalDataPackPage(m_instance));
        pages.append(new TexturePackPage(m_instance, m_instance->texturePackList()));
        return pages;
    }

    QString dialogTitle() override { return QObject::tr("Installed content"); }

   private:
    MinecraftInstance* m_instance;
};

}  // namespace

ProfileContentsPanel::ProfileContentsPanel(QWidget* parent) : QWidget(parent)
{
    setObjectName("galaxyContentsPanel");
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 8, 8, 8);
    m_layout->setSpacing(6);
    m_header = new QLabel(this);
    m_header->setObjectName("galaxyPanelHeader");
    m_layout->addWidget(m_header);
    m_placeholder = new QLabel(tr("Select a profile to see its installed mods, packs and shaders."), this);
    m_placeholder->setObjectName("galaxyHint");
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setWordWrap(true);
    m_layout->addWidget(m_placeholder, 1);
    setInstance(nullptr);

    // The pages hold pointers into the instance and its models: let go before Prism destroys a removed profile.
    auto* instances = APPLICATION->instances();
    connect(instances, &QAbstractItemModel::rowsAboutToBeRemoved, this, [this, instances](const QModelIndex&, int first, int last) {
        for (int row = first; row <= last; row++) {
            if (m_instance && instances->at(row) == m_instance) {
                setInstance(nullptr);
                return;
            }
        }
    });
    connect(instances, &QAbstractItemModel::modelAboutToBeReset, this, [this] { setInstance(nullptr); });
}

ProfileContentsPanel::~ProfileContentsPanel()
{
    release();
}

void ProfileContentsPanel::release()
{
    if (m_container) {
        m_container->prepareToClose();
        // Hide and detach now; the pages are deleted once control returns to the event loop.
        m_container->hide();
        m_layout->removeWidget(m_container);
        m_container->deleteLater();
        m_container = nullptr;
    }
    m_provider.reset();
    m_instance = nullptr;
}

void ProfileContentsPanel::setInstance(MinecraftInstance* instance)
{
    if (instance && instance == m_instance && m_container) {
        return;
    }
    release();
    m_instance = instance;
    m_placeholder->setVisible(!instance);
    m_header->setText(instance ? tr("Installed in %1").arg(instance->name()) : tr("Installed content"));
    if (!instance) {
        return;
    }
    m_provider = std::make_unique<ContentPageProvider>(instance);
    m_container = new PageContainer(m_provider.get(), "mods", this);
    QTimer::singleShot(1500, this, [instance = QPointer<MinecraftInstance>(instance)] { UpdateChecker::instance()->check(instance); });
    m_layout->addWidget(m_container, 1);
}
