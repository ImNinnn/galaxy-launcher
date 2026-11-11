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

#include "OrbitTheme.h"

#include <QObject>

QString OrbitTheme::id()
{
    return "orbit";
}

QString OrbitTheme::name()
{
    return QObject::tr("Orbit");
}

QString OrbitTheme::tooltip()
{
    return QObject::tr("Dark grid with Orbit red, in pixel art");
}

QPalette OrbitTheme::colorScheme()
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0x14, 0x14, 0x16));
    palette.setColor(QPalette::WindowText, QColor(0xEC, 0xEC, 0xEF));
    palette.setColor(QPalette::Base, QColor(0x1C, 0x1C, 0x20));
    palette.setColor(QPalette::AlternateBase, QColor(0x23, 0x23, 0x28));
    palette.setColor(QPalette::ToolTipBase, QColor(0x1C, 0x1C, 0x20));
    palette.setColor(QPalette::ToolTipText, QColor(0xEC, 0xEC, 0xEF));
    palette.setColor(QPalette::Text, QColor(0xEC, 0xEC, 0xEF));
    palette.setColor(QPalette::Button, QColor(0x34, 0x34, 0x3C));
    palette.setColor(QPalette::ButtonText, QColor(0xEC, 0xEC, 0xEF));
    palette.setColor(QPalette::BrightText, QColor(0xFF, 0x6E, 0x6E));
    palette.setColor(QPalette::Link, QColor(0xFF, 0x26, 0x26));
    palette.setColor(QPalette::LinkVisited, QColor(0xB8, 0x1B, 0x1B));
    palette.setColor(QPalette::Highlight, QColor(0xC2, 0x1E, 0x1E));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor(0x6F, 0x6F, 0x7A));
    palette.setColor(QPalette::Light, QColor(0x45, 0x45, 0x4F));
    palette.setColor(QPalette::Midlight, QColor(0x2E, 0x2E, 0x35));
    palette.setColor(QPalette::Mid, QColor(0x23, 0x23, 0x28));
    palette.setColor(QPalette::Dark, QColor(0x0A, 0x0A, 0x0C));
    palette.setColor(QPalette::Shadow, Qt::black);
    return fadeInactive(palette, fadeAmount(), fadeColor());
}

double OrbitTheme::fadeAmount()
{
    return 0.5;
}

QColor OrbitTheme::fadeColor()
{
    return QColor(0x14, 0x14, 0x16);
}

bool OrbitTheme::hasStyleSheet()
{
    return true;
}

