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

#include "DiscoverWindow.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QTabBar>
#include <QVBoxLayout>

#include "Application.h"
#include "GalaxySettings.h"
#include "InstanceList.h"
#include "SunColor.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/DataPackFolderModel.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ResourcePackFolderModel.h"
#include "minecraft/mod/ShaderPackFolderModel.h"
#include "settings/SettingsObject.h"
#include "ui/dialogs/InstallLoaderDialog.h"
#include "ui/dialogs/ResourceDownloadDialog.h"
#include "ui/pages/modplatform/ResourcePage.h"
#include "ui/themes/GalaxyTheme.h"

using Category = ProjectActions::Category;

QPointer<DiscoverWindow> DiscoverWindow::s_window;

namespace {

QIcon sunIcon(const QColor& color)
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(QRectF(2, 2, 12, 12));
    return QIcon(pixmap);
}

QString categoryNoun(Category category)
{
    switch (category) {
        case Category::Mods:
            return DiscoverWindow::tr("mods");
        case Category::ResourcePacks:
            return DiscoverWindow::tr("resource packs");
        case Category::ShaderPacks:
            return DiscoverWindow::tr("shader packs");
        case Category::DataPacks:
            return DiscoverWindow::tr("data packs");
    }
    return {};
}

}  // namespace

void DiscoverWindow::showFor(QWidget* parent, MinecraftInstance* instance, Category category, const QString& search)
{
    if (!s_window) {
        s_window = new DiscoverWindow(parent ? parent->window() : nullptr);
    }
    s_window->setTarget(instance, category, search);
    s_window->show();
    s_window->raise();
    s_window->activateWindow();
}

DiscoverWindow::DiscoverWindow(QWidget* parent) : QDialog(parent, Qt::Window)
{
    setObjectName("galaxyDiscoverWindow");
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Discover"));
    setWindowIcon(QIcon::fromTheme("new"));
    setModal(false);
    setMinimumSize(980, 620);
    resize(1180, 780);
    if (const auto geometry = APPLICATION->settings()->get(GalaxySettings::DiscoverGeometry).toString(); !geometry.isEmpty()) {
        restoreGeometry(QByteArray::fromBase64(geometry.toUtf8()));
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header: where things get installed, and what kind of content to look for.
    auto* header = new QWidget(this);
    header->setObjectName("galaxyTopBar");
    header->setAttribute(Qt::WA_StyledBackground);
    header->setStyleSheet(GalaxyTheme::chromeStyleSheet());
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 8, 14, 8);
    headerLayout->setSpacing(10);
    auto* title = new QLabel(tr("Discover"), header);
    title->setObjectName("galaxyBrand");
    m_tabs = new QTabBar(header);
    m_tabs->setObjectName("galaxyDiscoverTabs");
    m_tabs->setDrawBase(false);
    m_tabs->setExpanding(false);
    // Order matches ProjectActions::Category.
    m_tabs->addTab(QIcon::fromTheme("loadermods"), tr("Mods"));
    m_tabs->addTab(QIcon::fromTheme("resourcepacks"), tr("Resource packs"));
    m_tabs->addTab(QIcon::fromTheme("shaderpacks"), tr("Shader packs"));
    m_tabs->addTab(QIcon::fromTheme("datapacks"), tr("Data packs"));
    m_tabs->setAccessibleName(tr("Content type"));
    auto* into = new QLabel(tr("Install into"), header);
    into->setObjectName("galaxyLocation");
    m_profile = new QComboBox(header);
    m_profile->setObjectName("galaxySearchCategory");
    m_profile->setMinimumWidth(220);
    m_profile->setAccessibleName(tr("Profile to install into"));
    headerLayout->addWidget(title);
    headerLayout->addSpacing(12);
    headerLayout->addWidget(m_tabs);
    headerLayout->addStretch(1);
    headerLayout->addWidget(into);
    headerLayout->addWidget(m_profile);
    layout->addWidget(header);

    m_host = new QWidget(this);
    m_hostLayout = new QVBoxLayout(m_host);
    m_hostLayout->setContentsMargins(8, 8, 8, 4);
    layout->addWidget(m_host, 1);

    m_status = new QLabel(this);
    m_status->setObjectName("galaxyHint");
    m_status->setContentsMargins(14, 4, 14, 8);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    reloadProfiles();

    connect(m_tabs, &QTabBar::currentChanged, this, [this](int index) {
        if (m_switching) {
            return;
        }
        if (!confirmDiscard()) {
            m_switching = true;
            m_tabs->setCurrentIndex(static_cast<int>(m_category));
            m_switching = false;
            return;
        }
        m_category = static_cast<Category>(index);
        rebuild();
    });
    connect(m_profile, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_switching) {
            return;
        }
        auto* instance = APPLICATION->instances()->getInstanceById(m_profile->itemData(index).toString());
        if (instance == m_instance) {
            return;
        }
        if (!confirmDiscard()) {
            m_switching = true;
            m_profile->setCurrentIndex(m_profile->findData(m_instance ? m_instance->id() : QString()));
            m_switching = false;
            return;
        }
        m_instance = instance;
        rebuild();
    });
    auto* instances = APPLICATION->instances();
    connect(instances, &QAbstractItemModel::rowsInserted, this, &DiscoverWindow::reloadProfiles);
    connect(instances, &QAbstractItemModel::dataChanged, this, &DiscoverWindow::reloadProfiles);
    connect(instances, &QAbstractItemModel::rowsAboutToBeRemoved, this, [this, instances](const QModelIndex&, int first, int last) {
        for (int row = first; row <= last; row++) {
            if (instances->at(row) == m_instance) {
                // The profile is going away: drop the browser before its models are destroyed.
                m_instance = nullptr;
                rebuild();
            }
        }
    });
    connect(instances, &QAbstractItemModel::rowsRemoved, this, &DiscoverWindow::reloadProfiles);
}

