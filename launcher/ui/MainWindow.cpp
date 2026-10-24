// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
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
 *      Authors: Andrew Okin
 *               Peterix
 *               Orochimarufan <orochimarufan.x3@gmail.com>
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

#include "Application.h"
#include "BuildConfig.h"
#include "FileSystem.h"

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QUrlQuery>
#include <QVariant>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QDialogButtonBox>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>
#include <QWidgetAction>
#include <QWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <functional>
#include <memory>

#include <BaseInstance.h>
#include <BuildConfig.h>
#include <DesktopServices.h>
#include <InstanceList.h>
#include <MMCZip.h>
#include <icons/IconList.h>
#include <java/JavaInstallList.h>
#include <java/JavaUtils.h>
#include <launch/LaunchTask.h>
#include <minecraft/MinecraftInstance.h>
#include <minecraft/auth/AccountList.h>
#include <net/ApiRequest.h>
#include <net/NetJob.h>
#include <news/NewsChecker.h>
#include <tools/BaseProfiler.h>
#include <updater/ExternalUpdater.h>
#include "InstanceWindow.h"

#include "ui/GuiUtil.h"
#include "ui/InstanceWindow.h"
#include "ui/ViewLogWindow.h"
#include "ui/dialogs/AboutDialog.h"
#include "ui/dialogs/CopyInstanceDialog.h"
#include "ui/dialogs/CreateShortcutDialog.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ExportInstanceDialog.h"
#include "ui/dialogs/ExportPackDialog.h"
#include "ui/dialogs/IconPickerDialog.h"
#include "ui/dialogs/ImportResourceDialog.h"
#include "ui/dialogs/NewInstanceDialog.h"
#include "ui/dialogs/NewsDialog.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/skins/SkinManageDialog.h"
#include "ui/instanceview/InstanceDelegate.h"
#include "ui/instanceview/InstanceProxyModel.h"
#include "ui/instanceview/InstanceView.h"
#include "ui/themes/GalaxyTheme.h"
#include "ui/themes/ITheme.h"
#include "ui/themes/ThemeManager.h"

#include "ui/galaxy/DiscordPresence.h"
#include "ui/galaxy/DiscoverWindow.h"
#include "ui/widgets/PageContainer.h"
#include "ui/galaxy/GalaxyDevTour.h"
#include "ui/galaxy/GalaxySearchBar.h"
#include "ui/galaxy/GalaxySettings.h"
#include "ui/galaxy/GalaxySidebar.h"
#include "ui/galaxy/GalaxyTopBar.h"
#include "ui/galaxy/GalaxyView.h"
#include "ui/galaxy/ProfileContentsPanel.h"
#include "ui/galaxy/ProfileOrganizer.h"
#include "ui/galaxy/ProjectActions.h"
#include "ui/galaxy/LauncherUpdate.h"
#include "ui/galaxy/NotificationArea.h"
#include "ui/galaxy/Notifications.h"
#include "ui/galaxy/UpdateChecker.h"
#include "ui/galaxy/UpdatesDialog.h"
#include "ui/galaxy/SunColor.h"

#include "minecraft/PackProfile.h"
#include "minecraft/VersionFile.h"
#include "minecraft/WorldList.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ResourcePackFolderModel.h"
#include "minecraft/mod/ShaderPackFolderModel.h"
#include "minecraft/mod/TexturePackFolderModel.h"
#include "minecraft/mod/tasks/LocalResourceParse.h"

#include "modplatform/ModIndex.h"
#include "modplatform/flame/FlameAPI.h"
#include "modplatform/flame/FlameModIndex.h"
#include "modplatform/modrinth/ModrinthAPI.h"

#include "KonamiCode.h"

#include "InstanceCopyTask.h"
#include "InstanceDirUpdate.h"

#include "Json.h"

#include "MMCTime.h"

namespace {
QString profileInUseFilter(const QString& profile, bool used)
{
    if (used) {
        return QObject::tr("%1 (in use)").arg(profile);
    } else {
        return profile;
    }
}
}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowIcon(APPLICATION->logo());
    setWindowTitle(APPLICATION->applicationDisplayName());
#ifndef QT_NO_ACCESSIBILITY
    setAccessibleName(BuildConfig.LAUNCHER_DISPLAYNAME);
#endif

    // instance toolbar stuff
    {
        connect(ui->instanceToolBar, &QToolBar::orientationChanged, this, &MainWindow::updateInstanceToolBarOrientation);

        // FIXME: why doesn't this work when done in the UI file?
        ui->instanceToolBar->setMinimumWidth(140);

        // if you try to add a widget to a toolbar in a .ui file
        // qt designer will delete it when you save the file >:(
        // HACK: the toolbar interferes with the sizing for some reason
        auto* infoWidget = new QWidget(this);
        auto* infoLayout = new QVBoxLayout(infoWidget);
        infoLayout->setContentsMargins(2, 0, 2, 0);

        changeIconButton = new QToolButton(infoWidget);
        changeIconButton->setObjectName(QStringLiteral("changeIconButton"));
        changeIconButton->setIconSize({ 80, 80 });
        changeIconButton->setAutoRaise(true);
        updateInstanceToolIcon("default");
        changeIconButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(changeIconButton, &QToolButton::clicked, this, &MainWindow::on_actionChangeInstIcon_triggered);
        infoLayout->addWidget(changeIconButton);

        nameLabel = new QLabel(infoWidget);
        nameLabel->setObjectName(QStringLiteral("nameLabel"));
        nameLabel->installEventFilter(this);
        nameLabel->setAlignment(Qt::AlignCenter);
        // QSizePolicy::Ignored allows the button to shrink or expand as necessary (ignoring the size hint)
        nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        nameLabel->setTextFormat(Qt::PlainText);
        infoLayout->addWidget(nameLabel);

        instanceInfoWidget = ui->instanceToolBar->insertWidgetBefore(ui->actionRenameInstance, infoWidget);
        Q_ASSERT(instanceInfoWidget != nullptr);
    }

    // set the menu for the folders help, accounts, and export tool buttons
    {
        auto foldersMenuButton = dynamic_cast<QToolButton*>(ui->mainToolBar->widgetForAction(ui->actionFoldersButton));
        ui->actionFoldersButton->setMenu(ui->foldersMenu);
        foldersMenuButton->setPopupMode(QToolButton::InstantPopup);

        helpMenuButton = dynamic_cast<QToolButton*>(ui->mainToolBar->widgetForAction(ui->actionHelpButton));
        ui->actionHelpButton->setMenu(new QMenu(this));
        ui->actionHelpButton->menu()->addActions(ui->helpMenu->actions());
        ui->actionHelpButton->menu()->removeAction(ui->actionCheckUpdate);
        helpMenuButton->setPopupMode(QToolButton::InstantPopup);

        auto accountMenuButton = dynamic_cast<QToolButton*>(ui->mainToolBar->widgetForAction(ui->actionAccountsButton));
        accountMenuButton->setPopupMode(QToolButton::InstantPopup);

        auto exportInstanceMenu = new QMenu(this);
        exportInstanceMenu->addAction(ui->actionExportInstanceZip);
        exportInstanceMenu->addAction(ui->actionExportInstanceMrPack);
        exportInstanceMenu->addAction(ui->actionExportInstanceFlamePack);
        ui->actionExportInstance->setMenu(exportInstanceMenu);
    }

    // hide, disable and show stuff
    {
        ui->actionReportBug->setVisible(!BuildConfig.BUG_TRACKER_URL.isEmpty());
        ui->actionMATRIX->setVisible(!BuildConfig.MATRIX_URL.isEmpty());
        ui->actionDISCORD->setVisible(!BuildConfig.DISCORD_URL.isEmpty());
        ui->actionREDDIT->setVisible(!BuildConfig.SUBREDDIT_URL.isEmpty());

        ui->actionCheckUpdate->setVisible(APPLICATION->updaterEnabled());

#ifndef Q_OS_MAC
        ui->actionAddToPATH->setVisible(false);
#endif

        // disabled until we have an instance selected
        ui->instanceToolBar->setEnabled(false);
        setInstanceActionsEnabled(false);

        // add a close button at the end of the main toolbar when running on gamescope / steam deck
        // this is only needed on gamescope because it defaults to an X11/XWayland session and
        // does not implement decorations
        if (qgetenv("XDG_CURRENT_DESKTOP") == "gamescope") {
            ui->mainToolBar->addAction(ui->actionCloseWindow);
        }

        ui->actionViewJavaFolder->setEnabled(BuildConfig.JAVA_DOWNLOADER_ENABLED);
    }

    {  // logs viewing
        connect(ui->actionViewLog, &QAction::triggered, this, [] { APPLICATION->showLogWindow(); });
    }

    // add the toolbar toggles to the view menu
    ui->viewMenu->addAction(ui->instanceToolBar->toggleViewAction());
    ui->viewMenu->addAction(ui->newsToolBar->toggleViewAction());

    updateThemeMenu();
    updateMainToolBar();
    // OSX magic.
    setUnifiedTitleAndToolBarOnMac(true);

    // Global shortcuts
    {
        // you can't set QKeySequence::StandardKey shortcuts in qt designer >:(
        ui->actionAddInstance->setShortcut(QKeySequence::New);
        ui->actionSettings->setShortcut(QKeySequence::Preferences);
        ui->actionUndoTrashInstance->setShortcut(QKeySequence::Undo);
        ui->actionDeleteInstance->setShortcuts({ QKeySequence(tr("Backspace")), QKeySequence::Delete });
        ui->actionCloseWindow->setShortcut(QKeySequence::Close);
        connect(ui->actionCloseWindow, &QAction::triggered, APPLICATION, &Application::closeCurrentWindow);

        // FIXME: This is kinda weird. and bad. We need some kind of managed shutdown.
        auto q = new QShortcut(QKeySequence::Quit, this);
        connect(q, &QShortcut::activated, APPLICATION, &Application::quit);
    }

    // Konami Code
    {
        secretEventFilter = new KonamiCode(this);
        connect(secretEventFilter, &KonamiCode::triggered, this, &MainWindow::konamiTriggered);
    }

    // Add the news label to the news toolbar.
    {
        m_newsChecker.reset(new NewsChecker(APPLICATION->network(), BuildConfig.NEWS_RSS_URL));
        newsLabel = new QToolButton();
        newsLabel->setIcon(QIcon::fromTheme("news"));
        newsLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        newsLabel->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        newsLabel->setFocusPolicy(Qt::NoFocus);
        ui->newsToolBar->insertWidget(ui->actionMoreNews, newsLabel);

        connect(newsLabel, &QAbstractButton::clicked, this, &MainWindow::newsButtonClicked);
        connect(m_newsChecker.get(), &NewsChecker::newsLoaded, this, &MainWindow::updateNewsLabel);
        updateNewsLabel();
    }

    // Create the instance list widget
    {
        view = new InstanceView(ui->centralWidget);

        view->setSelectionMode(QAbstractItemView::SingleSelection);
        // FIXME: leaks ListViewDelegate
        auto delegate = new ListViewDelegate(this);
        view->setItemDelegate(delegate);
        view->setFrameShape(QFrame::NoFrame);
        // do not show ugly blue border on the mac
        view->setAttribute(Qt::WA_MacShowFocusRect, false);
        connect(delegate, &ListViewDelegate::textChanged, this, &MainWindow::applyInstanceRename);

        view->installEventFilter(this);
        view->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(view, &QWidget::customContextMenuRequested, this, &MainWindow::showInstanceContextMenu);
        connect(view, &InstanceView::droppedURLs, this, &MainWindow::processURLs, Qt::QueuedConnection);

        proxymodel = new InstanceProxyModel(this);
        proxymodel->setSourceModel(APPLICATION->instances());
        proxymodel->sort(0);
        connect(proxymodel, &InstanceProxyModel::dataChanged, this, &MainWindow::instanceDataChanged);

        view->setModel(proxymodel);
        view->setSourceOfGroupCollapseStatus(
            [](const QString& groupName) -> bool { return APPLICATION->instances()->isGroupCollapsed(groupName); });
        connect(view, &InstanceView::groupStateChanged, APPLICATION->instances(), &InstanceList::on_GroupStateChanged);
        // Galaxy Launcher: the instance view becomes the list view, inside the galaxy chrome.
        setupGalaxyUi();
    }
    // The cat background
    {
        // set the cat action priority here so you can still see the action in qt designer
        ui->actionCAT->setPriority(QAction::LowPriority);
        updateCatState();
        connect(ui->actionCAT, &QAction::toggled, this, &MainWindow::onCatToggled);
        connect(APPLICATION, &Application::currentCatChanged, this, &MainWindow::onCatChanged);
    }

    // Togglable status bar
    {
        bool statusBarVisible = APPLICATION->settings()->get("StatusBarVisible").toBool();
        ui->actionToggleStatusBar->setChecked(statusBarVisible);
        connect(ui->actionToggleStatusBar, &QAction::toggled, this, &MainWindow::setStatusBarVisibility);
        setStatusBarVisibility(statusBarVisible);
    }

    // Lock toolbars
    {
        bool toolbarsLocked = APPLICATION->settings()->get("ToolbarsLocked").toBool();
        ui->actionLockToolbars->setChecked(toolbarsLocked);
        connect(ui->actionLockToolbars, &QAction::toggled, this, &MainWindow::lockToolbars);
        lockToolbars(toolbarsLocked);
    }
    // start instance when double-clicked
    connect(view, &InstanceView::activated, this, &MainWindow::instanceActivated);

    // track the selection -- update the instance toolbar
    connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::instanceChanged);

    // track icon changes and update the toolbar!
    connect(APPLICATION->icons(), &IconList::iconUpdated, this, &MainWindow::iconUpdated);

    // model reset -> selection is invalid. All the instance pointers are wrong.
    connect(APPLICATION->instances(), &InstanceList::dataIsInvalid, this, &MainWindow::selectionBad);

    // handle newly added instances
    connect(APPLICATION->instances(), &InstanceList::instanceSelectRequest, this, &MainWindow::instanceSelectRequest);

    // When the global settings page closes, we want to know about it and update our state
    connect(APPLICATION, &Application::globalSettingsApplied, this, &MainWindow::globalSettingsClosed);

    m_statusLeft = new QLabel(tr("No instance selected"), this);
    m_statusCenter = new QLabel(tr("Total playtime: 0s"), this);
    statusBar()->addPermanentWidget(m_statusLeft, 1);
    statusBar()->addPermanentWidget(m_statusCenter, 0);

    // Add "manage accounts" button, right align
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->mainToolBar->insertWidget(ui->actionAccountsButton, spacer);

    // Use undocumented property... https://stackoverflow.com/questions/7121718/create-a-scrollbar-in-a-submenu-qt
    ui->accountsMenu->setStyleSheet("QMenu { menu-scrollable: 1; }");

    repopulateAccountsMenu();

    // Update the menu when the active account changes.
    // Shouldn't have to use lambdas here like this, but if I don't, the compiler throws a fit.
    // Template hell sucks...
    connect(APPLICATION->accounts(), &AccountList::defaultAccountChanged, this, [this] { defaultAccountChanged(); });
    connect(APPLICATION->accounts(), &AccountList::listActivityChanged, this, [this] { defaultAccountChanged(); });
    connect(APPLICATION->accounts(), &AccountList::listChanged, this, [this] { defaultAccountChanged(); });

    // Show initial account
    defaultAccountChanged();

    // TODO: refresh accounts here?
    // auto accounts = APPLICATION->accounts();

    // load the news
    {
        m_newsChecker->reloadNews();
        updateNewsLabel();
    }

    if (APPLICATION->updaterEnabled()) {
        bool updatesAllowed = APPLICATION->updatesAreAllowed();
        updatesAllowedChanged(updatesAllowed);

        connect(ui->actionCheckUpdate, &QAction::triggered, this, &MainWindow::checkForUpdates);

        // set up the updater object.
        auto updater = APPLICATION->updater();

        if (updater) {
            connect(updater, &ExternalUpdater::canCheckForUpdatesChanged, this, &MainWindow::updatesAllowedChanged);
        }
    }

    connect(ui->actionUndoTrashInstance, &QAction::triggered, this, &MainWindow::undoTrashInstance);

    setSelectedInstanceById(APPLICATION->settings()->get("SelectedInstance").toString());

    // removing this looks stupid
    view->setFocus();

    retranslateUi();
}

