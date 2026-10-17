// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2023 Trial97 <alexandru.tripon97@gmail.com>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "ExternalResourcesPage.h"
#include "ui/MultiDecorationItemDelegate.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui_ExternalResourcesPage.h"

#include "DesktopServices.h"
#include "minecraft/mod/ResourceFolderModel.h"
#include "modplatform/ModIndex.h"
#include "ui/galaxy/ProjectActions.h"
#include "ui/galaxy/SystemContent.h"
#include "ui/galaxy/UpdateChecker.h"
#include "ui/galaxy/UpdatesDialog.h"
#include "ui/GuiUtil.h"

#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <algorithm>

namespace {
class LockDelegate : public QStyledItemDelegate {
   public:
    explicit LockDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& opt, const QModelIndex& index) const override
    {
        QStyleOptionViewItem option(opt);
        initStyleOption(&option, index);

        bool locked = index.data(Qt::UserRole).toBool();

        option.text.clear();
        option.icon = QIcon::fromTheme(locked ? "lock" : "unlock");
        option.features |= QStyleOptionViewItem::HasDecoration;
        option.decorationAlignment = Qt::AlignBottom | Qt::AlignHCenter;
        option.decorationPosition = QStyleOptionViewItem::Top;

        int size = qMin(option.rect.width(), option.rect.height()) * 3 / 4;
        option.decorationSize = QSize(size, size);

        option.widget->style()->drawControl(QStyle::CE_ItemViewItem, &option, painter);
    }

    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& /*option*/, const QModelIndex& index) override
    {
        if (event->type() == QEvent::MouseButtonRelease) {
            bool locked = index.data(Qt::UserRole).toBool();
            model->setData(index, !locked, Qt::UserRole);
            return true;
        }
        return event->type() == QEvent::MouseButtonDblClick;  // if double click ignore it
    }
};
}  // namespace

