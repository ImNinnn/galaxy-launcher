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

#pragma once

#include <QMainWindow>
#include <QProcess>
#include <QTimer>
#include <QUrl>

#include "minecraft/auth/MinecraftAccount.h"

class LaunchController;
class NewsChecker;
class QToolButton;
class InstanceProxyModel;
class QLabel;
class MinecraftInstance;
class MinecraftLauncher;
class BaseProfilerFactory;
class InstanceView;
class KonamiCode;
class InstanceTask;
class GalaxyView;
class GalaxySidebar;
class GalaxyTopBar;
class GalaxySearchBar;
class ProfileContentsPanel;
class QStackedWidget;
class QSplitter;

namespace Ui {
class MainWindow;
}
class PageContainer;
class InstanceWindow;
class MainWindow : public QMainWindow {
    Q_OBJECT

   public:
    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();

    bool eventFilter(QObject* obj, QEvent* ev) override;
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

    void checkInstancePathForProblems();

    void updatesAllowedChanged(bool allowed);

    void processURLs(QList<QUrl> urls);

    // Galaxy: launcher settings and profile windows are pages of the main window (Discover splits the view).
    /** Shows the settings as a page of the main window; false when it cannot (a dialog is open over it). */
    bool showSettingsPanel(const QString& page);
    /** Where profile windows go when they open inside the main window. */
    QWidget* instanceHost();
    /** Shows a profile window that is a page of the main window. */
    void showInstancePanel(InstanceWindow* window);

   signals:
    void isClosing();

   protected:
    QMenu* createPopupMenu() override;

   private slots:
    void onCatToggled(bool);

    void onCatChanged(int);

    void on_actionAbout_triggered();

    void on_actionAddInstance_triggered();

    void on_actionREDDIT_triggered();

    void on_actionMATRIX_triggered();

    void on_actionDISCORD_triggered();

    void on_actionCopyInstance_triggered();

    void on_actionChangeInstGroup_triggered();

    void on_actionChangeInstIcon_triggered();

    void on_actionViewLauncherRootFolder_triggered();

    void on_actionViewInstanceFolder_triggered();
    void on_actionViewCentralModsFolder_triggered();

    void on_actionViewIconThemeFolder_triggered();
    void on_actionViewWidgetThemeFolder_triggered();
    void on_actionViewCatPackFolder_triggered();
    void on_actionViewIconsFolder_triggered();
    void on_actionViewLogsFolder_triggered();
    void on_actionViewJavaFolder_triggered();

    void on_actionViewSkinsFolder_triggered();

    void on_actionViewSelectedInstFolder_triggered();

    void refreshInstances();

    void checkForUpdates();

    void on_actionSettings_triggered();

    void on_actionManageSkins_triggered();

    void on_actionManageAccounts_triggered();

    void on_actionReportBug_triggered();

    void on_actionClearMetadata_triggered();

#ifdef Q_OS_MAC
    void on_actionAddToPATH_triggered();
#endif

    void on_actionOpenWiki_triggered();

    void on_actionMoreNews_triggered();

    void newsButtonClicked();

    void on_actionLaunchInstance_triggered();

    void on_actionKillInstance_triggered();

    void on_actionDeleteInstance_triggered();

    void deleteGroup(QString group);
    /** Galaxy: asks for a new category and the profiles to put in it. */
    void createCategory();
    void renameGroup(QString group);
    void undoTrashInstance();

    inline void on_actionExportInstance_triggered() { on_actionExportInstanceZip_triggered(); }
    void on_actionExportInstanceZip_triggered();
    void on_actionExportInstanceMrPack_triggered();
    void on_actionExportInstanceFlamePack_triggered();

    void on_actionRenameInstance_triggered();

    void on_actionEditInstance_triggered();

    void on_actionCreateInstanceShortcut_triggered();

    void taskEnd();

    /**
     * called when an icon is changed in the icon model.
     */
    void iconUpdated(QString);

    void showInstanceContextMenu(const QPoint&);

    void updateMainToolBar();

    void updateLaunchButton();

    void updateThemeMenu();

    void instanceActivated(QModelIndex);

    void instanceChanged(const QModelIndex& current, const QModelIndex& previous);

    void instanceSelectRequest(QString id);

    void instanceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);

    void selectionBad();

    void startTask(Task* task);

    void defaultAccountChanged();

    void changeActiveAccount();

    void repopulateAccountsMenu();

    void updateNewsLabel();

    void konamiTriggered();

    void globalSettingsClosed();

    void setStatusBarVisibility(bool);

    void lockToolbars(bool);