// macOS always has a native menu bar, so these fixes are not applicable
// Other systems may or may not have a native menu bar (most do not - it seems like only Ubuntu Unity does)
#ifndef Q_OS_MAC
void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Alt && !APPLICATION->settings()->get("MenuBarInsteadOfToolBar").toBool())
        ui->menuBar->setVisible(!ui->menuBar->isVisible());
    else
        QMainWindow::keyReleaseEvent(event);
}
#endif

void MainWindow::retranslateUi()
{
    if (m_selectedInstance) {
        m_statusLeft->setText(m_selectedInstance->getStatusbarDescription());
    } else {
        m_statusLeft->setText(tr("No instance selected"));
    }

    ui->retranslateUi(this);

    MinecraftAccountPtr defaultAccount = APPLICATION->accounts()->defaultAccount();
    if (defaultAccount) {
        auto profileLabel = profileInUseFilter(defaultAccount->displayName(), defaultAccount->isInUse());
        ui->actionAccountsButton->setText(profileLabel);
    }

    changeIconButton->setToolTip(ui->actionChangeInstIcon->toolTip());

    // replace the %1 with the launcher display name in some actions
    if (helpMenuButton->toolTip().contains("%1"))
        helpMenuButton->setToolTip(helpMenuButton->toolTip().arg(BuildConfig.LAUNCHER_DISPLAYNAME));

    for (auto action : ui->helpMenu->actions()) {
        if (action->text().contains("%1"))
            action->setText(action->text().arg(BuildConfig.LAUNCHER_DISPLAYNAME));
        if (action->toolTip().contains("%1"))
            action->setToolTip(action->toolTip().arg(BuildConfig.LAUNCHER_DISPLAYNAME));
    }
}

MainWindow::~MainWindow() {}

QMenu* MainWindow::createPopupMenu()
{
    QMenu* filteredMenu = QMainWindow::createPopupMenu();
    filteredMenu->removeAction(ui->mainToolBar->toggleViewAction());

    filteredMenu->addAction(ui->actionToggleStatusBar);
    filteredMenu->addAction(ui->actionLockToolbars);

    return filteredMenu;
}
void MainWindow::setStatusBarVisibility(bool state)
{
    // Galaxy: no status bar. A profile's play time is in its tooltip in the galaxy, the total in the black hole's.
    statusBar()->setVisible(false);
    APPLICATION->settings()->set("StatusBarVisible", state);
}
void MainWindow::lockToolbars(bool state)
{
    ui->mainToolBar->setMovable(!state);
    ui->instanceToolBar->setMovable(!state);
    ui->newsToolBar->setMovable(!state);
    APPLICATION->settings()->set("ToolbarsLocked", state);
}

void MainWindow::konamiTriggered()
{
    QString gradient =
        " stop:0 rgba(125, 0, 0, 255), stop:0.166 rgba(125, 125, 0, 255), stop:0.333 rgba(0, 125, 0, 255), stop:0.5 rgba(0, 125, 125, "
        "255), stop:0.666 rgba(0, 0, 125, 255), stop:0.833 rgba(125, 0, 125, 255), stop:1 rgba(125, 0, 0, 255));";
    QString stylesheet = "background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0," + gradient;
    if (ui->mainToolBar->styleSheet() == stylesheet) {
        ui->mainToolBar->setStyleSheet("");
        ui->instanceToolBar->setStyleSheet("");
        ui->centralWidget->setStyleSheet("");
        ui->newsToolBar->setStyleSheet("");
        ui->statusBar->setStyleSheet("");
        qDebug() << "Super Secret Mode DEACTIVATED!";
    } else {
        ui->mainToolBar->setStyleSheet(stylesheet);
        ui->instanceToolBar->setStyleSheet("background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1," + gradient);
        ui->centralWidget->setStyleSheet("background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1," + gradient);
        ui->newsToolBar->setStyleSheet(stylesheet);
        ui->statusBar->setStyleSheet(stylesheet);
        qDebug() << "Super Secret Mode ACTIVATED!";
    }
}

void MainWindow::showInstanceContextMenu(const QPoint& pos)
{
    QList<QAction*> actions;

    QAction* actionSep = new QAction("", this);
    actionSep->setSeparator(true);

    bool onInstance = view->indexAt(pos).isValid();
    if (onInstance) {
        // reuse the file menu actions
        actions = ui->fileMenu->actions();

        // remove the add instance action, launcher settings action and close action
        actions.removeFirst();
        actions.removeLast();
        actions.removeLast();

        actions.prepend(ui->actionChangeInstIcon);
        actions.prepend(ui->actionRenameInstance);

        // add header
        actions.prepend(actionSep);
        QAction* actionVoid = new QAction(m_selectedInstance->name(), this);
        actionVoid->setEnabled(false);
        actions.prepend(actionVoid);
    } else {
        auto group = view->groupNameAt(pos);

        QAction* actionVoid = new QAction(group.isNull() ? BuildConfig.LAUNCHER_DISPLAYNAME : group, this);
        actionVoid->setEnabled(false);

        QAction* actionCreateInstance = new QAction(tr("&Create instance"), this);
        actionCreateInstance->setToolTip(ui->actionAddInstance->toolTip());
        if (!group.isNull()) {
            QVariantMap instance_action_data;
            instance_action_data["group"] = group;
            actionCreateInstance->setData(instance_action_data);
        }

        connect(actionCreateInstance, &QAction::triggered, this, &MainWindow::on_actionAddInstance_triggered);

        actions.prepend(actionSep);
        actions.prepend(actionVoid);
        actions.append(actionCreateInstance);
        if (!group.isNull()) {
            QAction* actionDeleteGroup = new QAction(tr("&Delete group"), this);
            connect(actionDeleteGroup, &QAction::triggered, this, [this, group] { deleteGroup(group); });
            actions.append(actionDeleteGroup);

            QAction* actionRenameGroup = new QAction(tr("&Rename group"), this);
            connect(actionRenameGroup, &QAction::triggered, this, [this, group] { renameGroup(group); });
            actions.append(actionRenameGroup);
        }
    }
    QMenu myMenu;
    myMenu.addActions(actions);
    /*
    if (onInstance)
        myMenu.setEnabled(m_selectedInstance->canLaunch());
    */
    myMenu.exec(view->mapToGlobal(pos));
}

void MainWindow::updateMainToolBar()
{
    ui->menuBar->setVisible(APPLICATION->settings()->get("MenuBarInsteadOfToolBar").toBool());
    // Galaxy Launcher: the galaxy top bar carries every main toolbar action, so Prism's main toolbar stays hidden.
    ui->mainToolBar->setVisible(false);
}

void MainWindow::updateLaunchButton()
{
    QMenu* launchMenu = ui->actionLaunchInstance->menu();
    if (launchMenu)
        launchMenu->clear();
    else
        launchMenu = new QMenu(this);
    if (m_selectedInstance)
        m_selectedInstance->populateLaunchMenu(launchMenu);
    ui->actionLaunchInstance->setMenu(launchMenu);
}

void MainWindow::updateThemeMenu()
{
    QMenu* themeMenu = ui->actionChangeTheme->menu();

    if (themeMenu) {
        themeMenu->clear();
    } else {
        themeMenu = new QMenu(this);
    }

    auto themes = APPLICATION->themeManager()->getValidApplicationThemes();

    QActionGroup* themesGroup = new QActionGroup(this);

    for (auto* theme : themes) {
        QAction* themeAction = themeMenu->addAction(theme->name());

        themeAction->setCheckable(true);
        if (APPLICATION->settings()->get("ApplicationTheme").toString() == theme->id()) {
            themeAction->setChecked(true);
        }
        themeAction->setActionGroup(themesGroup);

        connect(themeAction, &QAction::triggered, APPLICATION, [id = theme->id()]() {
            APPLICATION->themeManager()->setApplicationTheme(id);
            APPLICATION->settings()->set("ApplicationTheme", id);
        });
    }

    ui->actionChangeTheme->setMenu(themeMenu);
}