ExternalResourcesPage::ExternalResourcesPage(MinecraftInstance* instance, ResourceFolderModel* model, QWidget* parent)
    : QMainWindow(parent)
    , m_instance(instance)
    , m_ui(new Ui::ExternalResourcesPage)
    , m_model(model)
    , m_filterModel(ResourceFolderModel::createFilterProxyModel(this))
{
    m_ui->setupUi(this);

    m_ui->actionsToolbar->insertSpacer(m_ui->actionViewFolder);

    m_filterModel->setDynamicSortFilter(true);
    m_filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_filterModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_filterModel->setSourceModel(m_model);
    m_filterModel->setFilterKeyColumn(-1);
    m_ui->treeView->setModel(m_filterModel);

    // keep the Update at the end of the list(otherwise there will be a need to iterate over the columns)
    auto lockColumn = static_cast<int>(model->columnNames(false).size()) - 1;
    m_ui->treeView->setItemDelegateForColumn(lockColumn, new LockDelegate(m_ui->treeView));
    m_ui->treeView->setItemDelegateForColumn(ResourceFolderModel::NameColumn, new MultiDecorationItemDelegate(this));
    // must come after setModel
    const auto& resizeModes = m_model->columnResizeModes();
    for (int i = 0; i < resizeModes.count(); ++i) {
        m_ui->treeView->header()->setSectionResizeMode(i, resizeModes[i]);
    }

    m_ui->treeView->installEventFilter(this);
    m_ui->treeView->sortByColumn(1, Qt::AscendingOrder);
    m_ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);

    // The default function names by Qt are pretty ugly, so let's just connect the actions manually,
    // to make it easier to read :)
    connect(m_ui->actionAddItem, &QAction::triggered, this, &ExternalResourcesPage::addItem);
    connect(m_ui->actionRemoveItem, &QAction::triggered, this, &ExternalResourcesPage::removeItem);
    connect(m_ui->actionEnableItem, &QAction::triggered, this, &ExternalResourcesPage::enableItem);
    connect(m_ui->actionDisableItem, &QAction::triggered, this, &ExternalResourcesPage::disableItem);
    connect(m_ui->actionViewHomepage, &QAction::triggered, this, &ExternalResourcesPage::viewHomepage);
    connect(m_ui->actionViewConfigs, &QAction::triggered, this, &ExternalResourcesPage::viewConfigs);
    connect(m_ui->actionViewFolder, &QAction::triggered, this, &ExternalResourcesPage::viewFolder);

    connect(m_ui->treeView, &QWidget::customContextMenuRequested, this, &ExternalResourcesPage::showContextMenu);
    connect(m_ui->treeView, &QAbstractItemView::activated, this, &ExternalResourcesPage::itemActivated);

    connect(m_ui->actionLockUpdates, &QAction::triggered, this, &ExternalResourcesPage::lockUpdates);
    connect(m_ui->actionUnlockUpdates, &QAction::triggered, this, &ExternalResourcesPage::unlockUpdates);

    auto* selectionModel = m_ui->treeView->selectionModel();

    connect(selectionModel, &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current, const QModelIndex& previous) {
        if (!current.isValid()) {
            m_ui->frame->clear();
            return;
        }

        updateFrame(current, previous);
    });

    auto updateExtra = [this]() {
        if (updateExtraInfo) {
            updateExtraInfo(id(), extraHeaderInfoString());
        }
    };

    connect(selectionModel, &QItemSelectionModel::selectionChanged, this, updateExtra);
    connect(model, &ResourceFolderModel::updateFinished, this, updateExtra);
    connect(model, &ResourceFolderModel::parseFinished, this, updateExtra);

    connect(selectionModel, &QItemSelectionModel::selectionChanged, this, [this] { updateActions(); });
    connect(m_model, &ResourceFolderModel::rowsInserted, this, [this] { updateActions(); });
    connect(m_model, &ResourceFolderModel::rowsRemoved, this, [this] { updateActions(); });
    connect(m_model, &ResourceFolderModel::dataChanged, this, [this] { updateActions(); });
    connect(m_model, &ResourceFolderModel::sizeHintChanged, m_ui->treeView->itemDelegate(),
            [this] { m_ui->treeView->itemDelegate()->sizeHintChanged(QModelIndex()); });

    auto* viewHeader = m_ui->treeView->header();
    viewHeader->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(viewHeader, &QHeaderView::customContextMenuRequested, this, &ExternalResourcesPage::showHeaderContextMenu);

    // always use the smallest possible size to avoid wasted space
    m_ui->treeView->header()->resizeSection(0, 0);

    m_model->loadColumns(m_ui->treeView);
    connect(m_ui->treeView->header(), &QHeaderView::sectionResized, this, [this] { m_model->saveColumns(m_ui->treeView); });
    connect(m_ui->filterEdit, &QLineEdit::textChanged, this, &ExternalResourcesPage::filterTextChanged);
    updateActions();
}

ExternalResourcesPage::~ExternalResourcesPage()
{
    delete m_ui;
}

QMenu* ExternalResourcesPage::createPopupMenu()
{
    QMenu* filteredMenu = QMainWindow::createPopupMenu();
    filteredMenu->removeAction(m_ui->actionsToolbar->toggleViewAction());
    return filteredMenu;
}

void ExternalResourcesPage::showContextMenu(const QPoint& pos)
{
    // Galaxy Launcher: one project gets the same menu as in the galaxy; a multiple selection keeps the bulk actions.
    const auto clicked = m_ui->treeView->indexAt(pos);
    const auto rows = m_ui->treeView->selectionModel()->selectedRows();
    if (clicked.isValid() && rows.size() <= 1) {
        const auto source = m_filterModel->mapToSource(clicked);
        if (source.isValid() && source.row() < m_model->size()) {
            const auto body = SystemContent::bodyFor(m_model, m_model->at(source.row()));
            ProjectActions::showContextMenu(this, m_instance, body, m_ui->treeView->viewport()->mapToGlobal(pos));
            return;
        }
    }
    auto* menu = m_ui->actionsToolbar->createContextMenu(this, tr("Context menu"));
    menu->exec(m_ui->treeView->mapToGlobal(pos));
    delete menu;
}