void DiscoverWindow::reloadProfiles()
{
    auto* list = APPLICATION->instances();
    QList<MinecraftInstance*> instances;
    for (int i = 0; i < list->count(); i++) {
        instances.append(list->at(i));
    }
    std::sort(instances.begin(), instances.end(),
              [](MinecraftInstance* a, MinecraftInstance* b) { return QString::localeAwareCompare(a->name(), b->name()) < 0; });
    m_switching = true;
    m_profile->clear();
    for (auto* instance : instances) {
        m_profile->addItem(sunIcon(SunColor::get(instance)), instance->name(), instance->id());
    }
    m_profile->setCurrentIndex(m_instance ? m_profile->findData(m_instance->id()) : -1);
    m_switching = false;
}

void DiscoverWindow::setTarget(MinecraftInstance* instance, Category category, const QString& search)
{
    const bool same = instance == m_instance && category == m_category && m_current;
    if (same && search.isEmpty()) {
        return;
    }
    if (!confirmDiscard()) {
        return;
    }
    m_instance = instance;
    m_category = category;
    m_switching = true;
    m_tabs->setCurrentIndex(static_cast<int>(category));
    m_profile->setCurrentIndex(instance ? m_profile->findData(instance->id()) : -1);
    m_switching = false;
    rebuild(search);
}

bool DiscoverWindow::confirmDiscard()
{
    if (!m_dialog || m_dialog->getTasks().isEmpty()) {
        return true;
    }
    const auto reply = QMessageBox::question(this, tr("Discard selection?"),
                                             tr("You selected %n project(s) that are not installed yet. Discard the selection?", "",
                                                static_cast<int>(m_dialog->getTasks().size())),
                                             QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
    return reply == QMessageBox::Discard;
}

void DiscoverWindow::showPlaceholder(const QString& text, const QString& buttonText, const std::function<void()>& action)
{
    auto* placeholder = new QWidget(m_host);
    auto* layout = new QVBoxLayout(placeholder);
    layout->addStretch();
    auto* label = new QLabel(text, placeholder);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignCenter);
    label->setMaximumWidth(560);
    layout->addWidget(label, 0, Qt::AlignHCenter);
    if (!buttonText.isEmpty()) {
        auto* button = new QPushButton(buttonText, placeholder);
        button->setObjectName("galaxyPrimaryButton");
        button->setStyleSheet(GalaxyTheme::chromeStyleSheet());
        connect(button, &QPushButton::clicked, this, action);
        layout->addSpacing(10);
        layout->addWidget(button, 0, Qt::AlignHCenter);
    }
    layout->addStretch();
    m_current = placeholder;
    m_hostLayout->addWidget(placeholder);
}

