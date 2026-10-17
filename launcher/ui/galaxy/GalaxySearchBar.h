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

class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;

/**
 * Bottom bar of the canvas.
 *
 * The search field finds installed content: matching profiles, mods, packs and shaders are highlighted in the
 * galaxy, and Enter jumps from match to match. "Add content" opens the Discover window to browse Modrinth. The button on the right switches between galaxy and list view.
 */
class GalaxySearchBar : public QWidget {
    Q_OBJECT
   public:
    explicit GalaxySearchBar(QWidget* parent = nullptr);

    void setListView(bool list);
    void focusSearch();
    QString text() const;
    /** Shows how many installed items match the current search (-1 hides the count). */
    void setMatchCount(int matches);
    /** Adapts the placeholder to what is shown: all profiles, or one profile's content. */
    void setScope(bool insideSystem);
    /** Shows the red updates button next to the search field when count > 0. */
    void setUpdateCount(int count);

   protected:
    void resizeEvent(QResizeEvent* event) override;

   signals:
    void filterChanged(const QString& text);
    void nextMatchRequested();
    void addContentRequested();
    void viewToggleRequested();
    void updatesRequested();

   private:
    void updatePlaceholder();

    QLineEdit* m_edit = nullptr;
    QLabel* m_matches = nullptr;
    QPushButton* m_add = nullptr;
    QToolButton* m_viewToggle = nullptr;
    QToolButton* m_updates = nullptr;
    int m_updateCount = 0;
    bool m_list = false;
    bool m_compact = false;
    bool m_insideSystem = false;
};
