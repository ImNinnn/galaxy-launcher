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

#include "ui/pages/BasePage.h"

class QCheckBox;
class QComboBox;
class QSlider;
class QLabel;

/** Settings > Galaxy: motion, quality and layout options for the galaxy view. */
class GalaxyPage : public QWidget, public BasePage {
    Q_OBJECT
   public:
    explicit GalaxyPage(QWidget* parent = nullptr);

    QString displayName() const override { return tr("Galaxy"); }
    QIcon icon() const override { return QIcon::fromTheme("appearance"); }
    QString id() const override { return "galaxy-settings"; }
    QString helpPage() const override { return "Launcher-settings"; }
    bool apply() override;
    void retranslate() override {}

   private:
    void load();
    QSlider* addSlider(class QFormLayout* form, const QString& label, int min, int max, QLabel** valueLabel);

    QCheckBox* m_launchAnimation = nullptr;
    QCheckBox* m_reducedMotion = nullptr;
    QCheckBox* m_orbitAnimation = nullptr;
    QCheckBox* m_backgroundMotion = nullptr;
    QCheckBox* m_showOrbits = nullptr;
    QCheckBox* m_updateChecks = nullptr;
    QCheckBox* m_systemTitleBar = nullptr;
    QCheckBox* m_discordPresence = nullptr;
    QCheckBox* m_launcherUpdates = nullptr;
    QComboBox* m_renderQuality = nullptr;
    QComboBox* m_starfield = nullptr;
    QComboBox* m_defaultView = nullptr;
    QComboBox* m_blackHoleProfile = nullptr;
    QSlider* m_objectScale = nullptr;
    QSlider* m_spacing = nullptr;
    QSlider* m_zoomSpeed = nullptr;
    QLabel* m_objectScaleValue = nullptr;
    QLabel* m_spacingValue = nullptr;
    QLabel* m_zoomSpeedValue = nullptr;
};