void DiscoverWindow::rebuild(const QString& search)
{
    if (m_current) {
        m_current->hide();
        m_hostLayout->removeWidget(m_current);
        m_current->deleteLater();
        m_current = nullptr;
    }
    m_dialog = nullptr;

    auto* instance = m_instance.data();
    if (!instance) {
        setWindowTitle(tr("Discover"));
        m_status->clear();
        showPlaceholder(APPLICATION->instances()->count() > 0 ? tr("Choose a profile to install into.")
                                                              : tr("Create a profile first: content is always installed into a profile."),
                        QString(), {});
        return;
    }
    setWindowTitle(tr("Discover %1 for %2").arg(categoryNoun(m_category), instance->name()));

    using Dialog = ResourceDownload::ResourceDownloadDialog;
    Dialog* dialog = nullptr;
    ResourceFolderModel* model = nullptr;
    switch (m_category) {
        case Category::Mods: {
            if (!instance->getPackProfile()->getModLoaders().has_value()) {
                m_status->clear();
                showPlaceholder(tr("\"%1\" has no mod loader, so it cannot run mods yet.\n\n"
                                   "Install Fabric, Quilt, Forge or NeoForge first. Resource packs, shader packs and data packs work "
                                   "without one.")
                                    .arg(instance->name()),
                                tr("Install a mod loader…"), [this, instance] {
                                    InstallLoaderDialog dialog(instance->getPackProfile(), QString(), this);
                                    dialog.exec();
                                    rebuild();
                                });
                return;
            }
            model = instance->loaderModList();
            dialog = Dialog::createMod(m_host, model, instance);
            break;
        }
        case Category::ResourcePacks:
            model = instance->resourcePackList();
            dialog = Dialog::createResourcePack(m_host, model, instance);
            break;
        case Category::ShaderPacks:
            model = instance->shaderPackList();
            dialog = Dialog::createShaderPack(m_host, model, instance);
            break;
        case Category::DataPacks:
            model = instance->dataPackList();
            if (!model) {
                m_status->clear();
                showPlaceholder(tr("Minecraft keeps data packs per world, which you manage from a world's folder.\n\n"
                                   "Prism can also keep one data pack folder for the whole profile, loaded into every world by mods "
                                   "such as Global Packs, Paxi or Open Loader. It is turned off for \"%1\".")
                                    .arg(instance->name()),
                                tr("Use a profile-wide data pack folder"), [this, instance] {
                                    instance->settings()->set("GlobalDataPacksEnabled", true);
                                    rebuild();
                                });
                return;
            }
            dialog = Dialog::createDataPack(m_host, model, instance);
            break;
    }
    if (!dialog) {
        return;
    }

    // Embed Prism's browser as a page of this window.
    dialog->setWindowFlags(Qt::Widget);
    dialog->setWindowModality(Qt::NonModal);
    if (auto* buttons = dialog->findChild<QDialogButtonBox*>()) {
        if (auto* cancel = buttons->button(QDialogButtonBox::Cancel)) {
            cancel->hide();  // the window has its own close button
        }
    }
    if (dialog->getPages().isEmpty()) {
        dialog->deleteLater();
        showPlaceholder(tr("No source offers %1 for this profile's Minecraft version and loader.").arg(categoryNoun(m_category)),
                        QString(), {});
        return;
    }
    for (auto* page : dialog->getPages()) {
        if (auto* resourcePage = dynamic_cast<ResourceDownload::ResourcePage*>(page); resourcePage && !search.isEmpty()) {
            resourcePage->setSearchTerm(search);
            QMetaObject::invokeMethod(resourcePage, "triggerSearch", Qt::QueuedConnection);
        }
    }
    dialog->installEventFilter(this);
    m_dialog = dialog;
    m_current = dialog;
    m_hostLayout->addWidget(dialog);
    dialog->show();

    m_status->setText(
        tr("Projects come from Modrinth. Select projects (and versions) in the list, then press \"Review and confirm\": nothing "
           "is installed before you confirm. Use the filter button next to the search field for Minecraft version, loader, "
           "categories and more."));

    QPointer<ResourceFolderModel> guardedModel(model);
    connect(dialog, &QDialog::finished, this, [this, dialog, guardedModel](int result) {
        if (result != 0) {
            const auto count = dialog->getTasks().size();
            ProjectActions::runDownloads(this, dialog, guardedModel);
            m_status->setText(tr("Installed %n project(s) into %1. Search for more, or close this window.", "", static_cast<int>(count))
                                  .arg(m_instance ? m_instance->name() : QString()));
            const auto status = m_status->text();
            QMetaObject::invokeMethod(this, [this, status] {
                rebuild();
                m_status->setText(status);
            }, Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(this, [this] { rebuild(); }, Qt::QueuedConnection);
        }
    });
}

bool DiscoverWindow::eventFilter(QObject* watched, QEvent* event)
{
    // Escape inside the embedded browser closes this window (after the discard check) instead of resetting it.
    if (watched == m_dialog && event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        close();
        return true;
    }
    return QDialog::eventFilter(watched, event);
}

void DiscoverWindow::reject()
{
    close();  // goes through closeEvent: discard check and geometry
}

void DiscoverWindow::closeEvent(QCloseEvent* event)
{
    if (!confirmDiscard()) {
        event->ignore();
        return;
    }
    APPLICATION->settings()->set(GalaxySettings::DiscoverGeometry, QString::fromUtf8(saveGeometry().toBase64()));
    // Not QDialog::closeEvent(): it calls reject(), which calls close() again; that nested close fails while this one
    // is in progress, so QDialog would ignore the event and the window could never be closed.
    event->accept();
}
