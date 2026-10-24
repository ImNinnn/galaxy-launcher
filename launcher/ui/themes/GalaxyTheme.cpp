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

#include "GalaxyTheme.h"

#include <QObject>

QString GalaxyTheme::id()
{
    return "galaxy";
}

QString GalaxyTheme::name()
{
    return QObject::tr("Galaxy");
}

QString GalaxyTheme::tooltip()
{
    return QObject::tr("Deep space with a space purple accent");
}

QPalette GalaxyTheme::colorScheme()
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(20, 16, 42));
    palette.setColor(QPalette::WindowText, QColor(236, 233, 255));
    palette.setColor(QPalette::Base, QColor(15, 12, 34));
    palette.setColor(QPalette::AlternateBase, QColor(24, 19, 51));
    palette.setColor(QPalette::ToolTipBase, QColor(26, 20, 56));
    palette.setColor(QPalette::ToolTipText, QColor(236, 233, 255));
    palette.setColor(QPalette::Text, QColor(236, 233, 255));
    palette.setColor(QPalette::Button, QColor(34, 26, 68));
    palette.setColor(QPalette::ButtonText, QColor(236, 233, 255));
    palette.setColor(QPalette::BrightText, QColor(255, 120, 160));
    palette.setColor(QPalette::Link, QColor(182, 156, 255));
    palette.setColor(QPalette::LinkVisited, QColor(160, 130, 240));
    palette.setColor(QPalette::Highlight, QColor(124, 77, 255));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor(138, 132, 174));
    palette.setColor(QPalette::Light, QColor(60, 48, 110));
    palette.setColor(QPalette::Midlight, QColor(46, 37, 87));
    palette.setColor(QPalette::Mid, QColor(36, 29, 70));
    palette.setColor(QPalette::Dark, QColor(12, 9, 26));
    palette.setColor(QPalette::Shadow, QColor(4, 3, 10));
    return fadeInactive(palette, fadeAmount(), fadeColor());
}

double GalaxyTheme::fadeAmount()
{
    return 0.5;
}

QColor GalaxyTheme::fadeColor()
{
    return QColor(20, 16, 42);
}

bool GalaxyTheme::hasStyleSheet()
{
    return true;
}