void MainWindow::repopulateAccountsMenu()
{
    ui->accountsMenu->clear();

    // NOTE: this is done so the accounts button text is not set to the accounts menu title
    QMenu* accountsButtonMenu = ui->actionAccountsButton->menu();
    if (accountsButtonMenu) {
        accountsButtonMenu->clear();
    } else {
        accountsButtonMenu = new QMenu(this);
        ui->actionAccountsButton->setMenu(accountsButtonMenu);
    }

    auto accounts = APPLICATION->accounts();
    MinecraftAccountPtr defaultAccount = accounts->defaultAccount();

    bool canChangeSkin = defaultAccount && (defaultAccount->accountType() == AccountType::MSA) && !defaultAccount->isActive();
    ui->actionManageSkins->setEnabled(canChangeSkin);

    QString active_profileId = "";
    if (defaultAccount) {
        // this can be called before accountMenuButton exists
        if (ui->actionAccountsButton) {
            auto profileLabel = profileInUseFilter(defaultAccount->displayName(), defaultAccount->isInUse());
            ui->actionAccountsButton->setText(profileLabel);
        }
    }

    QActionGroup* accountsGroup = new QActionGroup(this);

    if (accounts->count() <= 0) {
        ui->actionNoAccountsAdded->setEnabled(false);
        ui->accountsMenu->addAction(ui->actionNoAccountsAdded);
    } else {
        // TODO: Nicer way to iterate?
        for (int i = 0; i < accounts->count(); i++) {
            MinecraftAccountPtr account = accounts->at(i);
            auto profileLabel = profileInUseFilter(account->displayName(), account->isInUse());
            QAction* action = new QAction(profileLabel, this);
            action->setData(i);
            action->setCheckable(true);
            action->setActionGroup(accountsGroup);
            if (defaultAccount == account) {
                action->setChecked(true);
            }

            auto face = account->getFace();
            if (!face.isNull()) {
                action->setIcon(face);
            } else {
                action->setIcon(QIcon::fromTheme("noaccount"));
            }

            const int highestNumberKey = 9;
            if (i < highestNumberKey) {
                action->setShortcut(QKeySequence(tr("Ctrl+%1").arg(i + 1)));
            }

            ui->accountsMenu->addAction(action);
            connect(action, &QAction::triggered, this, &MainWindow::changeActiveAccount);
        }
    }

    ui->accountsMenu->addSeparator();

    ui->actionNoDefaultAccount->setData(-1);
    ui->actionNoDefaultAccount->setChecked(!defaultAccount);
    ui->actionNoDefaultAccount->setActionGroup(accountsGroup);

    ui->accountsMenu->addAction(ui->actionNoDefaultAccount);

    connect(ui->actionNoDefaultAccount, &QAction::triggered, this, &MainWindow::changeActiveAccount);

    ui->accountsMenu->addSeparator();
    ui->accountsMenu->addAction(ui->actionManageSkins);
    ui->accountsMenu->addAction(ui->actionManageAccounts);

    accountsButtonMenu->addActions(ui->accountsMenu->actions());
}

void MainWindow::updatesAllowedChanged(bool allowed)
{
    if (!APPLICATION->updaterEnabled()) {
        return;
    }
    ui->actionCheckUpdate->setEnabled(allowed);
}

/*
 * Assumes the sender is a QAction
 */
void MainWindow::changeActiveAccount()
{
    QAction* sAction = (QAction*)sender();

    // Profile's associated Mojang username
    if (sAction->data().typeId() != QMetaType::Int)
        return;

    QVariant action_data = sAction->data();
    bool valid = false;
    int index = action_data.toInt(&valid);
    if (!valid) {
        index = -1;
    }
    auto accounts = APPLICATION->accounts();
    accounts->setDefaultAccount(index == -1 ? nullptr : accounts->at(index));
    defaultAccountChanged();
}

void MainWindow::defaultAccountChanged()
{
    repopulateAccountsMenu();

    MinecraftAccountPtr account = APPLICATION->accounts()->defaultAccount();

    if (m_sidebar) {
        if (account && account->profileName() != "") {
            const auto face = account->getFace();
            m_sidebar->setAccount(profileInUseFilter(account->displayName(), account->isInUse()),
                                  face.isNull() ? QIcon::fromTheme("noaccount") : QIcon(face));
        } else {
            m_sidebar->setAccount(APPLICATION->accounts()->count() > 0 ? tr("No default account") : tr("Add an account"),
                                  QIcon::fromTheme("noaccount"));
        }
    }

    // FIXME: this needs adjustment for MSA
    if (account && account->profileName() != "") {
        auto profileLabel = profileInUseFilter(account->displayName(), account->isInUse());
        ui->actionAccountsButton->setText(profileLabel);
        auto face = account->getFace();
        if (face.isNull()) {
            ui->actionAccountsButton->setIcon(QIcon::fromTheme("noaccount"));
        } else {
            ui->actionAccountsButton->setIcon(face);
        }
        return;
    }

    // Set the icon to the "no account" icon.
    ui->actionAccountsButton->setIcon(QIcon::fromTheme("noaccount"));
    ui->actionAccountsButton->setText(tr("Accounts"));
}

bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
    if (handleFramelessResize(obj, ev)) {
        return true;
    }
    if (obj == view) {
        if (ev->type() == QEvent::KeyPress) {
            secretEventFilter->input(ev);
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(ev);
            switch (keyEvent->key()) {
                    /*
                case Qt::Key_Enter:
                case Qt::Key_Return:
                    activateInstance(m_selectedInstance);
                    return true;
                    */
                case Qt::Key_Delete:
                    on_actionDeleteInstance_triggered();
                    return true;
                case Qt::Key_F5:
                    refreshInstances();
                    return true;
                case Qt::Key_F2:
                    on_actionRenameInstance_triggered();
                    return true;
                default:
                    break;
            }
        }
    } else if (obj == nameLabel && ev->type() == QEvent::Resize) {
        updateNameLabel();
    }
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::updateNewsLabel()
{
    if (m_newsChecker->isLoadingNews()) {
        newsLabel->setText(tr("Loading news..."));
        newsLabel->setEnabled(false);
        ui->actionMoreNews->setVisible(false);
    } else {
        QList<NewsEntryPtr> entries = m_newsChecker->getNewsEntries();
        if (entries.length() > 0) {
            newsLabel->setText(entries[0]->title);
            newsLabel->setEnabled(true);
            ui->actionMoreNews->setVisible(true);
        } else {
            newsLabel->setText(tr("No news available."));
            newsLabel->setEnabled(false);
            ui->actionMoreNews->setVisible(false);
        }
    }
}

QList<int> stringToIntList(const QString& string)
{
    QStringList split = string.split(',', Qt::SkipEmptyParts);
    QList<int> out;
    for (int i = 0; i < split.size(); ++i) {
        out.append(split.at(i).toInt());
    }
    return out;
}
QString intListToString(const QList<int>& list)
{
    QStringList slist;
    for (int i = 0; i < list.size(); ++i) {
        slist.append(QString::number(list.at(i)));
    }
    return slist.join(',');
}

void MainWindow::onCatToggled(bool state)
{
    setCatBackground(state);
    APPLICATION->settings()->set("TheCat", state);
}

void MainWindow::setCatBackground(bool enabled)
{
    view->setPaintCat(enabled);
    view->viewport()->repaint();
}

void MainWindow::updateCatState()
{
    SettingsObject* settings = APPLICATION->settings();
    const bool catEnabled = settings->get("EnableCat").toBool();
    bool catVisible = settings->get("TheCat").toBool();
    if (!catEnabled && catVisible) {
        settings->set("TheCat", false);
        catVisible = false;
    }

    ui->actionCAT->setVisible(catEnabled);
    ui->actionCAT->setChecked(catVisible);
    setCatBackground(catVisible);
}