#ifndef Q_OS_MAC
    void keyReleaseEvent(QKeyEvent* event) override;
#endif

    void refreshCurrentInstance();

   private:
    void retranslateUi();

    void updateNameLabel();

    void updateInstanceToolBarOrientation(Qt::Orientation orientation);

    void addInstance(const QString& url = QString(), const QMap<QString, QString>& extra_info = {}, const QString& initialPage = QString());
    void setCatBackground(bool enabled);
    void updateCatState();
    void updateInstanceToolIcon(QString new_icon);
    void setSelectedInstanceById(const QString& id);
    void updateStatusCenter();
    void setInstanceActionsEnabled(bool enabled);

    void runModalTask(Task* task, const QString& title = {});
    void instanceFromInstanceTask(InstanceTask* task);

    // Galaxy Launcher
    void setupGalaxyUi();
    void setListView(bool list, bool remember = true);
    void updateGalaxyNavigation();
    void refreshUpdatesButton();
    void launchInstance(MinecraftInstance* instance);
    void populateProfileMenu(QMenu* menu);
    void showProfileMenu(const QString& instanceId, const QPoint& globalPos);
    void renameSelectedInstance();
    void applyInstanceRename(const QString& before, const QString& after);
    void pickSunColor();
    MinecraftInstance* instanceForInstall();
    void openDiscover();
    /** Custom (frameless) window with buttons in the top bar, or the window manager's title bar. */
    void applyWindowFrame();
    /** Edge resizing for the frameless window; returns true if the event was used. */
    bool handleFramelessResize(QObject* obj, QEvent* ev);
    /** Applies and closes the settings page; false if a page refused (unsaved input it could not apply). */
    bool closeSettingsPanel();
    /** Leaves the settings page or the shown profile window, back to the galaxy or the list. */
    bool leavePanels();
    /** True while the settings page or a profile window covers the galaxy or the list. */
    bool panelShown() const;

    friend class GalaxyDevTour;

   private:
    Ui::MainWindow* ui;
    // these are managed by Qt's memory management model!
    InstanceView* view = nullptr;
    InstanceProxyModel* proxymodel = nullptr;
    QToolButton* newsLabel = nullptr;
    QLabel* m_statusLeft = nullptr;
    QLabel* m_statusCenter = nullptr;
    QToolButton* changeIconButton = nullptr;
    QAction* instanceInfoWidget = nullptr;
    QLabel* nameLabel = nullptr;
    QToolButton* helpMenuButton = nullptr;
    KonamiCode* secretEventFilter = nullptr;

    unique_qobject_ptr<NewsChecker> m_newsChecker;

    MinecraftInstance* m_selectedInstance = nullptr;
    QString m_currentInstIcon;

    // managed by the application object
    Task* m_versionLoadTask = nullptr;

    // Galaxy Launcher chrome (owned by Qt's parent/child tree)
    GalaxyView* m_galaxy = nullptr;
    GalaxySidebar* m_sidebar = nullptr;
    GalaxyTopBar* m_topBar = nullptr;
    GalaxySearchBar* m_searchBar = nullptr;
    ProfileContentsPanel* m_contents = nullptr;
    QStackedWidget* m_stack = nullptr;
    QSplitter* m_listSplitter = nullptr;
    QSplitter* m_discoverSplit = nullptr;       ///< The views, and Discover beside them when it is open.
    QWidget* m_settingsPanel = nullptr;         ///< The settings page while it is open.
    PageContainer* m_settingsContainer = nullptr;
    QStackedWidget* m_instancePanels = nullptr;  ///< Profile windows opened inside the main window.
    QAction* m_actionSunColor = nullptr;
    QAction* m_actionProfileMenu = nullptr;
    QAction* m_actionDiscover = nullptr;
    QAction* m_actionListView = nullptr;
    bool m_listView = false;
    bool m_resizeCursor = false;
    class DiscordPresence* m_discord = nullptr;
    // New Galaxy Launcher versions on GitHub: a red download arrow right of the help button.
    class LauncherUpdate* m_launcherUpdate = nullptr;
    QAction* m_launcherUpdateAction = nullptr;
    QToolButton* m_launcherUpdateButton = nullptr;
    QString m_launcherUpdateVersion;
    QString m_launcherUpdateNotes;
    QUrl m_launcherUpdatePage;
    QUrl m_launcherUpdateInstaller;
    QHash<QString, int> m_notifiedUpdates;  ///< Galaxy: update counts already announced per profile.
};