void ExternalResourcesPage::showHeaderContextMenu(const QPoint& pos)
{
    auto* menu = m_model->createHeaderContextMenu(m_ui->treeView);
    menu->exec(m_ui->treeView->mapToGlobal(pos));
    menu->deleteLater();
}

void ExternalResourcesPage::openedImpl()
{
    m_model->startWatching();
    UpdateChecker::instance()->check(m_instance);  // at most every 30 minutes; waits for the folders to load
}

void ExternalResourcesPage::closedImpl()
{
    m_model->stopWatching();
}

void ExternalResourcesPage::retranslate()
{
    m_ui->retranslateUi(this);
    applyGalaxyActions();
}

void ExternalResourcesPage::itemActivated(const QModelIndex& /*unused*/)
{
    auto selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection());
    m_model->setResourceEnabled(selection.indexes(), EnableAction::TOGGLE);
}

void ExternalResourcesPage::filterTextChanged(const QString& newContents)
{
    m_viewFilter = newContents;
    m_filterModel->setFilterRegularExpression(m_viewFilter);
}

bool ExternalResourcesPage::shouldDisplay() const
{
    return true;
}

bool ExternalResourcesPage::listFilter(QKeyEvent* keyEvent)
{
    switch (keyEvent->key()) {
        case Qt::Key_Delete:
            removeItem();
            return true;
        case Qt::Key_Plus:
            addItem();
            return true;
        default:
            break;
    }
    return QWidget::eventFilter(m_ui->treeView, keyEvent);
}

bool ExternalResourcesPage::event(QEvent* ev)
{
    if (ev->type() == QEvent::StyleChange) {
        // NOTE: style can change minimum size
        m_ui->treeView->header()->resizeSection(0, 0);
    }
    return true;
}

bool ExternalResourcesPage::eventFilter(QObject* obj, QEvent* ev)
{
    if (ev->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(obj, ev);
    }

    auto* keyEvent = static_cast<QKeyEvent*>(ev);
    if (obj == m_ui->treeView) {
        return listFilter(keyEvent);
    }

    return QWidget::eventFilter(obj, ev);
}

void ExternalResourcesPage::addItem()
{
    auto list = GuiUtil::browseForFiles(
        helpPage(), tr("Select %1", "Select whatever type of files the page contains. Example: 'Loader Mods'").arg(displayName()),
        m_fileSelectionFilter.arg(displayName()), APPLICATION->settings()->get("CentralModsDir").toString(), this->parentWidget());

    if (!list.isEmpty()) {
        for (const auto& filename : list) {
            m_model->installResource(filename);
        }
    }
}

void ExternalResourcesPage::removeItem()
{
    auto selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection());

    int count = 0;
    bool folder = false;
    for (auto& i : selection.indexes()) {
        if (i.column() == 0) {
            count++;

            // if a folder is selected, show the confirmation dialog
            if (m_model->at(i.row()).fileinfo().isDir()) {
                folder = true;
            }
        }
    }

    QString text;
    bool multiple = count > 1;

    if (multiple) {
        text = tr("You are about to remove %1 items.\n"
                  "This may be permanent and they will be gone from the folder.\n\n"
                  "Are you sure?")
                   .arg(count);
    } else if (folder) {
        text = tr("You are about to remove the folder \"%1\".\n"
                  "This may be permanent and it will be gone from the parent folder.\n\n"
                  "Are you sure?")
                   .arg(m_model->at(selection.indexes().at(0).row()).fileinfo().fileName());
    }

    if (!text.isEmpty()) {
        auto response = CustomMessageBox::selectable(this, tr("Confirm Removal"), text, QMessageBox::Warning,
                                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                            ->exec();

        if (response != QMessageBox::Yes) {
            return;
        }
    }

    removeItems(selection);
}

