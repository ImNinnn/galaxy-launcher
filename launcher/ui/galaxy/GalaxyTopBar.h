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

class QAction;
class QHBoxLayout;
class QLabel;
class QToolButton;

/**
 * Compact top navigation: branding, back navigation, where the user is, and actions for the selected profile.
 * Buttons are bound to MainWindow's existing QActions, so enablement and behavior match Prism's menus exactly.
 */
class GalaxyTopBar : public QWidget {
    Q_OBJECT
   public:
    explicit GalaxyTopBar(QWidget* parent = nullptr);

    /** Shows "Galaxy", "Galaxy › Profile" or "List › Profile". */
    void setLocation(const QString& area, const QString& profile, bool canGoBack);

    /** Actions shown only while a profile is selected. */
    QToolButton* addContextAction(QAction* action, bool textBesideIcon = true);
    /** Always visible actions on the right. */
    QToolButton* addGlobalAction(QAction* action, bool textBesideIcon = false);

    void setContextVisible(bool visible);

    /** Shows minimize / maximize / close buttons, for a window without a system title bar. */
    void setWindowControlsVisible(bool visible);
    /** Updates the maximize button after the window state changed. */
    void updateWindowState();

   signals:
    void backRequested();

   protected:
    void resizeEvent(QResizeEvent* event) override;
    // Dragging empty space moves the window; a double click maximizes it (when the window has no title bar).
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

   private:
    QToolButton* m_back = nullptr;
    QLabel* m_location = nullptr;
    QHBoxLayout* m_contextLayout = nullptr;
    QWidget* m_context = nullptr;
    QHBoxLayout* m_globalLayout = nullptr;
    QLabel* m_brand = nullptr;
    QList<QToolButton*> m_textButtons;  ///< Buttons that show text when there is room.
    QWidget* m_windowControls = nullptr;
    QToolButton* m_maximize = nullptr;
};
