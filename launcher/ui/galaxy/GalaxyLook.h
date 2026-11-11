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

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QObject>
#include <QString>

#include <functional>

/**
 * Which look the main window has: Galaxy (the default, smooth deep space in purple) or Orbit (grey, black and red
 * pixel art, the Orbit Cosmetics identity). Follows the application theme: selecting the "orbit" theme switches it.
 *
 * Only the frontend changes: names shown to the user say "Orbit Launcher", but the application, its data, its IDs and
 * its packages stay Galaxy Launcher.
 */
namespace GalaxyLook {

/** Emits changed() whenever the look switches (after the new application theme was applied). */
class Notifier : public QObject {
    Q_OBJECT
   signals:
    void changed();
};

Notifier* notifier();

/** Called by the theme manager with the id of the theme it applied. */
void setTheme(const QString& themeId);

/** True while the Orbit theme is selected. */
bool orbit();

/** "Orbit Launcher" with the Orbit theme, the launcher's own display name otherwise. For text shown to the user only. */
QString launcherName();

/** The launcher's logo for the current look. */
QIcon logo();

/** Orbit's colors (ORBIT_THEME.md). */
namespace Orbit {
inline const QColor Background{ 0x14, 0x14, 0x16 };
inline const QColor Panel{ 0x1C, 0x1C, 0x20 };
inline const QColor Panel2{ 0x23, 0x23, 0x28 };
inline const QColor PanelHover{ 0x2B, 0x2B, 0x31 };
inline const QColor Border{ 0x2E, 0x2E, 0x35 };
inline const QColor BorderLight{ 0x45, 0x45, 0x4F };
inline const QColor Text{ 0xEC, 0xEC, 0xEF };
inline const QColor Muted{ 0xA3, 0xA3, 0xAD };
inline const QColor Faint{ 0x6F, 0x6F, 0x7A };
inline const QColor Red{ 0xFF, 0x26, 0x26 };
inline const QColor RedDark{ 0xB8, 0x1B, 0x1B };
inline const QColor Primary{ 0xC2, 0x1E, 0x1E };
inline const QColor PrimaryHover{ 0xE0, 0x26, 0x26 };
inline const QColor Secondary{ 0x34, 0x34, 0x3C };
inline const QColor Green{ 0x7C, 0xE3, 0x8B };
inline const QColor Yellow{ 0xFF, 0xCC, 0x4D };
inline const QColor ErrorText{ 0xFF, 0x6E, 0x6E };
inline const QColor Blue{ 0x4D, 0xA6, 0xFF };
inline const QColor Edge{ 0x0A, 0x0A, 0x0C };
inline const QColor EdgeHot{ 0xE6, 0xE6, 0xEA };
inline const QColor Dashed{ 0xB0, 0xB0, 0xBA };
}  // namespace Orbit

/** Colors the galaxy view and its widgets share, for the current look. */
QColor accent();      ///< Selection and focus outlines.
QColor textColor();   ///< Main text.
QColor mutedText();   ///< Secondary text.
QColor orbitPath();   ///< Orbit lines.
QColor ringColor();   ///< Shader rings.
QColor panelColor();  ///< Label and tooltip backgrounds (opaque; callers set the alpha).

/** Silkscreen, Orbit's pixel font, for headings and the logo. @param pixelSize a multiple of 8 stays crisp. */
QFont pixelFont(int pixelSize = 16);

/**
 * One of Orbit's 9x9 pixel icons (Orbit Cosmetics' icon set, plus a few drawn the same way: "minus", "stop", "menu",
 * "maximize", "restore", "fit", "help", "list", "tag"). Scaled up by whole pixels, never smoothed. Tinted muted
 * normally, white when active or selected and faint when disabled; @param color fixes the tint for every state.
 */
QIcon pixelIcon(const QString& name, const QColor& color = QColor());

/** pixelIcon() with the Orbit look, @param galaxy otherwise. */
QIcon icon(const QIcon& galaxy, const QString& orbitName);

/**
 * Keeps an action's or a button's icon in step with the look: its current icon is the Galaxy one, @p orbitName the
 * Orbit one. Works for QAction and QAbstractButton.
 */
void bindIcon(QObject* target, const QString& orbitName, const QColor& color = QColor());

/** Runs @p apply now and whenever the look changes, for as long as @p context lives. */
void onChange(QObject* context, const std::function<void()>& apply);

}  // namespace GalaxyLook
