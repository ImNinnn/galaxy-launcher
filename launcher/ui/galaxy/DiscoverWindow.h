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

#pragma once

#include <QDialog>
#include <QPointer>

#include <functional>

#include "ProjectActions.h"

class MinecraftInstance;
class QComboBox;
class QLabel;
class QSplitter;
class QTabBar;
class QVBoxLayout;

namespace ResourceDownload {
class ResourceDownloadDialog;
}

/**
 * "Discover": a window to search Modrinth and install mods, resource packs, shader packs and data
 * packs into a profile.
 *
 * Each category hosts Prism's own download browser (provider list, search, sorting, filters such as Minecraft
 * version, loader, categories and environment, project descriptions, version selection, dependency resolution and
 * a review step), so behavior and compatibility checks match the instance window. Nothing is installed until the
 * user reviews and confirms; the window stays open for more searches afterwards.
 */
class DiscoverWindow : public QDialog {
    Q_OBJECT
   public:
    /** Opens (or raises) the window for a profile and category, optionally pre-filled with a search. */
    static void showFor(QWidget* parent, MinecraftInstance* instance, ProjectActions::Category category, const QString& search = QString());
    /** Galaxy: open Discover beside the views in this splitter (the main window's) instead of as a window. */
    static void setHost(QSplitter* host);
    /** Galaxy: whether Discover is open beside the views. */
    static bool isOpenBeside();
    /** Galaxy: closes Discover beside the views; false if it stays (the user kept an uninstalled selection). */
    static bool closeBeside();

   public slots:
    void reject() override;

   protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

   private:
    DiscoverWindow(QWidget* parent, bool embedded);

    void setTarget(MinecraftInstance* instance, ProjectActions::Category category, const QString& search);
    void rebuild(const QString& search = QString());
    void reloadProfiles();
    /** True if there is nothing selected for download, or the user agrees to discard the selection. */
    bool confirmDiscard();
    void showPlaceholder(const QString& text, const QString& buttonText, const std::function<void()>& action);

    static QPointer<DiscoverWindow> s_window;
    static QPointer<QSplitter> s_host;
    bool m_embedded = false;

    QComboBox* m_profile = nullptr;
    QTabBar* m_tabs = nullptr;
    QLabel* m_status = nullptr;
    QWidget* m_host = nullptr;
    QVBoxLayout* m_hostLayout = nullptr;
    QWidget* m_current = nullptr;
    QPointer<ResourceDownload::ResourceDownloadDialog> m_dialog;
    QPointer<MinecraftInstance> m_instance;
    ProjectActions::Category m_category = ProjectActions::Category::Mods;
    bool m_switching = false;
};