void MainWindow::runModalTask(Task* task, const QString& title)
{
    connect(task, &Task::failed, this,
            [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
    connect(task, &Task::succeeded, this, [this, task]() {
        QStringList warnings = task->warnings();
        if (warnings.count()) {
            CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();
        }
    });
    ProgressDialog loadDialog(this);
    loadDialog.showSkipButton();
    if (!title.isEmpty()) {
        loadDialog.setWindowTitle(title);  // Galaxy: names the progress notification
    }
    loadDialog.execWithTask(task);
}

void MainWindow::instanceFromInstanceTask(InstanceTask* rawTask)
{
    const auto name = rawTask->name();
    unique_qobject_ptr<Task> task(APPLICATION->instances()->wrapInstanceTask(rawTask));
    runModalTask(task.get(), tr("Creating %1").arg(name));
}

void MainWindow::on_actionCopyInstance_triggered()
{
    if (!m_selectedInstance)
        return;

    CopyInstanceDialog copyInstDlg(m_selectedInstance, this);
    if (!copyInstDlg.exec())
        return;

    auto copyTask = new InstanceCopyTask(m_selectedInstance, copyInstDlg.getChosenOptions());
    copyTask->setName(copyInstDlg.instName());
    copyTask->setGroup(copyInstDlg.instGroup());
    copyTask->setIcon(copyInstDlg.iconKey());
    unique_qobject_ptr<Task> task(APPLICATION->instances()->wrapInstanceTask(copyTask));
    runModalTask(task.get(), tr("Copying %1").arg(copyInstDlg.instName()));
}

void MainWindow::addInstance(const QString& url, const QMap<QString, QString>& extra_info, const QString& initialPage)
{
    QString groupName;
    do {
        QObject* obj = sender();
        if (!obj)
            break;
        QAction* action = qobject_cast<QAction*>(obj);
        if (!action)
            break;
        auto map = action->data().toMap();
        if (!map.contains("group"))
            break;
        groupName = map["group"].toString();
    } while (0);

    if (groupName.isEmpty()) {
        groupName = APPLICATION->settings()->get("LastUsedGroupForNewInstance").toString();
    }

    NewInstanceDialog newInstDlg(groupName, url, extra_info, this);
    if (!initialPage.isEmpty()) {
        newInstDlg.selectPage(initialPage);
    }
    if (!newInstDlg.exec())
        return;

    APPLICATION->settings()->set("LastUsedGroupForNewInstance", newInstDlg.instGroup());
    APPLICATION->settings()->set("LastUsedInstDirForNewInstance", newInstDlg.instDir());

    InstanceTask* creationTask = newInstDlg.extractTask();
    if (creationTask) {
        instanceFromInstanceTask(creationTask);
    }
}

void MainWindow::on_actionAddInstance_triggered()
{
    addInstance();
}

void MainWindow::processURLs(QList<QUrl> urls)
{
    // NOTE: This loop only processes one dropped file!
    for (auto& url : urls) {
        if (url.isEmpty() || url.toString().trimmed().isEmpty())
            continue;

        qDebug() << "Processing" << url;

        // The isLocalFile() check below doesn't work as intended without an explicit scheme.
        if (url.scheme().isEmpty())
            url.setScheme("file");

        ModPlatform::IndexedVersion version;
        QMap<QString, QString> extra_info;
        QUrl local_url;
        if (!url.isLocalFile()) {  // download the remote resource and identify
            if (url.scheme().compare("modrinth", Qt::CaseInsensitive) == 0) {
                const auto packId = ModrinthAPI::getModpackIdFromUrl(url);
                if (!packId.isEmpty()) {
                    extra_info.insert("pack_id", packId);
                    addInstance(url.toString(), extra_info);
                } else {
                    CustomMessageBox::selectable(
                        this, tr("Error"),
                        tr("Unsupported Modrinth link.\n\nPrism Launcher currently only supports modpack links such as "
                           "modrinth://modpack/fabulously-optimized."),
                        QMessageBox::Critical)
                        ->show();
                }
                continue;
            }

            const bool isExternalURLImport = (url.host().toLower() == "import") || (url.path().startsWith("/import", Qt::CaseInsensitive));

            QUrl dl_url;
            if (url.scheme() == "curseforge" || (url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME && url.host() == "install")) {
                // need to find the download link for the modpack / resource
                // format of url curseforge://install?addonId=IDHERE&fileId=IDHERE
                // format of url binaryname://install?platform=curseforge&addonId=IDHERE&fileId=IDHERE
                QUrlQuery query(url);

                // check if this is a binaryname:// url
                if (url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME) {
                    // check this is an curseforge platform request
                    if (query.queryItemValue("platform").toLower() != "curseforge") {
                        qDebug() << "Invalid mod distribution platform:" << query.queryItemValue("platform");
                        continue;
                    }
                }

                if (query.allQueryItemValues("addonId").isEmpty() || query.allQueryItemValues("fileId").isEmpty()) {
                    qDebug() << "Invalid curseforge link:" << url;
                    continue;
                }

                auto addonId = query.allQueryItemValues("addonId")[0];
                auto fileId = query.allQueryItemValues("fileId")[0];

                extra_info.insert("pack_id", addonId);
                extra_info.insert("pack_version_id", fileId);

                auto [job, array] = FlameAPI::getFile(addonId, fileId);

                connect(job.get(), &Task::failed, this, [this](const QString& reason) {
                    CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show();
                });
                connect(job.get(), &Task::succeeded, this, [this, array, addonId, fileId, &dl_url, &version] {
                    qDebug() << "Returned CFURL Json:\n" << array->toStdString().c_str();
                    auto doc = Json::requireDocument(*array);
                    if (!doc) {
                        CustomMessageBox::selectable(this, tr("Error"), doc.error(), QMessageBox::Critical)->show();
                        return;
                    }
                    auto data = doc->object()["data"].toObject();
                    // No way to find out if it's a mod or a modpack before here
                    // And also we need to check if it ends with .zip, instead of any better way
                    auto versionRes = FlameMod::loadIndexedPackVersion(data);
                    if (!versionRes) {
                        CustomMessageBox::selectable(this, tr("Error"), versionRes.error(), QMessageBox::Critical)->show();
                        return;
                    }
                    version = versionRes.value();
                    auto fileName = version.fileName;

                    // Have to use ensureString then use QUrl to get proper url encoding
                    dl_url = QUrl(version.downloadUrl);
                    if (!dl_url.isValid()) {
                        CustomMessageBox::selectable(
                            this, tr("Error"),
                            tr("The modpack, mod, or resource %1 is blocked for third-parties! Please download it manually.").arg(fileName),
                            QMessageBox::Critical)
                            ->show();
                        return;
                    }
                });

                {  // drop stack
                    ProgressDialog dlUrlDialod(this);
                    dlUrlDialod.showSkipButton();
                    dlUrlDialod.execWithTask(job.get());
                }

            } else if (url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME && !isExternalURLImport) {
                QVariantMap receivedData;
                const QUrlQuery query(url.query());
                const auto items = query.queryItems();
                for (auto it = items.begin(), end = items.end(); it != end; ++it)
                    receivedData.insert(it->first, it->second);
                emit APPLICATION->oauthReplyRecieved(receivedData);
                continue;
            } else if ((url.scheme() == "prismlauncher" || url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME) && isExternalURLImport) {
                // PrismLauncher URL protocol modpack import
                // works for any prism fork
                // preferred import format: prismlauncher://import?url=ENCODED
                const auto host = url.host().toLower();
                const auto path = url.path();

                QString encodedTarget;

                {
                    QUrlQuery query(url);
                    const auto values = query.allQueryItemValues("url");
                    if (!values.isEmpty()) {
                        encodedTarget = values.first();
                    }
                }

                // alternative import format: prismlauncher://import/ENCODED
                if (encodedTarget.isEmpty()) {
                    QString p = path;

                    if (p.startsWith("/import/", Qt::CaseInsensitive)) {
                        p = p.mid(QString("/import/").size());
                    } else if (host == "import" && p.startsWith("/")) {
                        p = p.mid(1);
                    }

                    if (!p.isEmpty() && p != "/import") {
                        encodedTarget = p;
                    }
                }

                if (encodedTarget.isEmpty()) {
                    CustomMessageBox::selectable(this, tr("Error"), tr("Invalid import link: missing 'url' parameter."),
                                                 QMessageBox::Critical)
                        ->show();
                    continue;
                }

                const QString decodedStr = QUrl::fromPercentEncoding(encodedTarget.toUtf8()).trimmed();

                QUrl target = QUrl::fromUserInput(decodedStr);

                // Validate: only allow http(s)
                if (!target.isValid() || (target.scheme() != "https" && target.scheme() != "http")) {
                    CustomMessageBox::selectable(this, tr("Error"), tr("Invalid import link: URL must be http(s)."), QMessageBox::Critical)
                        ->show();
                    continue;
                }

                const auto res = QMessageBox::question(
                    this, tr("Install modpack"),
                    tr("Do you want to download and import a modpack from:\n%1\n\nURL:\n%2").arg(target.host(), target.toString()),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
                if (res != QMessageBox::Yes) {
                    continue;
                }

                dl_url = target;
            } else {
                dl_url = url;
            }

            if (!dl_url.isValid()) {
                continue;  // no valid url to download this resource
            }

            const QString path = dl_url.host() + '/' + dl_url.path();
            auto entry = APPLICATION->metacache()->resolveEntry("general", path);
            entry->setStale(true);
            auto dl_job = unique_qobject_ptr<NetJob>(new NetJob(tr("Modpack download"), APPLICATION->network()));
            dl_job->addNetAction(Net::ApiRequest::makeCached(dl_url, entry));
            auto archivePath = entry->getFullPath();

            bool dl_success = false;
            connect(dl_job.get(), &Task::failed, this,
                    [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
            connect(dl_job.get(), &Task::succeeded, this, [&dl_success] { dl_success = true; });

            {  // drop stack
                ProgressDialog dlUrlDialod(this);
                dlUrlDialod.showSkipButton();
                dlUrlDialod.execWithTask(dl_job.get());
            }

            if (!dl_success) {
                continue;  // no local file to identify
            }
            local_url = QUrl::fromLocalFile(archivePath);

        } else {
            local_url = url;
        }

        auto localFileName = QDir::toNativeSeparators(local_url.toLocalFile());
        QFileInfo localFileInfo(localFileName);

        if (localFileName.isEmpty() || !localFileInfo.exists()) {
            qDebug() << "Ignoring invalid path" << localFileName;
            continue;
        }

        auto type = ResourceUtils::identify(localFileInfo);

        if (ModPlatform::ResourceTypeUtils::g_VALID_RESOURCES.count(type) == 0) {  // probably instance/modpack
            addInstance(localFileName, extra_info);
            continue;
        }

        if (APPLICATION->instances()->count() <= 0) {
            CustomMessageBox::selectable(this, tr("No instance!"),
                                         tr("No instance available to add the resource to.\nPlease create a new instance before "
                                            "attempting to install this resource again."),
                                         QMessageBox::Critical)
                ->show();
            continue;
        }
        ImportResourceDialog dlg(localFileName, type, this);

        if (dlg.exec() != QDialog::Accepted)
            continue;

        qDebug() << "Adding resource" << localFileName << "to" << dlg.selectedInstanceKey;

        auto inst = APPLICATION->instances()->getInstanceById(dlg.selectedInstanceKey);
        auto minecraftInst = inst;

        switch (type) {
            case ModPlatform::ResourceType::ResourcePack:
                minecraftInst->resourcePackList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::TexturePack:
                minecraftInst->texturePackList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::DataPack:
                qWarning() << "Importing of Data Packs not supported at this time. Ignoring" << localFileName;
                break;
            case ModPlatform::ResourceType::Mod:
                minecraftInst->loaderModList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::ShaderPack:
                minecraftInst->shaderPackList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::World:
                minecraftInst->worldList()->installWorld(localFileInfo);
                break;
            case ModPlatform::ResourceType::Unknown:
            default:
                qDebug() << "Can't Identify" << localFileName << "Ignoring it.";
                break;
        }
    }
}

void MainWindow::on_actionREDDIT_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.SUBREDDIT_URL));
}

void MainWindow::on_actionDISCORD_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.DISCORD_URL));
}

void MainWindow::on_actionMATRIX_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.MATRIX_URL));
}

void MainWindow::on_actionChangeInstIcon_triggered()
{
    if (!m_selectedInstance)
        return;

    IconPickerDialog dlg(this);
    dlg.execWithSelection(m_selectedInstance->iconKey());
    if (dlg.result() == QDialog::Accepted) {
        m_selectedInstance->setIconKey(dlg.selectedIconKey);
        auto icon = APPLICATION->icons()->getIcon(dlg.selectedIconKey);
        ui->actionChangeInstIcon->setIcon(icon);
        changeIconButton->setIcon(icon);
    }
}

void MainWindow::iconUpdated(QString icon)
{
    if (icon == m_currentInstIcon) {
        updateInstanceToolIcon(icon);
    }
}

void MainWindow::updateInstanceToolIcon(QString new_icon)
{
    m_currentInstIcon = new_icon;
    auto icon = APPLICATION->icons()->getIcon(m_currentInstIcon);
    ui->actionChangeInstIcon->setIcon(icon);
    changeIconButton->setIcon(icon);
}

void MainWindow::setSelectedInstanceById(const QString& id)
{
    if (id.isNull())
        return;
    const QModelIndex index = APPLICATION->instances()->getInstanceIndexById(id);
    if (index.isValid()) {
        QModelIndex selectionIndex = proxymodel->mapFromSource(index);
        view->selectionModel()->setCurrentIndex(selectionIndex, QItemSelectionModel::ClearAndSelect);
        updateStatusCenter();
    }
}

void MainWindow::on_actionChangeInstGroup_triggered()
{
    if (!m_selectedInstance)
        return;

    InstanceId instId = m_selectedInstance->id();
    QString src(APPLICATION->instances()->getInstanceGroup(instId));

    QStringList groups = ProfileOrganizer::instance()->categories();  // includes categories without profiles yet
    groups.prepend("");
    int index = groups.indexOf(src);
    bool ok = false;
    QString dst = QInputDialog::getItem(this, tr("Profile category"), tr("Choose a category or type a new one (empty for none)."), groups,
                                        index, true, &ok);
    dst = dst.simplified();

    if (ok) {
        ProfileOrganizer::instance()->moveToCategory({ instId }, dst);
    }
}

void MainWindow::createCategory()
{
    ProfileOrganizer::instance()->createCategory(this, m_selectedInstance ? m_selectedInstance->id() : QString());
}

void MainWindow::deleteGroup(QString group)
{
    Q_ASSERT(!group.isEmpty());
    if (ProfileOrganizer::instance()->isEmptyCategory(group)) {
        ProfileOrganizer::instance()->removeEmptyCategory(group);  // nothing in it to ask about
        return;
    }

    const int reply = QMessageBox::question(this, tr("Delete category"),
                                            tr("Delete the category '%1'? Its profiles are kept, without a category.").arg(group),
                                            QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
        APPLICATION->instances()->deleteGroup(group);
}

void MainWindow::renameGroup(QString group)
{
    Q_ASSERT(!group.isEmpty());

    QString name = QInputDialog::getText(this, tr("Rename category"), tr("New name for the category:"), QLineEdit::Normal, group);
    name = name.simplified();
    if (name.isNull() || name == group)
        return;

    const bool empty = name.isEmpty();
    const bool duplicate = ProfileOrganizer::instance()->categories().contains(name, Qt::CaseInsensitive) && group.toLower() != name.toLower();

    if (empty || duplicate) {
        QMessageBox::warning(this, tr("Cannot rename category"),
                             empty ? tr("A category needs a name.") : tr("A category with this name already exists."));
        return;
    }

    ProfileOrganizer::instance()->renameInOrder(group, name);
    if (ProfileOrganizer::instance()->isEmptyCategory(group)) {
        ProfileOrganizer::instance()->renameEmptyCategory(group, name);
        return;
    }
    APPLICATION->instances()->renameGroup(group, name);
}

void MainWindow::undoTrashInstance()
{
    if (!APPLICATION->instances()->undoTrashInstance())
        QMessageBox::warning(
            this, tr("Failed to undo trashing instance"),
            tr("Some instances and shortcuts could not be restored.\nPlease check your trashbin to manually restore them."));
    ui->actionUndoTrashInstance->setEnabled(APPLICATION->instances()->trashedSomething());
}

void MainWindow::on_actionViewLauncherRootFolder_triggered()
{
    DesktopServices::openPath(".");
}

void MainWindow::on_actionViewInstanceFolder_triggered()
{
    QString str = APPLICATION->settings()->get("InstanceDir").toString();
    DesktopServices::openPath(str);
}

void MainWindow::on_actionViewCentralModsFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->settings()->get("CentralModsDir").toString(), true);
}

void MainWindow::on_actionViewSkinsFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->settings()->get("SkinsDir").toString(), true);
}

void MainWindow::on_actionViewIconThemeFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->themeManager()->getIconThemesFolder().path(), true);
}

void MainWindow::on_actionViewWidgetThemeFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->themeManager()->getApplicationThemesFolder().path(), true);
}

void MainWindow::on_actionViewCatPackFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->themeManager()->getCatPacksFolder().path(), true);
}

void MainWindow::on_actionViewIconsFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->icons()->getDirectory(), true);
}

void MainWindow::on_actionViewLogsFolder_triggered()
{
    DesktopServices::openPath("logs", true);
}

void MainWindow::on_actionViewJavaFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->javaPath(), true);
}

void MainWindow::refreshInstances()
{
    APPLICATION->instances()->loadList();
}