void ExternalResourcesPage::removeItems(const QItemSelection& selection)
{
    if (m_instance != nullptr && m_instance->isRunning()) {
        auto response = CustomMessageBox::selectable(this, tr("Confirm Delete"),
                                                     tr("If you remove this resource while the game is running it may crash your game.\n"
                                                        "Are you sure you want to do this?"),
                                                     QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                            ->exec();

        if (response != QMessageBox::Yes) {
            return;
        }
    }
    m_model->deleteResources(selection.indexes());
}

void ExternalResourcesPage::enableItem()
{
    auto selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection());
    m_model->setResourceEnabled(selection.indexes(), EnableAction::ENABLE);
}

void ExternalResourcesPage::disableItem()
{
    auto selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection());
    m_model->setResourceEnabled(selection.indexes(), EnableAction::DISABLE);
}

namespace {
/** Galaxy Launcher: the project's Modrinth page, when the file was installed from Modrinth. */
QString modrinthPage(const Resource* resource)
{
    const auto metadata = resource->metadata();
    if (!metadata || metadata->provider != ModPlatform::ResourceProvider::MODRINTH) {
        return {};
    }
    return ModPlatform::getMetaURL(metadata->provider, metadata->projectId);
}
}  // namespace

void ExternalResourcesPage::applyGalaxyActions()
{
    if (!m_actionUpdate) {
        m_actionUpdate = new QAction(this);
        connect(m_actionUpdate, &QAction::triggered, this, [this] {
            const auto rows = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection()).indexes();
            const auto selected = m_model->selectedResources(rows);
            if (selected.size() == 1) {
                ProjectActions::update(this, m_instance, SystemContent::bodyFor(m_model, *selected.first()));
            }
        });
        m_ui->actionsToolbar->insertActionAfter(m_ui->actionChangeVersion, m_actionUpdate);

        m_actionUpdateAll = new QAction(UpdatesDialog::icon(), QString(), this);
        connect(m_actionUpdateAll, &QAction::triggered, this, [this] { ProjectActions::updateAll(this, m_instance); });
        m_ui->actionsToolbar->insertActionAfter(m_actionUpdate, m_actionUpdateAll);
        for (auto* button : m_ui->actionsToolbar->findChildren<QToolButton*>()) {
            if (button->defaultAction() == m_actionUpdateAll) {
                button->setStyleSheet(QStringLiteral("QToolButton { color: #F85149; font-weight: 600; }"));
            }
        }
        connect(UpdateChecker::instance(), &UpdateChecker::updatesChanged, this, [this](const QString& id) {
            if (m_instance && id == m_instance->id()) {
                updateGalaxyActions();
            }
        });
    }
    m_actionUpdate->setText(tr("Update"));
    m_actionUpdate->setToolTip(tr("Update the selected project to its latest version."));
    m_actionUpdateAll->setToolTip(tr("Review and install every update available in this profile."));
    updateGalaxyActions();

    m_ui->actionsToolbar->removeAction(m_ui->actionUpdateItem);
    m_ui->actionsToolbar->removeAction(m_ui->actionLockUpdates);
    m_ui->actionsToolbar->removeAction(m_ui->actionUnlockUpdates);
    m_ui->actionDownloadItem->setText(tr("Add Content"));
    m_ui->actionDownloadItem->setToolTip(tr("Find and install content from Modrinth."));
    m_ui->actionViewHomepage->setText(tr("Modrinth Page"));
    m_ui->actionViewHomepage->setToolTip(tr("Open the Modrinth page of each selected item."));
}

void ExternalResourcesPage::updateGalaxyActions()
{
    if (!m_actionUpdate) {
        return;
    }
    const auto rows = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection()).indexes();
    const auto selected = m_model->selectedResources(rows);
    const bool one = selected.size() == 1;
    m_actionUpdate->setEnabled(one && !modrinthPage(selected.first()).isEmpty());
    const QString latest = one ? UpdateChecker::instance()->latestVersionFor(*selected.first()) : QString();
    m_actionUpdate->setText(latest.isEmpty() ? tr("Update") : tr("Update to %1").arg(latest));

    const auto count = m_instance ? UpdateChecker::instance()->updatesFor(m_instance->id()).size() : 0;
    m_actionUpdateAll->setText(tr("Update All (%1)").arg(count));
    m_actionUpdateAll->setVisible(count > 0);
}

