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

#include <QWidget>

class QAbstractItemModel;
class QItemSelectionModel;
class QLabel;
class QListView;
class QMenu;
class QToolButton;
class SidebarCategoryModel;

/**
 * Left sidebar: every profile (as its sun), account management and launcher settings.
 *
 * Profiles with a category (Prism's instance groups) are listed under a header per category that collapses and
 * expands them; profiles without one come first, and without any category there are no headers. The list follows
 * Prism's selection model, so selection is always in sync between the sidebar, the list view and the galaxy.
 */
class GalaxySidebar : public QWidget {
    Q_OBJECT
   public:
    /** True for the category header rows of the sidebar's list (its model's data role). */
    static constexpr int CategoryHeaderRole = Qt::UserRole + 200;

    GalaxySidebar(QAbstractItemModel* profiles, QItemSelectionModel* selection, QWidget* parent = nullptr);

    void setAccountMenu(QMenu* menu);
    void setAccount(const QString& name, const QIcon& face);
    void setCollapsed(bool collapsed);
    bool isCollapsed() const { return m_collapsed; }
    /** Repaints suns and running badges. */
    void refresh();

   signals:
    void profileActivated(const QString& instanceId);
    void profileDoubleClicked(const QString& instanceId);
    void profileContextMenuRequested(const QString& instanceId, const QPoint& globalPos);
    void categoryContextMenuRequested(const QString& category, const QPoint& globalPos);
    void createProfileRequested();
    void createCategoryRequested();
    void settingsRequested();
    void collapsedChanged(bool collapsed);

   private:
    void selectFromShared();

    QListView* m_list = nullptr;
    SidebarCategoryModel* m_model = nullptr;
    QItemSelectionModel* m_shared = nullptr;
    bool m_syncing = false;
    QLabel* m_title = nullptr;
    QToolButton* m_add = nullptr;
    QToolButton* m_addCategory = nullptr;
    QToolButton* m_collapse = nullptr;
    QToolButton* m_account = nullptr;
    QToolButton* m_settings = nullptr;
    bool m_collapsed = false;
};