void MainWindow::checkForUpdates()
{
    if (APPLICATION->updaterEnabled()) {
        APPLICATION->triggerUpdateCheck();
    } else {
        qWarning() << "Updater not set up. Cannot check for updates.";
    }
}

void MainWindow::on_actionSettings_triggered()
{
    // Galaxy: the settings button toggles the settings page.
    if (m_settingsPanel && m_stack->currentWidget() == m_settingsPanel) {
        closeSettingsPanel();
        return;
    }
    APPLICATION->ShowGlobalSettings(this, "global-settings");
}

void MainWindow::globalSettingsClosed()
{
    proxymodel->invalidate();
    proxymodel->sort(0);
    updateMainToolBar();
    updateLaunchButton();
    updateThemeMenu();
    updateStatusCenter();
    updateCatState();
    if (m_galaxy) {
        m_galaxy->reloadSettings();
        applyWindowFrame();
        m_discord->reloadSettings();
    }
    // This needs to be done to prevent UI elements disappearing in the event the config is changed
    // but Prism Launcher exits abnormally, causing the window state to never be saved:
    APPLICATION->settings()->set("MainWindowState", QString::fromUtf8(saveState().toBase64()));
    update();
}

void MainWindow::on_actionEditInstance_triggered()
{
    if (!m_selectedInstance)
        return;

    // Galaxy: the edit button toggles the profile's page.
    if (m_instancePanels && m_stack->currentWidget() == m_instancePanels) {
        auto* shown = qobject_cast<InstanceWindow*>(m_instancePanels->currentWidget());
        if (shown && shown->instanceId() == m_selectedInstance->id()) {
            leavePanels();
            return;
        }
    }
    if (m_selectedInstance->canEdit()) {
        APPLICATION->showInstanceWindow(m_selectedInstance);
    } else {
        CustomMessageBox::selectable(this, tr("Instance not editable"),
                                     tr("This instance is not editable. It may be broken, invalid, or too old. Check logs for details."),
                                     QMessageBox::Critical)
            ->show();
    }
}

void MainWindow::on_actionManageSkins_triggered()
{
    auto account = APPLICATION->accounts()->defaultAccount();

    if (account && (account->accountType() == AccountType::MSA) && !account->isActive()) {
        SkinManageDialog dialog(this, account);
        dialog.exec();
    }
}

void MainWindow::on_actionManageAccounts_triggered()
{
    APPLICATION->ShowGlobalSettings(this, "accounts");
}

void MainWindow::on_actionReportBug_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.BUG_TRACKER_URL));
}

void MainWindow::on_actionClearMetadata_triggered()
{
    // This if contains side effects!
    if (!APPLICATION->metacache()->evictAll()) {
        CustomMessageBox::selectable(this, tr("Error"),
                                     tr("Metadata cache clear Failed!\nTo clear the metadata cache manually, press Folders -> View "
                                        "Launcher Root Folder, and after closing the launcher delete the folder named \"meta\"\n"),
                                     QMessageBox::Warning)
            ->show();
    }

    APPLICATION->metacache()->SaveNow();
}

#ifdef Q_OS_MAC
void MainWindow::on_actionAddToPATH_triggered()
{
    auto binaryPath = APPLICATION->applicationFilePath();
    auto targetPath = QString("/usr/local/bin/%1").arg(BuildConfig.LAUNCHER_APP_BINARY_NAME);
    qDebug() << "Symlinking" << binaryPath << "to" << targetPath;

    QStringList args;
    args << "-e";
    args << QString("do shell script \"mkdir -p /usr/local/bin && ln -sf '%1' '%2'\" with administrator privileges")
                .arg(binaryPath, targetPath);
    auto outcome = QProcess::execute("/usr/bin/osascript", args);
    if (!outcome) {
        QMessageBox::information(this, tr("Successfully added %1 to PATH").arg(BuildConfig.LAUNCHER_DISPLAYNAME),
                                 tr("%1 was successfully added to your PATH. You can now start it by running `%2`.")
                                     .arg(BuildConfig.LAUNCHER_DISPLAYNAME, BuildConfig.LAUNCHER_APP_BINARY_NAME));
    } else {
        QMessageBox::critical(this, tr("Failed to add %1 to PATH").arg(BuildConfig.LAUNCHER_DISPLAYNAME),
                              tr("An error occurred while trying to add %1 to PATH").arg(BuildConfig.LAUNCHER_DISPLAYNAME));
    }
}
#endif

void MainWindow::on_actionOpenWiki_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.WIKI_URL));
}

void MainWindow::on_actionMoreNews_triggered()
{
    auto entries = m_newsChecker->getNewsEntries();
    NewsDialog news_dialog(entries, this);
    news_dialog.exec();
}

void MainWindow::newsButtonClicked()
{
    auto entries = m_newsChecker->getNewsEntries();
    NewsDialog news_dialog(entries, this);
    news_dialog.toggleArticleList();
    news_dialog.exec();
}

void MainWindow::onCatChanged(int)
{
    setCatBackground(APPLICATION->settings()->get("TheCat").toBool());
}