QString GalaxyTheme::appStyleSheet()
{
    return QStringLiteral(R"(
QToolTip { color: #ECE9FF; background-color: #1A1438; border: 1px solid #7C5CFF; border-radius: 6px; padding: 4px 6px; }
QMenu { background-color: #17122F; border: 1px solid #3A2E6B; padding: 4px; }
QMenu::item { padding: 6px 24px 6px 24px; border-radius: 6px; color: #ECE9FF; }
QMenu::item:selected { background-color: #4C2FB0; }
QMenu::item:disabled { color: #8A84AE; }
QMenu::separator { height: 1px; background: #2E2557; margin: 4px 8px; }
QMenu::icon { padding-left: 6px; }
QPushButton#galaxyPrimaryButton { background-color: #7C4DFF; color: white; border: none; border-radius: 15px; padding: 6px 16px; font-weight: 600; }
QPushButton#galaxyPrimaryButton:hover { background-color: #8F66FF; }
QPushButton#galaxyPrimaryButton:pressed { background-color: #6A3DF0; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #3A2E6B; border-radius: 4px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: #5A47A0; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: #3A2E6B; border-radius: 4px; min-width: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }
QCheckBox::indicator, QGroupBox::indicator { width: 15px; height: 15px; border: 1px solid #8070C0; border-radius: 4px; background-color: #1B1540; }
QCheckBox::indicator:hover, QGroupBox::indicator:hover { border-color: #C4B0FF; }
QCheckBox::indicator:checked, QGroupBox::indicator:checked { background-color: #7C4DFF; border-color: #C4B0FF; image: url(:/galaxy/check.svg); }
QCheckBox::indicator:indeterminate { background-color: #4C2FB0; border-color: #C4B0FF; }
QCheckBox::indicator:disabled, QGroupBox::indicator:disabled { border-color: #3A3260; background-color: #15112C; }
QCheckBox::indicator:checked:disabled { background-color: #3A2E6B; }
QAbstractItemView::indicator { width: 14px; height: 14px; border: 1px solid #8070C0; border-radius: 4px; background-color: #1B1540; }
QAbstractItemView::indicator:checked { background-color: #7C4DFF; border-color: #C4B0FF; image: url(:/galaxy/check.svg); }
QAbstractItemView::indicator:disabled { border-color: #3A3260; background-color: #15112C; }
QRadioButton::indicator { width: 15px; height: 15px; border: 1px solid #8070C0; border-radius: 8px; background-color: #1B1540; }
QRadioButton::indicator:hover { border-color: #C4B0FF; }
QRadioButton::indicator:checked { background-color: #7C4DFF; border-color: #C4B0FF; image: url(:/galaxy/dot.svg); }
QRadioButton::indicator:disabled { border-color: #3A3260; background-color: #15112C; }
QStatusBar { background-color: #0D0A20; color: #A39DC8; border-top: 1px solid #2C2358; }
QStatusBar QLabel { color: #A39DC8; }
)");
}

QString GalaxyTheme::chromeStyleSheet()
{
    return QStringLiteral(R"(
#galaxyTopBar { background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #120D2B, stop:1 #1A1240); border-bottom: 1px solid #2C2358; }
#galaxyBrand { color: #F3EEFF; font-weight: 600; font-size: 11pt; }
#galaxyLocation { color: #ECE9FF; }
#galaxySidebar { background-color: #100C25; border-right: 1px solid #2C2358; }
#galaxySidebarTitle { color: #B8B2D9; font-weight: 600; padding-left: 6px; }
QListView#galaxyProfileList { background: transparent; color: #ECE9FF; outline: 0; }
#galaxySearchBar { background-color: #120D2B; border-top: 1px solid #2C2358; }
QLineEdit#galaxySearchEdit { background-color: #1B1540; color: #ECE9FF; border: 1px solid #3A2E6B; border-radius: 16px; padding: 6px 12px; selection-background-color: #7C4DFF; }
QLineEdit#galaxySearchEdit:focus { border: 1px solid #A78BFA; }
QComboBox#galaxySearchCategory { background-color: #1B1540; color: #ECE9FF; border: 1px solid #3A2E6B; border-radius: 14px; padding: 5px 28px 5px 12px; }
QComboBox#galaxySearchCategory::drop-down { subcontrol-origin: padding; subcontrol-position: center right; width: 24px; border: none; background: transparent; }
QComboBox#galaxySearchCategory::down-arrow { image: url(:/galaxy/chevron-down.svg); width: 12px; height: 12px; }
QComboBox#galaxySearchCategory::down-arrow:on { top: 1px; }
QComboBox#galaxySearchCategory:focus { border: 1px solid #A78BFA; }
QComboBox#galaxySearchCategory QAbstractItemView { background-color: #17122F; color: #ECE9FF; selection-background-color: #4C2FB0; border: 1px solid #3A2E6B; }
QPushButton#galaxyPrimaryButton { background-color: #7C4DFF; color: white; border: none; border-radius: 15px; padding: 6px 16px; font-weight: 600; }
QPushButton#galaxyPrimaryButton:hover { background-color: #8F66FF; }
QPushButton#galaxyPrimaryButton:pressed { background-color: #6A3DF0; }
QPushButton#galaxyPrimaryButton:focus { border: 2px solid #E9DDFF; }
QPushButton { background-color: #221A44; color: #ECE9FF; border: 1px solid #3A2E6B; border-radius: 15px; padding: 6px 16px; }
QPushButton:hover { background-color: #2C2258; }
QPushButton:focus { border: 1px solid #A78BFA; }
QToolButton { color: #ECE9FF; border: 1px solid transparent; border-radius: 8px; padding: 5px 8px; background: transparent; }
QToolButton:hover { background-color: rgba(167, 139, 250, 40); }
QToolButton:pressed, QToolButton:checked { background-color: rgba(167, 139, 250, 72); }
QToolButton:focus { border: 1px solid #A78BFA; }
QToolButton:disabled { color: #6F6894; }
QToolButton[popupMode="1"] { padding-right: 18px; }
QToolButton::menu-button { border: none; border-left: 1px solid rgba(167, 139, 250, 60); background: transparent; width: 16px; border-top-right-radius: 8px; border-bottom-right-radius: 8px; }
QToolButton::menu-button:hover { background-color: rgba(167, 139, 250, 50); }
QToolButton::menu-indicator { subcontrol-position: bottom right; subcontrol-origin: padding; left: -2px; top: -2px; }
QToolButton#galaxyLaunchButton { background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #8B5CF6, stop:1 #6D28D9); color: #FFFFFF; font-weight: 600; border: 1px solid #A78BFA; padding: 5px 12px; }
QToolButton#galaxyLaunchButton:hover { background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #A07BFF, stop:1 #7C3AED); }
QToolButton#galaxyLaunchButton:pressed { background-color: #5B21B6; }
QToolButton#galaxyLaunchButton:disabled { background-color: #2A2350; color: #8A84AE; border-color: #3A2E6B; }
QToolButton#galaxyPanelClose { background-color: rgba(248, 113, 113, 34); border: 1px solid rgba(248, 113, 113, 85); border-radius: 6px; padding: 2px 7px; color: #F3D2D2; }
QToolButton#galaxyPanelClose:hover { background-color: rgba(248, 113, 113, 70); border-color: rgba(252, 165, 165, 150); }
QToolButton#galaxyPanelClose:pressed { background-color: rgba(220, 38, 38, 110); }
QToolButton#galaxyKillButton { background-color: rgba(248, 113, 113, 34); border: 1px solid rgba(248, 113, 113, 85); padding: 5px 8px; }
QToolButton#galaxyKillButton:hover { background-color: rgba(248, 113, 113, 70); border-color: rgba(252, 165, 165, 150); }
QToolButton#galaxyKillButton:pressed { background-color: rgba(220, 38, 38, 110); }
QToolButton#galaxyKillButton:disabled { background-color: rgba(255, 255, 255, 8); border-color: #2C2358; }
QToolButton#galaxyWindowButton, QToolButton#galaxyCloseButton { padding: 4px 8px; border-radius: 6px; }
QToolButton#galaxyCloseButton:hover { background-color: #C42B1C; }
QToolButton#galaxyCloseButton:pressed { background-color: #A1251A; }
QToolButton#galaxyMoreButton::menu-indicator { image: none; width: 0; }
QToolButton#galaxyViewToggle { background-color: #1B1540; border: 1px solid #3A2E6B; border-radius: 15px; padding: 5px 12px; }
QToolButton#galaxyViewToggle:hover { background-color: #2C2258; }
#galaxyAccountButton, #galaxySettingsButton { background-color: rgba(255, 255, 255, 10); border-radius: 10px; padding: 7px 10px; text-align: left; }
#galaxyAccountButton:hover, #galaxySettingsButton:hover { background-color: rgba(167, 139, 250, 40); }
#galaxyEmptyState { background-color: rgba(16, 12, 37, 225); border: 1px solid #3A2E6B; border-radius: 18px; }
#galaxyEmptyTitle { color: #FFFFFF; font-size: 16pt; font-weight: 600; }
#galaxyEmptyText { color: #C9C3EA; }
#galaxyHint { color: #A39DC8; }
)");
}