void ExternalResourcesPage::viewHomepage()
{
    auto selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection()).indexes();
    for (auto* resource : m_model->selectedResources(selection)) {
        if (const auto url = modrinthPage(resource); !url.isEmpty()) {
            DesktopServices::openUrl(url);
        }
    }
}

void ExternalResourcesPage::viewConfigs()
{
    DesktopServices::openPath(m_instance->instanceConfigFolder(), true);
}

void ExternalResourcesPage::viewFolder()
{
    DesktopServices::openPath(m_model->dir().absolutePath(), true);
}

void ExternalResourcesPage::updateActions()
{
    const bool hasSelection = m_ui->treeView->selectionModel()->hasSelection();
    const QModelIndexList selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection()).indexes();
    const QList<Resource*> selectedResources = m_model->selectedResources(selection);
    const bool hasUpdatesUnlocked = hasSelection && std::ranges::any_of(selectedResources, [](Resource* resource) {
                                        return resource->metadata() && !resource->lockUpdate();
                                    });
    const bool hasUpdatesLocked = hasSelection && std::ranges::any_of(selectedResources, [](Resource* resource) {
                                      return resource->metadata() && resource->lockUpdate();
                                  });
    const bool allSelectedUpdatesLocked =
        hasSelection && std::ranges::all_of(selectedResources, [](Resource* resource) { return resource->lockUpdate(); });

    m_ui->actionUpdateItem->setEnabled(!m_model->empty() && !allSelectedUpdatesLocked);
    m_ui->actionResetItemMetadata->setEnabled(hasSelection);

    m_ui->actionChangeVersion->setEnabled(selectedResources.size() == 1 && selectedResources[0]->metadata() != nullptr);

    m_ui->actionRemoveItem->setEnabled(hasSelection);
    m_ui->actionEnableItem->setEnabled(hasSelection);
    m_ui->actionDisableItem->setEnabled(hasSelection);

    m_ui->actionViewHomepage->setEnabled(hasSelection && std::any_of(selectedResources.begin(), selectedResources.end(),
                                                                     [](Resource* resource) { return !modrinthPage(resource).isEmpty(); }));

    m_ui->actionLockUpdates->setEnabled(hasUpdatesUnlocked);
    m_ui->actionUnlockUpdates->setEnabled(hasUpdatesLocked);
    m_ui->actionExportMetadata->setEnabled(!m_model->empty());
    updateGalaxyActions();
}

void ExternalResourcesPage::updateFrame(const QModelIndex& current, [[maybe_unused]] const QModelIndex& previous)
{
    auto sourceCurrent = m_filterModel->mapToSource(current);
    int row = sourceCurrent.row();
    const Resource& resource = m_model->at(row);
    m_ui->frame->updateWithResource(resource);
}

QString ExternalResourcesPage::extraHeaderInfoString()
{
    auto all = m_model->allResources();
    auto enabledCount = std::ranges::count_if(all, [](Resource* res) { return res->enabled(); });
    auto installedCount = m_model->size();

    if (m_ui && m_ui->treeView && m_ui->treeView->selectionModel()) {
        auto selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection()).indexes();
        if (auto count = std::count_if(selection.cbegin(), selection.cend(), [](auto v) { return v.column() == 0; }); count != 0) {
            return tr(" (%1 installed, %2 enabled, %3 selected)").arg(installedCount).arg(enabledCount).arg(count);
        }
    }

    if (enabledCount != 0) {
        return tr(" (%1 installed, %2 enabled)").arg(installedCount).arg(enabledCount);
    }

    return tr(" (%1 installed)").arg(installedCount);
}

void ExternalResourcesPage::lockUpdates()
{
    auto selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection());
    m_model->setUpdateLock(selection.indexes(), EnableAction::ENABLE);
    updateActions();
}

void ExternalResourcesPage::unlockUpdates()
{
    auto selection = m_filterModel->mapSelectionToSource(m_ui->treeView->selectionModel()->selection());
    m_model->setUpdateLock(selection.indexes(), EnableAction::DISABLE);
    updateActions();
}