void MainWindow::on_actionAbout_triggered()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::on_actionDeleteInstance_triggered()
{
    if (!m_selectedInstance) {
        return;
    }

    if (m_selectedInstance->isRunning()) {
        CustomMessageBox::selectable(this, tr("Cannot Delete Running Instance"),
                                     tr("The selected instance is currently running and cannot be deleted. Please stop the instance before "
                                        "attempting to delete it."),
                                     QMessageBox::Warning, QMessageBox::Ok)
            ->exec();
        return;
    }
    auto id = m_selectedInstance->id();

    QString shortcutStr;
    auto shortcuts = m_selectedInstance->shortcuts();
    if (!shortcuts.isEmpty())
        shortcutStr = tr(" and its %n registered shortcut(s)", "", shortcuts.size());
    auto response = CustomMessageBox::selectable(this, tr("Confirm Deletion"),
                                                 tr("You are about to delete \"%1\"%2.\n"
                                                    "This may be permanent and will completely delete the instance.\n\n"
                                                    "Are you sure?")
                                                     .arg(m_selectedInstance->name(), shortcutStr),
                                                 QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                        ->exec();

    if (response != QMessageBox::Yes)
        return;

    if (!checkLinkedInstances(id, this, tr("Deleting")))
        return;

    if (APPLICATION->instances()->trashInstance(id)) {
        ui->actionUndoTrashInstance->setEnabled(APPLICATION->instances()->trashedSomething());
    } else {
        APPLICATION->instances()->deleteInstance(id);
    }
    APPLICATION->settings()->set("SelectedInstance", QString());
    selectionBad();
}

void MainWindow::on_actionExportInstanceZip_triggered()
{
    if (m_selectedInstance) {
        ExportInstanceDialog dlg(m_selectedInstance, this);
        dlg.exec();
    }
}

void MainWindow::on_actionExportInstanceMrPack_triggered()
{
    if (m_selectedInstance) {
        ExportPackDialog dlg(m_selectedInstance, this);
        dlg.exec();
    }
}

void MainWindow::on_actionExportInstanceFlamePack_triggered()
{
    if (m_selectedInstance) {
        if (auto cmp = m_selectedInstance->getPackProfile()->getComponent("net.minecraft");
            cmp && cmp->getVersionFile() && cmp->getVersionFile()->type == "snapshot") {
            QMessageBox msgBox(this);
            msgBox.setText("Snapshots are currently not supported by CurseForge modpacks.");
            msgBox.exec();
            return;
        }
        ExportPackDialog dlg(m_selectedInstance, this, ModPlatform::ResourceProvider::FLAME);
        dlg.exec();
    }
}

void MainWindow::on_actionRenameInstance_triggered()
{
    renameSelectedInstance();
}

void MainWindow::on_actionViewSelectedInstFolder_triggered()
{
    if (m_selectedInstance) {
        QString str = m_selectedInstance->instanceRoot();
        DesktopServices::openPath(QFileInfo(str));
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Galaxy: the settings page and profile windows inside this window close (and save) first.
    if (!closeSettingsPanel()) {
        event->ignore();
        return;
    }
    if (m_instancePanels) {
        while (auto* window = qobject_cast<InstanceWindow*>(m_instancePanels->widget(0))) {
            if (!window->requestClose()) {
                event->ignore();
                return;
            }
            m_instancePanels->removeWidget(window);
        }
    }

    // Save the window state and geometry.
    APPLICATION->settings()->set("MainWindowState", QString::fromUtf8(saveState().toBase64()));
    APPLICATION->settings()->set("MainWindowGeometry", QString::fromUtf8(saveGeometry().toBase64()));
    event->accept();
    emit isClosing();
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    if (event->type() == QEvent::WindowStateChange && m_topBar) {
        m_topBar->updateWindowState();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::instanceActivated(QModelIndex index)
{
    if (!index.isValid())
        return;
    QString id = index.data(InstanceList::InstanceIDRole).toString();
    MinecraftInstance* inst = APPLICATION->instances()->getInstanceById(id);
    if (!inst)
        return;

    if (APPLICATION->settings()->get("EditInstanceOnDoubleClick").toBool()) {
        if (inst->canEdit()) {
            APPLICATION->showInstanceWindow(inst);
        } else {
            CustomMessageBox::selectable(
                this, tr("Instance not editable"),
                tr("This instance is not editable. It may be broken, invalid, or too old. Check logs for details."), QMessageBox::Critical)
                ->show();
        }
        return;
    }
    launchInstance(inst);
}

void MainWindow::on_actionLaunchInstance_triggered()
{
    if (m_selectedInstance && !m_selectedInstance->isRunning()) {
        launchInstance(m_selectedInstance);
    }
}

void MainWindow::on_actionKillInstance_triggered()
{
    if (m_selectedInstance && m_selectedInstance->isRunning()) {
        APPLICATION->kill(m_selectedInstance);
    }
}

void MainWindow::on_actionCreateInstanceShortcut_triggered()
{
    if (!m_selectedInstance)
        return;

    CreateShortcutDialog shortcutDlg(m_selectedInstance, this);
    if (!shortcutDlg.exec())
        return;
    shortcutDlg.createShortcut();
}

void MainWindow::taskEnd()
{
    QObject* sender = QObject::sender();
    if (sender == m_versionLoadTask)
        m_versionLoadTask = NULL;

    sender->deleteLater();
}

void MainWindow::startTask(Task* task)
{
    connect(task, &Task::succeeded, this, &MainWindow::taskEnd);
    connect(task, &Task::failed, this, &MainWindow::taskEnd);
    task->start();
}

void MainWindow::instanceChanged(const QModelIndex& current, [[maybe_unused]] const QModelIndex& previous)
{
    if (!current.isValid()) {
        APPLICATION->settings()->set("SelectedInstance", QString());
        selectionBad();
        return;
    }
    if (m_selectedInstance) {
        disconnect(m_selectedInstance, &BaseInstance::runningStatusChanged, this, &MainWindow::refreshCurrentInstance);
        disconnect(m_selectedInstance, &BaseInstance::profilerChanged, this, &MainWindow::refreshCurrentInstance);
    }
    QString id = current.data(InstanceList::InstanceIDRole).toString();
    m_selectedInstance = APPLICATION->instances()->getInstanceById(id);
    if (m_selectedInstance) {
        ui->instanceToolBar->setEnabled(true);
        setInstanceActionsEnabled(true);
        ui->actionLaunchInstance->setEnabled(m_selectedInstance->canLaunch());

        ui->actionKillInstance->setEnabled(m_selectedInstance->isRunning());
        ui->actionExportInstance->setEnabled(m_selectedInstance->canExport());
        updateNameLabel();
        nameLabel->setToolTip(m_selectedInstance->name());
        m_statusLeft->setText(m_selectedInstance->getStatusbarDescription());
        updateStatusCenter();
        updateInstanceToolIcon(m_selectedInstance->iconKey());

        updateLaunchButton();

        APPLICATION->settings()->set("SelectedInstance", m_selectedInstance->id());

        connect(m_selectedInstance, &BaseInstance::runningStatusChanged, this, &MainWindow::refreshCurrentInstance);
        connect(m_selectedInstance, &BaseInstance::profilerChanged, this, &MainWindow::refreshCurrentInstance);

        if (m_galaxy) {
            m_galaxy->setSelectedInstance(m_selectedInstance->id());
            if (m_listView) {
                m_contents->setInstance(m_selectedInstance);
            }
            updateGalaxyNavigation();
        }
    } else {
        APPLICATION->settings()->set("SelectedInstance", QString());
        selectionBad();
        return;
    }
}

void MainWindow::instanceSelectRequest(QString id)
{
    setSelectedInstanceById(id);
}

void MainWindow::instanceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    auto current = view->selectionModel()->currentIndex();
    QItemSelection test(topLeft, bottomRight);
    if (test.contains(current)) {
        instanceChanged(current, current);
    }
}

void MainWindow::selectionBad()
{
    // start by reseting everything...
    m_selectedInstance = nullptr;
    m_statusLeft->setText(tr("No instance selected"));

    statusBar()->clearMessage();
    ui->instanceToolBar->setEnabled(false);
    setInstanceActionsEnabled(false);
    updateLaunchButton();
    updateNameLabel();
    nameLabel->setToolTip(QString());
    updateInstanceToolIcon("default");
    if (m_galaxy) {
        m_galaxy->setSelectedInstance(QString());
        if (m_listView) {
            m_contents->setInstance(nullptr);
        }
        updateGalaxyNavigation();
    }

    // ...and then see if we can enable the previously selected instance
    setSelectedInstanceById(APPLICATION->settings()->get("SelectedInstance").toString());
}

void MainWindow::checkInstancePathForProblems()
{
    QString instanceFolder = APPLICATION->settings()->get("InstanceDir").toString();
    if (FS::checkProblemticPathJava(QDir(instanceFolder))) {
        QMessageBox warning(this);
        warning.setText(tr("Your instance folder contains \'!\' and this is known to cause Java problems!"));
        warning.setInformativeText(tr("You have now two options: <br/>"
                                      " - change the instance folder in the settings <br/>"
                                      " - move this installation of %1 to a different folder")
                                       .arg(BuildConfig.LAUNCHER_DISPLAYNAME));
        warning.setDefaultButton(QMessageBox::Ok);
        warning.exec();
    }
    auto tempFolderText =
        tr("This is a problem: <br/>"
           " - The launcher will likely be deleted without warning by the operating system <br/>"
           " - close the launcher now and extract it to a real location, not a temporary folder");
    QString pathfoldername = QDir(instanceFolder).absolutePath();
    if (pathfoldername.contains("Rar$", Qt::CaseInsensitive)) {
        QMessageBox warning(this);
        warning.setText(tr("Your instance folder contains \'Rar$\' - that means you haven't extracted the launcher archive!"));
        warning.setInformativeText(tempFolderText);
        warning.setDefaultButton(QMessageBox::Ok);
        warning.exec();
    } else if (pathfoldername.startsWith(QDir::tempPath()) || pathfoldername.contains("/TempState/")) {
        QMessageBox warning(this);
        warning.setText(tr("Your instance folder is in a temporary folder: \'%1\'!").arg(QDir::tempPath()));
        warning.setInformativeText(tempFolderText);
        warning.setDefaultButton(QMessageBox::Ok);
        warning.exec();
    }
}

void MainWindow::updateStatusCenter()
{
    m_statusCenter->setVisible(APPLICATION->settings()->get("ShowGlobalGameTime").toBool());
    int64_t timePlayed = APPLICATION->playtimeSettings()->get("TotalPlayTime").toLongLong();
    if (timePlayed > 0) {
        m_statusCenter->setText(
            tr("Total playtime: %1")
                .arg(Time::prettifyDuration(timePlayed, APPLICATION->settings()->get("ShowGameTimeWithoutDays").toBool())));
    }
}
// "Instance actions" are actions that require an instance to be selected (i.e. "new instance" is not here)
// Actions that also require other conditions (e.g. a running instance) won't be changed.
void MainWindow::setInstanceActionsEnabled(bool enabled)
{
    ui->actionEditInstance->setEnabled(enabled);
    ui->actionChangeInstGroup->setEnabled(enabled);
    ui->actionViewSelectedInstFolder->setEnabled(enabled);
    ui->actionExportInstance->setEnabled(enabled);
    ui->actionDeleteInstance->setEnabled(enabled);
    ui->actionCopyInstance->setEnabled(enabled);
    ui->actionCreateInstanceShortcut->setEnabled(enabled);
}

void MainWindow::refreshCurrentInstance()
{
    auto current = view->selectionModel()->currentIndex();
    instanceChanged(current, current);
}

void MainWindow::updateNameLabel()
{
    if (m_selectedInstance == nullptr) {
        nameLabel->setText(QObject::tr("No Instance Selected"));
    } else {
        const auto metrics = nameLabel->fontMetrics();
        nameLabel->setText(metrics.elidedText(m_selectedInstance->name(), Qt::ElideRight, nameLabel->width()));
    }
}

void MainWindow::updateInstanceToolBarOrientation(Qt::Orientation orientation)
{
    ui->instanceToolBar->removeAction(ui->actionChangeInstIcon);

    if (orientation == Qt::Vertical) {
        instanceInfoWidget->setVisible(true);
    } else {
        instanceInfoWidget->setVisible(false);
        ui->instanceToolBar->insertActionAfter(ui->actionRenameInstance, ui->actionChangeInstIcon);
    }
    // HACK: sync size (no, I don't know why this works)
    ui->instanceToolBar->resize(0, 0);
}

// ---------------------------------------------------------------------------------------------------------------
// Galaxy Launcher

void MainWindow::setupGalaxyUi()
{
    const auto chrome = GalaxyTheme::chromeStyleSheet();

    // Prism's toolbars are replaced by the top bar; their actions stay reachable there, in the menus and in the
    // View menu (the instance and news toolbars can still be shown from there).
    ui->mainToolBar->hide();
    ui->instanceToolBar->hide();
    ui->newsToolBar->hide();
    // Galaxy Launcher uses Modrinth only.
    ui->actionExportInstanceFlamePack->setVisible(false);

    // Top bar
    m_topBar = new GalaxyTopBar(this);
    m_topBar->setStyleSheet(chrome);
    m_topBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* topToolBar = new QToolBar(tr("Galaxy navigation"), this);
    topToolBar->setObjectName("galaxyTopToolBar");
    topToolBar->setMovable(false);
    topToolBar->setFloatable(false);
    topToolBar->setStyleSheet("QToolBar#galaxyTopToolBar { border: 0; padding: 0; spacing: 0; background: #120D2B; }");
    topToolBar->toggleViewAction()->setVisible(false);
    topToolBar->addWidget(m_topBar);
    addToolBar(Qt::TopToolBarArea, topToolBar);
    insertToolBarBreak(ui->mainToolBar);

    m_actionSunColor = new QAction(QIcon::fromTheme("appearance"), tr("Sun color…"), this);
    m_actionSunColor->setToolTip(tr("Choose the color of this profile's sun"));
    connect(m_actionSunColor, &QAction::triggered, this, &MainWindow::pickSunColor);

    m_actionDiscover = new QAction(QIcon::fromTheme("new"), tr("Add content"), this);
    m_actionDiscover->setToolTip(tr("Browse Modrinth and CurseForge and install mods, packs and shaders into this profile"));
    m_actionDiscover->setShortcut(QKeySequence(tr("Ctrl+Shift+D")));
    connect(m_actionDiscover, &QAction::triggered, this, &MainWindow::openDiscover);

    // Three-line ("hamburger") symbol, drawn here so it does not depend on the icon theme.
    QPixmap hamburger(QSize(32, 32));
    hamburger.fill(Qt::transparent);
    {
        QPainter painter(&hamburger);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor(236, 233, 255), 3.2, Qt::SolidLine, Qt::RoundCap));
        for (const int y : { 9, 16, 23 }) {
            painter.drawLine(QPointF(7, y), QPointF(25, y));
        }
    }
    m_actionProfileMenu = new QAction(QIcon(hamburger), tr("More"), this);
    m_actionProfileMenu->setToolTip(tr("All profile actions"));
    auto* profileMenu = new QMenu(this);
    connect(profileMenu, &QMenu::aboutToShow, this, [this, profileMenu] {
        profileMenu->clear();
        populateProfileMenu(profileMenu);
    });
    m_actionProfileMenu->setMenu(profileMenu);

    auto* launchButton = m_topBar->addContextAction(ui->actionLaunchInstance);
    launchButton->setObjectName("galaxyLaunchButton");  // colored: the one action that starts the game
    auto* killButton = m_topBar->addContextAction(ui->actionKillInstance, false);
    killButton->setObjectName("galaxyKillButton");  // a soft red background, like Launch's
    auto* profileButton = m_topBar->addContextAction(m_actionProfileMenu, false);
    profileButton->setPopupMode(QToolButton::InstantPopup);
    profileButton->setToolTip(tr("All profile actions"));
    profileButton->setObjectName("galaxyMoreButton");
    m_topBar->addContextAction(ui->actionEditInstance, false);
    m_topBar->addContextAction(m_actionSunColor, false);
    m_topBar->addGlobalAction(ui->actionAddInstance);
    m_topBar->addGlobalAction(ui->actionFoldersButton);
    m_topBar->addGlobalAction(ui->actionHelpButton);
    if (APPLICATION->updaterEnabled()) {
        m_topBar->addGlobalAction(ui->actionCheckUpdate);
    }
    // Newer Galaxy Launcher release on GitHub: shown right of the help button once found (checked shortly after start).
    m_launcherUpdateAction = new QAction(UpdatesDialog::icon(), tr("Update Galaxy Launcher"), this);
    m_launcherUpdateButton = m_topBar->addGlobalAction(m_launcherUpdateAction);
    m_launcherUpdateButton->setObjectName("galaxyLauncherUpdateButton");
    m_launcherUpdateButton->hide();
    m_launcherUpdate = new LauncherUpdate(this);
    connect(m_launcherUpdate, &LauncherUpdate::available, this, [this](const LauncherUpdate::Release& release) {
        m_launcherUpdateVersion = release.version;
        m_launcherUpdateNotes = release.notes;
        m_launcherUpdatePage = release.page;
        m_launcherUpdateInstaller = release.installer;
        m_launcherUpdateAction->setToolTip(tr("Galaxy Launcher %1 is available: click to update").arg(release.version));
        m_launcherUpdateButton->show();
        Notifications::instance()->post(Notifications::Kind::Info, tr("Galaxy Launcher %1 is available").arg(release.version),
                                        tr("You can update now, or later with the red arrow in the top bar."),
                                        { { tr("See what's new"), [this] { m_launcherUpdateAction->trigger(); } } });
    });
    connect(m_launcherUpdateAction, &QAction::triggered, this, [this] {
        LauncherUpdate::show(this, { m_launcherUpdateVersion, QString(), m_launcherUpdateNotes, m_launcherUpdatePage, m_launcherUpdateInstaller });
    });
    QTimer::singleShot(4000, m_launcherUpdate, &LauncherUpdate::check);
    connect(m_topBar, &GalaxyTopBar::backRequested, this, [this] {
        if (panelShown()) {
            leavePanels();
            return;
        }
        if (m_listView) {
            setListView(false);
        } else {
            m_galaxy->showGalaxy();
        }
    });

    // Sidebar: shares the instance model and selection with the list view.
    m_sidebar = new GalaxySidebar(proxymodel, view->selectionModel(), ui->centralWidget);
    m_sidebar->setStyleSheet(chrome);
    m_sidebar->setAccountMenu(ui->accountsMenu);
    m_sidebar->setCollapsed(APPLICATION->settings()->get(GalaxySettings::SidebarCollapsed).toBool());
    connect(m_sidebar, &GalaxySidebar::profileActivated, this, [this](const QString& id) {
        if (!leavePanels()) {
            return;
        }
        setSelectedInstanceById(id);
        if (!m_listView) {
            m_galaxy->enterSystem(id);
        }
    });
    connect(m_sidebar, &GalaxySidebar::profileDoubleClicked, this,
            [this](const QString& id) { launchInstance(APPLICATION->instances()->getInstanceById(id)); });
    connect(m_sidebar, &GalaxySidebar::profileContextMenuRequested, this, &MainWindow::showProfileMenu);
    connect(m_sidebar, &GalaxySidebar::categoryContextMenuRequested, this, [this](const QString& category, const QPoint& globalPos) {
        QMenu menu(this);
        menu.addAction(QIcon::fromTheme("rename"), tr("Rename category..."), this, [this, category] { renameGroup(category); });
        menu.addAction(QIcon::fromTheme("delete"), tr("Delete category"), this, [this, category] { deleteGroup(category); });
        menu.addSeparator();
        menu.addAction(QIcon::fromTheme("tag"), tr("New category..."), this, &MainWindow::createCategory);
        menu.exec(globalPos);
    });
    connect(m_sidebar, &GalaxySidebar::createCategoryRequested, this, &MainWindow::createCategory);
    connect(m_sidebar, &GalaxySidebar::createProfileRequested, this, &MainWindow::on_actionAddInstance_triggered);
    connect(m_sidebar, &GalaxySidebar::settingsRequested, this, &MainWindow::on_actionSettings_triggered);
    connect(m_sidebar, &GalaxySidebar::collapsedChanged, this,
            [](bool collapsed) { APPLICATION->settings()->set(GalaxySettings::SidebarCollapsed, collapsed); });

    // Galaxy canvas
    m_galaxy = new GalaxyView(ui->centralWidget);
    m_galaxy->findChild<QWidget*>("galaxyEmptyState")->setStyleSheet(chrome);
    connect(m_galaxy, &GalaxyView::instanceSelected, this, &MainWindow::setSelectedInstanceById);
    connect(m_galaxy, &GalaxyView::launchRequested, this, [this](const QString& id) {
        setSelectedInstanceById(id);
        launchInstance(APPLICATION->instances()->getInstanceById(id));
    });
    connect(m_galaxy, &GalaxyView::sunContextMenuRequested, this, &MainWindow::showProfileMenu);
    connect(m_galaxy, &GalaxyView::createCategoryRequested, this, &MainWindow::createCategory);
    connect(m_galaxy, &GalaxyView::bodyActivated, this, [this](const QString& id, const CelestialBody& body) {
        ProjectActions::activate(this, APPLICATION->instances()->getInstanceById(id), body);
    });
    connect(m_galaxy, &GalaxyView::bodyContextMenuRequested, this, [this](const QString& id, const CelestialBody& body, const QPoint& pos) {
        ProjectActions::showContextMenu(this, APPLICATION->instances()->getInstanceById(id), body, pos);
    });
    connect(m_galaxy, &GalaxyView::createProfileRequested, this, &MainWindow::on_actionAddInstance_triggered);
    connect(m_galaxy, &GalaxyView::importProfileRequested, this, [this] { addInstance(QString(), {}, "import"); });
    connect(m_galaxy, &GalaxyView::navigationChanged, this, &MainWindow::updateGalaxyNavigation);
    connect(m_galaxy, &GalaxyView::profilesUpdated, m_sidebar, &GalaxySidebar::refresh);

    // List view: the selected profile's content pages (profiles are picked in the sidebar).
    m_contents = new ProfileContentsPanel(ui->centralWidget);
    m_listSplitter = new QSplitter(Qt::Horizontal, ui->centralWidget);
    m_listSplitter->addWidget(view);
    m_listSplitter->addWidget(m_contents);
    m_listSplitter->setStretchFactor(0, 2);
    m_listSplitter->setStretchFactor(1, 3);
    m_listSplitter->setChildrenCollapsible(false);
    // The sidebar already lists the profiles: Prism's profile grid stays as the (hidden) selection model behind it.
    view->hide();

    m_stack = new QStackedWidget(ui->centralWidget);
    m_stack->addWidget(m_galaxy);
    m_stack->addWidget(m_listSplitter);

    // Bottom search bar
    m_searchBar = new GalaxySearchBar(ui->centralWidget);
    m_searchBar->setStyleSheet(chrome);
    connect(m_searchBar, &GalaxySearchBar::filterChanged, m_galaxy, &GalaxyView::setFilter);
    connect(m_searchBar, &GalaxySearchBar::nextMatchRequested, m_galaxy, &GalaxyView::focusNextMatch);
    connect(m_searchBar, &GalaxySearchBar::addContentRequested, this, &MainWindow::openDiscover);
    connect(m_galaxy, &GalaxyView::filterMatchesChanged, m_searchBar, &GalaxySearchBar::setMatchCount);
    connect(m_searchBar, &GalaxySearchBar::viewToggleRequested, this, [this] { setListView(!m_listView); });
    connect(m_searchBar, &GalaxySearchBar::updatesRequested, this, [this] {
        if (auto* instance = dynamic_cast<MinecraftInstance*>(APPLICATION->instances()->getInstanceById(m_galaxy->currentSystemId()))) {
            ProjectActions::updateAll(this, instance);
        }
    });
    connect(UpdateChecker::instance(), &UpdateChecker::updatesChanged, this, &MainWindow::refreshUpdatesButton);
    // A notification when a profile has new updates (not again for the same ones).
    connect(UpdateChecker::instance(), &UpdateChecker::updatesChanged, this, [this](const QString& id) {
        const auto count = static_cast<int>(UpdateChecker::instance()->updatesFor(id).size());
        const int known = m_notifiedUpdates.value(id);
        m_notifiedUpdates[id] = count;
        auto* instance = dynamic_cast<MinecraftInstance*>(APPLICATION->instances()->getInstanceById(id));
        if (!instance || count <= known) {
            return;
        }
        Notifications::instance()->post(Notifications::Kind::Info, tr("Updates for %1").arg(instance->name()),
                                        tr("%n project(s) can be updated.", nullptr, count),
                                        { { tr("Review updates"), [this, id] {
                                               if (auto* target = dynamic_cast<MinecraftInstance*>(APPLICATION->instances()->getInstanceById(id))) {
                                                   ProjectActions::updateAll(this, target);
                                               }
                                           } } });
    });

    auto* column = new QWidget(ui->centralWidget);
    auto* columnLayout = new QVBoxLayout(column);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->setSpacing(0);
    // Discover opens beside the galaxy (or the list) rather than in a window of its own.
    m_discoverSplit = new QSplitter(Qt::Horizontal, column);
    m_discoverSplit->setObjectName("galaxyDiscoverSplit");
    m_discoverSplit->setChildrenCollapsible(false);
    m_discoverSplit->setHandleWidth(4);
    m_discoverSplit->addWidget(m_stack);
    DiscoverWindow::setHost(m_discoverSplit);
    columnLayout->addWidget(m_discoverSplit, 1);
    columnLayout->addWidget(m_searchBar);
    // Notifications: top right, under the top bar.
    new NotificationArea(column);

    ui->horizontalLayout->addWidget(m_sidebar);
    ui->horizontalLayout->addWidget(column, 1);

    // Keyboard access to the new views.
    m_actionListView = new QAction(tr("&List View"), this);
    m_actionListView->setCheckable(true);
    m_actionListView->setShortcut(QKeySequence(tr("Ctrl+L")));
    connect(m_actionListView, &QAction::triggered, this, [this](bool checked) { setListView(checked); });
    auto* searchAction = new QAction(tr("&Search Projects"), this);
    searchAction->setShortcut(QKeySequence::Find);
    connect(searchAction, &QAction::triggered, m_searchBar, &GalaxySearchBar::focusSearch);
    auto* backAction = new QAction(tr("Back to &Galaxy"), this);
    backAction->setShortcut(QKeySequence::Back);
    connect(backAction, &QAction::triggered, m_topBar, &GalaxyTopBar::backRequested);
    ui->viewMenu->insertActions(ui->actionChangeTheme, { m_actionListView, searchAction, backAction, m_actionDiscover });
    ui->viewMenu->insertSeparator(ui->actionChangeTheme);
    addAction(m_actionListView);
    addAction(searchAction);
    addAction(backAction);
    addAction(m_actionDiscover);

    setListView(GalaxySettings::initialView(APPLICATION->settings()) == "list", false);

    // No status bar (see setStatusBarVisibility()), so no menu entry for it.
    ui->actionToggleStatusBar->setVisible(false);

    m_discord = new DiscordPresence(this);

    // Custom window frame: buttons in the top bar, resizing from the edges.
    qApp->installEventFilter(this);
    applyWindowFrame();

    // Developer tooling: GALAXY_DEV_SNAPSHOTS=<dir> walks through the main views and saves screenshots there.
    if (const auto snapshots = qEnvironmentVariable("GALAXY_DEV_SNAPSHOTS"); !snapshots.isEmpty()) {
        QTimer::singleShot(1500, this, [this, snapshots] { GalaxyDevTour::start(this, snapshots); });
    }
}

void MainWindow::setListView(bool list, bool remember)
{
    if (panelShown() && !leavePanels()) {
        return;
    }
    m_listView = list;
    m_stack->setCurrentIndex(list ? 1 : 0);
    m_searchBar->setListView(list);
    if (m_actionListView) {
        m_actionListView->setChecked(list);
    }
    if (list) {
        m_contents->setInstance(m_selectedInstance);
    } else {
        // Close the pages so the list view stops watching folders it no longer shows.
        m_contents->release();
    }
    (list ? static_cast<QWidget*>(m_contents) : m_galaxy)->setFocus();
    if (remember) {
        APPLICATION->settings()->set(GalaxySettings::LastView, list ? "list" : "galaxy");
    }
    updateGalaxyNavigation();
}

void MainWindow::refreshUpdatesButton()
{
    // The galaxy's bottom bar offers the open profile's updates; the list view has its own button.
    const auto systemId = m_listView ? QString() : m_galaxy->currentSystemId();
    m_searchBar->setUpdateCount(systemId.isEmpty() ? 0 : static_cast<int>(UpdateChecker::instance()->updatesFor(systemId).size()));
}

void MainWindow::updateGalaxyNavigation()
{
    if (!m_topBar) {
        return;
    }
    m_searchBar->setVisible(!panelShown());
    if (m_settingsPanel && m_stack->currentWidget() == m_settingsPanel) {
        m_topBar->setLocation(tr("Settings"), QString(), true);
        setWindowTitle(tr("Settings — %1").arg(APPLICATION->applicationDisplayName()));
        return;
    }
    if (m_instancePanels && m_stack->currentWidget() == m_instancePanels) {
        auto* window = qobject_cast<InstanceWindow*>(m_instancePanels->currentWidget());
        auto* instance = window ? APPLICATION->instances()->getInstanceById(window->instanceId()) : nullptr;
        const auto name = instance ? instance->name() : QString();
        m_topBar->setLocation(tr("Profile"), name, true);
        setWindowTitle(QStringLiteral("%1 — %2").arg(name, APPLICATION->applicationDisplayName()));
        return;
    }
    QString profile;
    if (m_listView) {
        profile = m_selectedInstance ? m_selectedInstance->name() : QString();
        m_topBar->setLocation(tr("List"), profile, true);
    } else {
        const auto systemId = m_galaxy->currentSystemId();
        if (auto* inst = systemId.isEmpty() ? nullptr : APPLICATION->instances()->getInstanceById(systemId)) {
            profile = inst->name();
        }
        m_topBar->setLocation(tr("Galaxy"), profile, !systemId.isEmpty());
    }
    m_searchBar->setScope(!m_listView && !m_galaxy->currentSystemId().isEmpty());
    m_searchBar->setMatchCount(m_galaxy->matchCount());
    refreshUpdatesButton();
    m_topBar->setContextVisible(m_selectedInstance != nullptr);
    m_actionSunColor->setEnabled(m_selectedInstance != nullptr);
    m_actionProfileMenu->setEnabled(m_selectedInstance != nullptr);
    setWindowTitle(profile.isEmpty() ? APPLICATION->applicationDisplayName()
                                     : QStringLiteral("%1 — %2").arg(profile, APPLICATION->applicationDisplayName()));
}

void MainWindow::launchInstance(MinecraftInstance* instance)
{
    if (!instance) {
        return;
    }
    if (instance->isRunning()) {
        // Prism shows the running game's console in this case.
        APPLICATION->launch(instance);
        return;
    }
    const bool willLaunch = instance->canLaunch();
    // The real launch starts right away through Prism's pipeline; the animation only plays alongside it.
    if (!APPLICATION->launch(instance)) {
        CustomMessageBox::selectable(this, tr("Cannot launch"),
                                     tr("\"%1\" could not be launched. The profile may be invalid or incomplete, or the launcher is "
                                        "busy updating. Check the logs (Help › View Launcher Logs) for details.")
                                         .arg(instance->name()),
                                     QMessageBox::Warning)
            ->show();
        return;
    }
    if (willLaunch) {
        m_galaxy->playLaunchAnimation(instance->id());
    }
}

void MainWindow::populateProfileMenu(QMenu* menu)
{
    if (!m_selectedInstance) {
        return;
    }
    auto* instance = m_selectedInstance;
    auto* header = menu->addAction(instance->name());
    header->setEnabled(false);
    menu->addSeparator();
    auto* launch = menu->addAction(QIcon::fromTheme("launch"), instance->isRunning() ? tr("Show game console") : tr("Launch"));
    connect(launch, &QAction::triggered, this, [this, instance] { launchInstance(instance); });
    auto* launchOptions = menu->addMenu(tr("Launch options"));
    instance->populateLaunchMenu(launchOptions);
    if (instance->isRunning()) {
        menu->addAction(ui->actionKillInstance);
    }
    menu->addAction(m_actionDiscover);
    menu->addSeparator();
    menu->addAction(ui->actionRenameInstance);
    menu->addAction(m_actionSunColor);
    menu->addAction(ui->actionChangeInstIcon);
    if (const auto key = instance->iconKey(); !key.isEmpty() && key != QStringLiteral("default")) {
        // Galaxy: back to no icon ("default"); the sidebar then shows the name without one.
        menu->addAction(QIcon::fromTheme("delete"), tr("Remove Icon"), this, [id = instance->id()] {
            if (auto* target = APPLICATION->instances()->getInstanceById(id)) {
                target->setIconKey(QStringLiteral("default"));
            }
        });
    }
    menu->addAction(ui->actionChangeInstGroup);
    menu->addSeparator();
    menu->addAction(ui->actionEditInstance);
    menu->addAction(ui->actionViewSelectedInstFolder);
    menu->addAction(ui->actionCopyInstance);
    menu->addAction(ui->actionExportInstance);
    menu->addAction(ui->actionCreateInstanceShortcut);
    menu->addSeparator();
    menu->addAction(ui->actionDeleteInstance);
}

void MainWindow::showProfileMenu(const QString& instanceId, const QPoint& globalPos)
{
    setSelectedInstanceById(instanceId);
    if (!m_selectedInstance || m_selectedInstance->id() != instanceId) {
        return;
    }
    QMenu menu(this);
    populateProfileMenu(&menu);
    menu.exec(globalPos);
}

void MainWindow::renameSelectedInstance()
{
    if (!m_selectedInstance) {
        return;
    }
    if (m_listView) {
        view->edit(view->currentIndex());
        return;
    }
    bool ok = false;
    const QString before = m_selectedInstance->name();
    const QString after =
        QInputDialog::getText(this, tr("Rename profile"), tr("New name for \"%1\":").arg(before), QLineEdit::Normal, before, &ok).trimmed();
    if (!ok || after.isEmpty() || after == before) {
        return;
    }
    // Same path as renaming in the list view: through the instance model, then the optional folder rename.
    const auto index = APPLICATION->instances()->getInstanceIndexById(m_selectedInstance->id());
    if (APPLICATION->instances()->setData(index, after, Qt::EditRole)) {
        applyInstanceRename(before, after);
    }
}

void MainWindow::applyInstanceRename(const QString& before, const QString& after)
{
    if (auto newRoot = askToUpdateInstanceDirName(m_selectedInstance, before, after, this); !newRoot.isEmpty()) {
        auto oldID = m_selectedInstance->id();
        auto newID = QFileInfo(newRoot).fileName();
        QString origGroup(APPLICATION->instances()->getInstanceGroup(oldID));
        bool syncGroup = origGroup != GroupId() && oldID != newID;
        if (syncGroup)
            APPLICATION->instances()->setInstanceGroup(oldID, GroupId());

        refreshInstances();
        setSelectedInstanceById(newID);

        if (syncGroup)
            APPLICATION->instances()->setInstanceGroup(newID, origGroup);
    }
    updateGalaxyNavigation();
}

void MainWindow::pickSunColor()
{
    if (m_selectedInstance && SunColor::pick(m_selectedInstance, this)) {
        m_galaxy->refreshProfiles();
        m_sidebar->refresh();
    }
}

MinecraftInstance* MainWindow::instanceForInstall()
{
    // The profile being looked at; Discover has its own profile picker to change it.
    if (const auto systemId = m_galaxy->currentSystemId(); !m_listView && !systemId.isEmpty()) {
        return APPLICATION->instances()->getInstanceById(systemId);
    }
    if (m_selectedInstance) {
        return m_selectedInstance;
    }
    auto* list = APPLICATION->instances();
    return list->count() > 0 ? list->at(0) : nullptr;
}

void MainWindow::openDiscover()
{
    // Galaxy: the Add content button toggles Discover.
    if (DiscoverWindow::isOpenBeside()) {
        DiscoverWindow::closeBeside();
        return;
    }
    if (panelShown() && !leavePanels()) {
        return;  // Discover goes beside the galaxy or the list, not beside settings
    }
    if (APPLICATION->instances()->count() == 0) {
        const auto response = QMessageBox::question(this, tr("No profiles yet"),
                                                    tr("Mods, packs and shaders are installed into a profile. Create one now?"),
                                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (response == QMessageBox::Yes) {
            on_actionAddInstance_triggered();
        }
        return;
    }
    ProjectActions::openDownloadDialog(this, instanceForInstall(), ProjectActions::Category::Mods);
}

bool MainWindow::panelShown() const
{
    return m_stack && ((m_settingsPanel && m_stack->currentWidget() == m_settingsPanel) ||
                       (m_instancePanels && m_stack->currentWidget() == m_instancePanels));
}

bool MainWindow::showSettingsPanel(const QString& page)
{
    if (!m_stack || QApplication::activeModalWidget() || !APPLICATION->globalSettingsProvider()) {
        return false;
    }
    if (!DiscoverWindow::closeBeside()) {
        return true;  // the user kept Discover (a selection is not installed yet)
    }
    if (!m_settingsPanel) {
        // A profile window that is shown closes first (it saves on close), like switching to another window did.
        if (m_instancePanels && m_stack->currentWidget() == m_instancePanels) {
            if (auto* window = qobject_cast<InstanceWindow*>(m_instancePanels->currentWidget()); window && !window->requestClose()) {
                return true;
            }
        }
        m_settingsPanel = new QWidget(m_stack);
        m_settingsPanel->setObjectName("galaxySettingsPanel");
        auto* layout = new QVBoxLayout(m_settingsPanel);
        layout->setContentsMargins(0, 0, 0, 0);
        m_settingsContainer = new PageContainer(APPLICATION->globalSettingsProvider(), page, m_settingsPanel);
        layout->addWidget(m_settingsContainer);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Help | QDialogButtonBox::Close, m_settingsPanel);
        buttons->button(QDialogButtonBox::Close)->setText(tr("Done"));
        buttons->button(QDialogButtonBox::Close)->setToolTip(tr("Apply the settings and go back"));
        buttons->button(QDialogButtonBox::Help)->setText(tr("Help"));
        buttons->setContentsMargins(0, 0, 6, 6);
        m_settingsContainer->addButtons(buttons);
        connect(buttons->button(QDialogButtonBox::Close), &QPushButton::clicked, this, [this] { closeSettingsPanel(); });
        connect(buttons->button(QDialogButtonBox::Help), &QPushButton::clicked, m_settingsContainer, &PageContainer::help);
        m_stack->addWidget(m_settingsPanel);
    } else if (!page.isEmpty()) {
        m_settingsContainer->selectPage(page);
    }
    m_stack->setCurrentWidget(m_settingsPanel);
    updateGalaxyNavigation();
    return true;
}

bool MainWindow::closeSettingsPanel()
{
    if (!m_settingsPanel) {
        return true;
    }
    if (!m_settingsContainer->prepareToClose()) {  // applies every page
        return false;
    }
    auto* panel = m_settingsPanel;
    const bool shown = m_stack->currentWidget() == panel;
    m_settingsPanel = nullptr;
    m_settingsContainer = nullptr;
    m_stack->removeWidget(panel);
    panel->deleteLater();
    if (shown) {
        m_stack->setCurrentIndex(m_listView ? 1 : 0);
    }
    APPLICATION->globalSettingsClosed();
    updateGalaxyNavigation();
    return true;
}

QWidget* MainWindow::instanceHost()
{
    if (!m_stack) {
        return nullptr;
    }
    if (!m_instancePanels) {
        m_instancePanels = new QStackedWidget(m_stack);
        m_instancePanels->setObjectName("galaxyProfilePanels");
        m_stack->addWidget(m_instancePanels);
    }
    return m_instancePanels;
}

void MainWindow::showInstancePanel(InstanceWindow* window)
{
    if (!instanceHost() || !window) {
        return;
    }
    if ((m_settingsPanel && !closeSettingsPanel()) || !DiscoverWindow::closeBeside()) {
        return;
    }
    if (m_instancePanels->indexOf(window) < 0) {
        m_instancePanels->addWidget(window);
        // Closed (its own Close button, Back, or another profile picked): back to what it covered.
        connect(window, &InstanceWindow::isClosing, this, [this, window] {
            m_instancePanels->removeWidget(window);
            if (m_stack->currentWidget() == m_instancePanels) {
                m_stack->setCurrentIndex(m_listView ? 1 : 0);
            }
            updateGalaxyNavigation();
        });
    }
    m_instancePanels->setCurrentWidget(window);
    m_stack->setCurrentWidget(m_instancePanels);
    updateGalaxyNavigation();
}

bool MainWindow::leavePanels()
{
    if (!closeSettingsPanel()) {
        return false;
    }
    if (m_instancePanels && m_stack->currentWidget() == m_instancePanels) {
        if (auto* window = qobject_cast<InstanceWindow*>(m_instancePanels->currentWidget()); window && !window->requestClose()) {
            return false;
        }
        m_stack->setCurrentIndex(m_listView ? 1 : 0);
        updateGalaxyNavigation();
    }
    return true;
}

void MainWindow::applyWindowFrame()
{
    const bool frameless = !APPLICATION->settings()->get(GalaxySettings::SystemTitleBar).toBool();
    if (windowFlags().testFlag(Qt::FramelessWindowHint) != frameless) {
        const bool wasVisible = isVisible();
        setWindowFlag(Qt::FramelessWindowHint, frameless);
        if (wasVisible) {
            show();  // changing window flags hides the window
        }
    }
    m_topBar->setWindowControlsVisible(frameless);
}

bool MainWindow::handleFramelessResize(QObject* obj, QEvent* ev)
{
    if (ev->type() != QEvent::MouseMove && ev->type() != QEvent::MouseButtonPress && ev->type() != QEvent::Leave) {
        return false;
    }
    if (!windowFlags().testFlag(Qt::FramelessWindowHint) || isMaximized() || isFullScreen()) {
        if (m_resizeCursor) {
            QGuiApplication::restoreOverrideCursor();
            m_resizeCursor = false;
        }
        return false;
    }
    auto* widget = qobject_cast<QWidget*>(obj);
    if (!widget || widget->window() != this) {
        return false;
    }
    Qt::Edges edges;
    if (ev->type() != QEvent::Leave) {
        auto* mouse = static_cast<QMouseEvent*>(ev);
        if (ev->type() == QEvent::MouseMove && mouse->buttons() != Qt::NoButton) {
            return false;  // a drag inside the window
        }
        constexpr int margin = 6;
        const QPoint local = mouse->globalPosition().toPoint() - geometry().topLeft();
        if (local.x() < margin) {
            edges |= Qt::LeftEdge;
        }
        if (local.x() >= width() - margin) {
            edges |= Qt::RightEdge;
        }
        if (local.y() < margin) {
            edges |= Qt::TopEdge;
        }
        if (local.y() >= height() - margin) {
            edges |= Qt::BottomEdge;
        }
        if (edges && ev->type() == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton && windowHandle()) {
            return windowHandle()->startSystemResize(edges);
        }
    }
    // Resize cursors near the edges.
    Qt::CursorShape shape = Qt::ArrowCursor;
    if ((edges & Qt::LeftEdge && edges & Qt::TopEdge) || (edges & Qt::RightEdge && edges & Qt::BottomEdge)) {
        shape = Qt::SizeFDiagCursor;
    } else if ((edges & Qt::RightEdge && edges & Qt::TopEdge) || (edges & Qt::LeftEdge && edges & Qt::BottomEdge)) {
        shape = Qt::SizeBDiagCursor;
    } else if (edges & (Qt::LeftEdge | Qt::RightEdge)) {
        shape = Qt::SizeHorCursor;
    } else if (edges & (Qt::TopEdge | Qt::BottomEdge)) {
        shape = Qt::SizeVerCursor;
    }
    if (edges && !m_resizeCursor) {
        QGuiApplication::setOverrideCursor(shape);
        m_resizeCursor = true;
    } else if (edges) {
        QGuiApplication::changeOverrideCursor(shape);
    } else if (m_resizeCursor) {
        QGuiApplication::restoreOverrideCursor();
        m_resizeCursor = false;
    }
    return false;
}