// Raised buttons follow Orbit's bevel: light top and left edges, a 2 px dark bottom edge (shades 1.35, 1.15, 0.62).
// Dialogs and settings pages: Orbit's colors and square shapes only, with the system font and no pixel art.
QString OrbitTheme::appStyleSheet()
{
    return QStringLiteral(R"(
QToolTip { color: #ECECEF; background-color: #1C1C20; border: 1px solid #2E2E35; border-left: 2px solid #FF2626; border-radius: 0; padding: 4px 6px; }
QMenu { background-color: #1C1C20; border: 1px solid #2E2E35; padding: 3px; }
QMenu::item { padding: 5px 22px 5px 22px; border-radius: 0; color: #ECECEF; border-left: 2px solid transparent; }
QMenu::item:selected { background-color: #2B2B31; border-left: 2px solid #FF2626; }
QMenu::item:disabled { color: #6F6F7A; }
QMenu::separator { height: 1px; background: #2E2E35; margin: 3px 6px; }
QMenu::icon { padding-left: 6px; }
QPushButton { background-color: #34343C; color: #ECECEF; border-style: solid; border-width: 1px 1px 2px 1px; border-color: #464651 #0A0A0C #202025 #3C3C45; border-radius: 2px; padding: 4px 12px; min-height: 18px; }
QPushButton:hover { background-color: #42424B; border-color: #595965 #0A0A0C #29292E #4C4C56; }
QPushButton:pressed, QPushButton:checked { background-color: #2B2B31; border-color: #202025 #0A0A0C #45454F #202025; }
QPushButton:focus { border-color: #FF2626 #FF2626 #B81B1B #FF2626; }
QPushButton:default { background-color: #C21E1E; color: #FFFFFF; border-color: #FF2828 #0A0A0C #781212 #DF2222; }
QPushButton:default:hover { background-color: #E02626; border-color: #FF3333 #0A0A0C #8B1818 #FF2C2C; }
QPushButton:default:pressed { background-color: #A81A1A; border-color: #781212 #0A0A0C #C21E1E #781212; }
QPushButton:disabled { background-color: #232328; color: #6F6F7A; border-color: #2E2E35 #1C1C20 #1C1C20 #2E2E35; }
QPushButton::menu-indicator { subcontrol-origin: padding; subcontrol-position: center right; right: 4px; }
QLineEdit { background-color: #232328; color: #ECECEF; border: 1px solid #2E2E35; border-radius: 2px; padding: 3px 5px; selection-background-color: #C21E1E; }
QLineEdit:focus { border-color: #45454F; }
QLineEdit:disabled { color: #6F6F7A; background-color: #1C1C20; }
QScrollBar:vertical { background: transparent; width: 8px; margin: 1px; }
QScrollBar::handle:vertical { background: #45454F; border-radius: 0; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: #6F6F7A; }
QScrollBar:horizontal { background: transparent; height: 8px; margin: 1px; }
QScrollBar::handle:horizontal { background: #45454F; border-radius: 0; min-width: 24px; }
QScrollBar::handle:horizontal:hover { background: #6F6F7A; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }
QCheckBox::indicator, QGroupBox::indicator { width: 14px; height: 14px; border: 1px solid #45454F; border-radius: 2px; background-color: #232328; }
QCheckBox::indicator:hover, QGroupBox::indicator:hover { border-color: #A3A3AD; }
QCheckBox::indicator:checked, QGroupBox::indicator:checked { background-color: #C21E1E; border-color: #FF2626; image: url(:/galaxy/check.svg); }
QCheckBox::indicator:indeterminate { background-color: #B81B1B; border-color: #FF2626; }
QCheckBox::indicator:disabled, QGroupBox::indicator:disabled { border-color: #2E2E35; background-color: #1C1C20; }
QCheckBox::indicator:checked:disabled { background-color: #45454F; }
QAbstractItemView::indicator { width: 13px; height: 13px; border: 1px solid #45454F; border-radius: 2px; background-color: #232328; }
QAbstractItemView::indicator:checked { background-color: #C21E1E; border-color: #FF2626; image: url(:/galaxy/check.svg); }
QAbstractItemView::indicator:disabled { border-color: #2E2E35; background-color: #1C1C20; }
QRadioButton::indicator { width: 14px; height: 14px; border: 1px solid #45454F; border-radius: 8px; background-color: #232328; }
QRadioButton::indicator:hover { border-color: #A3A3AD; }
QRadioButton::indicator:checked { background-color: #C21E1E; border-color: #FF2626; image: url(:/galaxy/dot.svg); }
QRadioButton::indicator:disabled { border-color: #2E2E35; background-color: #1C1C20; }
QProgressBar { background-color: #232328; color: #ECECEF; border: 1px solid #2E2E35; border-radius: 0; text-align: center; }
QProgressBar::chunk { background-color: #C21E1E; }
QStatusBar { background-color: #1C1C20; color: #A3A3AD; border-top: 1px solid #2E2E35; }
QStatusBar QLabel { color: #A3A3AD; }
)");
}

// The main window: Orbit's pixel art. Square panels with a bevel, ghost tool buttons, a red primary action, Silkscreen
// for the brand and headings. Icons are Orbit's pixel icons (set in code, see GalaxyLook).
QString OrbitTheme::chromeStyleSheet()
{
    return QStringLiteral(R"(
#galaxyTopBar { background-color: #1C1C20; border-top: 1px solid rgba(255, 255, 255, 31); border-bottom: 1px solid #2E2E35; }
#galaxyBrand { color: #ECECEF; font-family: "Silkscreen"; font-size: 16px; }
#galaxyLocation { color: #ECECEF; }
#galaxySidebar { background-color: #1C1C20; border-right: 1px solid #2E2E35; }
#galaxySidebarTitle { color: #6F6F7A; font-weight: bold; padding-left: 6px; }
QListView#galaxyProfileList { background: transparent; color: #ECECEF; outline: 0; }
#galaxySearchBar { background-color: #1C1C20; border-top: 1px solid #2E2E35; }
QLineEdit#galaxySearchEdit { background-color: #232328; color: #ECECEF; border: 1px solid #2E2E35; border-radius: 0; padding: 6px 10px; selection-background-color: #C21E1E; }
QLineEdit#galaxySearchEdit:hover { border-color: #45454F; }
QLineEdit#galaxySearchEdit:focus { border-color: #45454F; border-left: 2px solid #FF2626; }
QComboBox#galaxySearchCategory { background-color: #232328; color: #ECECEF; border: 1px solid #2E2E35; border-radius: 0; padding: 5px 26px 5px 10px; }
QComboBox#galaxySearchCategory:hover { border-color: #45454F; }
QComboBox#galaxySearchCategory::drop-down { subcontrol-origin: padding; subcontrol-position: center right; width: 22px; border: none; background: transparent; }
QComboBox#galaxySearchCategory::down-arrow { image: url(:/orbit/icons/chevron_down.png); width: 9px; height: 9px; }
QComboBox#galaxySearchCategory::down-arrow:on { top: 1px; }
QComboBox#galaxySearchCategory:focus { border-color: #45454F; }
QComboBox#galaxySearchCategory QAbstractItemView { background-color: #1C1C20; color: #ECECEF; selection-background-color: #2B2B31; border: 1px solid #2E2E35; }
QPushButton { background-color: #34343C; color: #ECECEF; border-style: solid; border-width: 1px 1px 2px 1px; border-color: #464651 #0A0A0C #202025 #3C3C45; border-radius: 0; padding: 6px 14px; qproperty-iconSize: 18px 18px; }
QPushButton:hover { background-color: #42424B; border-color: #E6E6EA #E6E6EA #E6E6EA #E6E6EA; }
QPushButton:pressed { background-color: #2B2B31; border-color: #202025 #0A0A0C #45454F #202025; padding-top: 7px; padding-bottom: 5px; }
QPushButton:focus { border-color: #FF2626; }
QPushButton:disabled { background-color: #232328; color: #6F6F7A; border-color: #2E2E35; }
QPushButton#galaxyPrimaryButton { background-color: #C21E1E; color: #FFFFFF; border-color: #FF2828 #0A0A0C #781212 #DF2222; font-family: "Silkscreen"; font-size: 16px; }
QPushButton#galaxyPrimaryButton:hover { background-color: #E02626; border-color: #E6E6EA; }
QPushButton#galaxyPrimaryButton:pressed { background-color: #A81A1A; border-color: #781212 #0A0A0C #C21E1E #781212; }
QPushButton#galaxyPrimaryButton:focus { border-color: #E6E6EA; }
QToolButton { color: #A3A3AD; border: 1px solid transparent; border-radius: 0; padding: 4px 7px; background: transparent; qproperty-iconSize: 18px 18px; }
QToolButton:hover { background-color: #232328; color: #ECECEF; border-color: #2E2E35; }
QToolButton:pressed, QToolButton:checked { background-color: #2B2B31; color: #ECECEF; border-color: #45454F; }
QToolButton:focus { border: 1px dashed #B0B0BA; }
QToolButton:disabled { color: #6F6F7A; }
QToolButton[popupMode="1"] { padding-right: 18px; }
QToolButton::menu-button { border: none; border-left: 1px solid #2E2E35; background: transparent; width: 16px; }
QToolButton::menu-button:hover { background-color: #2B2B31; }
QToolButton::menu-indicator { subcontrol-position: bottom right; subcontrol-origin: padding; left: -2px; top: -2px; }
QToolButton#galaxyLaunchButton { background-color: #C21E1E; color: #FFFFFF; font-family: "Silkscreen"; font-size: 16px; border-style: solid; border-width: 1px 1px 2px 1px; border-color: #FF2828 #0A0A0C #781212 #DF2222; padding: 3px 12px 2px 10px; }
QToolButton#galaxyLaunchButton:hover { background-color: #E02626; border-color: #E6E6EA; }
QToolButton#galaxyLaunchButton:pressed { background-color: #A81A1A; border-color: #781212 #0A0A0C #C21E1E #781212; padding-top: 4px; padding-bottom: 1px; }
QToolButton#galaxyLaunchButton:disabled { background-color: #232328; color: #6F6F7A; border-color: #2E2E35; }
QToolButton#galaxyPanelClose { background-color: #5A2328; border-style: solid; border-width: 1px 1px 2px 1px; border-color: #7A2F36 #0A0A0C #381619 #67282E; padding: 2px 7px; color: #FFB0B0; }
QToolButton#galaxyPanelClose:hover { background-color: #73292F; border-color: #E6E6EA; }
QToolButton#galaxyPanelClose:pressed { background-color: #4A1D21; }
QToolButton#galaxyKillButton { background-color: #5A2328; color: #FFB0B0; border-style: solid; border-width: 1px 1px 2px 1px; border-color: #7A2F36 #0A0A0C #381619 #67282E; padding: 4px 8px; }
QToolButton#galaxyKillButton:hover { background-color: #73292F; border-color: #E6E6EA; }
QToolButton#galaxyKillButton:pressed { background-color: #4A1D21; }
QToolButton#galaxyKillButton:disabled { background-color: #232328; border-color: #2E2E35; }
QToolButton#galaxyWindowButton, QToolButton#galaxyCloseButton { padding: 4px 8px; border: 1px solid transparent; }
QToolButton#galaxyWindowButton:hover { background-color: #2B2B31; border-color: transparent; }
QToolButton#galaxyCloseButton:hover { background-color: #C21E1E; border-color: transparent; }
QToolButton#galaxyCloseButton:pressed { background-color: #A81A1A; }
QToolButton#galaxyMoreButton::menu-indicator { image: none; width: 0; }
QToolButton#galaxyViewToggle { background-color: #34343C; color: #ECECEF; border-style: solid; border-width: 1px 1px 2px 1px; border-color: #464651 #0A0A0C #202025 #3C3C45; padding: 4px 10px; }
QToolButton#galaxyViewToggle:hover { background-color: #42424B; border-color: #E6E6EA; }
QToolButton#galaxyUpdatesButton { color: #FF6E6E; font-weight: 600; }
#galaxyAccountButton { qproperty-iconSize: 22px 22px; }
#galaxyAccountButton, #galaxySettingsButton { background-color: #232328; color: #ECECEF; border: 1px solid #2E2E35; border-radius: 0; padding: 7px 10px; text-align: left; }
#galaxyAccountButton:hover, #galaxySettingsButton:hover { background-color: #2B2B31; border-color: #45454F; }
#galaxyEmptyState { background-color: #1C1C20; border: 1px solid #2E2E35; border-left: 2px solid #FF2626; border-radius: 0; }
#galaxyEmptyTitle { color: #ECECEF; font-family: "Silkscreen"; font-size: 24px; }
#galaxyEmptyText { color: #A3A3AD; }
#galaxyHint { color: #A3A3AD; }
)");
}
