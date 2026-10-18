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

#include "GalaxyPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>

#include "Application.h"
#include "InstanceList.h"
#include "settings/SettingsObject.h"
#include "ui/galaxy/GalaxySettings.h"

GalaxyPage::GalaxyPage(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scroll);
    auto* layout = new QVBoxLayout(content);
    scroll->setWidget(content);
    outer->addWidget(scroll);

    auto* motion = new QGroupBox(tr("Motion"), content);
    auto* motionLayout = new QVBoxLayout(motion);
    m_launchAnimation = new QCheckBox(tr("Play the launch animation when starting a profile"), motion);
    m_launchAnimation->setToolTip(tr("The solar system collapses into its sun. Purely cosmetic: it never delays the game or touches files."));
    m_reducedMotion = new QCheckBox(tr("Reduce motion"), motion);
    m_reducedMotion->setToolTip(tr("Stops orbiting and pulsing, and replaces zooms and the launch sequence with short fades."));
    m_orbitAnimation = new QCheckBox(tr("Animate orbits"), motion);
    m_orbitAnimation->setToolTip(tr("Planets and moons move along their orbits while the launcher window is active."));
    motionLayout->addWidget(m_launchAnimation);
    motionLayout->addWidget(m_reducedMotion);
    motionLayout->addWidget(m_orbitAnimation);
    m_backgroundMotion = new QCheckBox(tr("Animate the background"), motion);
    m_backgroundMotion->setToolTip(tr("The star field drifts slowly and moves in depth as you pan, zoom and travel between systems."));
    motionLayout->addWidget(m_backgroundMotion);
    layout->addWidget(motion);

    auto* look = new QGroupBox(tr("Appearance and performance"), content);
    auto* lookForm = new QFormLayout(look);
    m_renderQuality = new QComboBox(look);
    m_renderQuality->addItem(tr("Simple (flat shapes, fastest)"), 0);
    m_renderQuality->addItem(tr("Shaded"), 1);
    m_renderQuality->addItem(tr("Detailed (surfaces and shadows)"), 2);
    lookForm->addRow(tr("Celestial quality:"), m_renderQuality);
    m_starfield = new QComboBox(look);
    m_starfield->addItem(tr("Off (plain background)"), 0);
    m_starfield->addItem(tr("Sparse"), 1);
    m_starfield->addItem(tr("Full (nebulae and distant galaxies)"), 2);
    lookForm->addRow(tr("Star field:"), m_starfield);
    m_showOrbits = new QCheckBox(tr("Show orbit paths"), look);
    lookForm->addRow(QString(), m_showOrbits);
    m_objectScale = addSlider(lookForm, tr("Celestial object size:"), 50, 200, &m_objectScaleValue);
    m_objectScale->setToolTip(tr("Scales planets, moons and rings. Relative sizes still follow the installed file sizes."));
    layout->addWidget(look);

    auto* navigation = new QGroupBox(tr("Layout and navigation"), content);
    auto* navForm = new QFormLayout(navigation);
    m_defaultView = new QComboBox(navigation);
    m_defaultView->addItem(tr("Remember the last view"), "last");
    m_defaultView->addItem(tr("Galaxy"), "galaxy");
    m_defaultView->addItem(tr("List"), "list");
    navForm->addRow(tr("View on startup:"), m_defaultView);
    m_blackHoleProfile = new QComboBox(navigation);
    m_blackHoleProfile->addItem(tr("The last played profile"), QString());
    auto* instances = APPLICATION->instances();
    for (int i = 0; i < instances->count(); i++) {
        m_blackHoleProfile->addItem(instances->at(i)->name(), instances->at(i)->id());
    }
    navForm->addRow(tr("Clicking the black hole launches:"), m_blackHoleProfile);
    m_spacing = addSlider(navForm, tr("Space between solar systems:"), 50, 300, &m_spacingValue);
    m_zoomSpeed = addSlider(navForm, tr("Mouse wheel zoom speed:"), 25, 400, &m_zoomSpeedValue);
    layout->addWidget(navigation);

    auto* other = new QGroupBox(tr("Window and updates"), content);
    auto* otherLayout = new QVBoxLayout(other);
    m_systemTitleBar = new QCheckBox(tr("Use the system title bar instead of the launcher's own window buttons"), other);
    m_updateChecks = new QCheckBox(tr("Check Modrinth for updates of installed content when a profile is opened"), other);
    m_updateChecks->setToolTip(tr("Content with an update gets a red outline and \"!\" in the galaxy, and a red name in the list view."));
    otherLayout->addWidget(m_systemTitleBar);
    otherLayout->addWidget(m_updateChecks);
    m_discordPresence = new QCheckBox(tr("Show what you play on Discord (Rich Presence)"), other);
    m_discordPresence->setToolTip(tr("Shows \"In the launcher\" or \"Playing <profile>\" with its loader and Minecraft version, and your "
                                     "account's head, when the Discord app is running."));
    otherLayout->addWidget(m_discordPresence);
    m_launcherUpdates = new QCheckBox(tr("Check GitHub for new versions of Galaxy Launcher at startup"), other);
    otherLayout->addWidget(m_launcherUpdates);
    layout->addWidget(other);

    auto* note = new QLabel(tr("Profile sun colors are set per profile: right-click a sun or a profile in the sidebar."), content);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();

    load();
}

QSlider* GalaxyPage::addSlider(QFormLayout* form, const QString& label, int min, int max, QLabel** valueLabel)
{
    auto* row = new QWidget(this);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    auto* slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(min, max);
    slider->setSingleStep(5);
    slider->setPageStep(25);
    auto* value = new QLabel(row);
    value->setMinimumWidth(48);
    connect(slider, &QSlider::valueChanged, value, [value](int v) { value->setText(QStringLiteral("%1%").arg(v)); });
    rowLayout->addWidget(slider, 1);
    rowLayout->addWidget(value);
    slider->setAccessibleName(label);
    form->addRow(label, row);
    *valueLabel = value;
    return slider;
}

void GalaxyPage::load()
{
    auto* s = APPLICATION->settings();
    m_launchAnimation->setChecked(s->get(GalaxySettings::LaunchAnimation).toBool());
    m_reducedMotion->setChecked(s->get(GalaxySettings::ReducedMotion).toBool());
    m_orbitAnimation->setChecked(s->get(GalaxySettings::OrbitAnimation).toBool());
    m_backgroundMotion->setChecked(s->get(GalaxySettings::BackgroundMotion).toBool());
    m_showOrbits->setChecked(s->get(GalaxySettings::ShowOrbits).toBool());
    m_updateChecks->setChecked(s->get(GalaxySettings::UpdateChecks).toBool());
    m_systemTitleBar->setChecked(s->get(GalaxySettings::SystemTitleBar).toBool());
    m_discordPresence->setChecked(s->get(GalaxySettings::DiscordPresence).toBool());
    m_launcherUpdates->setChecked(s->get(GalaxySettings::LauncherUpdates).toBool());
    m_renderQuality->setCurrentIndex(m_renderQuality->findData(s->get(GalaxySettings::RenderQuality).toInt()));
    m_starfield->setCurrentIndex(m_starfield->findData(s->get(GalaxySettings::StarfieldQuality).toInt()));
    m_defaultView->setCurrentIndex(std::max(0, m_defaultView->findData(s->get(GalaxySettings::DefaultView).toString())));
    m_blackHoleProfile->setCurrentIndex(std::max(0, m_blackHoleProfile->findData(s->get(GalaxySettings::BlackHoleProfile).toString())));
    m_objectScale->setValue(s->get(GalaxySettings::ObjectScale).toInt());
    m_spacing->setValue(s->get(GalaxySettings::Spacing).toInt());
    m_zoomSpeed->setValue(s->get(GalaxySettings::ZoomSpeed).toInt());
    m_objectScaleValue->setText(QStringLiteral("%1%").arg(m_objectScale->value()));
    m_spacingValue->setText(QStringLiteral("%1%").arg(m_spacing->value()));
    m_zoomSpeedValue->setText(QStringLiteral("%1%").arg(m_zoomSpeed->value()));
}

bool GalaxyPage::apply()
{
    auto* s = APPLICATION->settings();
    s->set(GalaxySettings::LaunchAnimation, m_launchAnimation->isChecked());
    s->set(GalaxySettings::ReducedMotion, m_reducedMotion->isChecked());
    s->set(GalaxySettings::OrbitAnimation, m_orbitAnimation->isChecked());
    s->set(GalaxySettings::BackgroundMotion, m_backgroundMotion->isChecked());
    s->set(GalaxySettings::ShowOrbits, m_showOrbits->isChecked());
    s->set(GalaxySettings::UpdateChecks, m_updateChecks->isChecked());
    s->set(GalaxySettings::SystemTitleBar, m_systemTitleBar->isChecked());
    s->set(GalaxySettings::DiscordPresence, m_discordPresence->isChecked());
    s->set(GalaxySettings::LauncherUpdates, m_launcherUpdates->isChecked());
    s->set(GalaxySettings::RenderQuality, m_renderQuality->currentData().toInt());
    s->set(GalaxySettings::StarfieldQuality, m_starfield->currentData().toInt());
    s->set(GalaxySettings::DefaultView, m_defaultView->currentData().toString());
    s->set(GalaxySettings::BlackHoleProfile, m_blackHoleProfile->currentData().toString());
    s->set(GalaxySettings::ObjectScale, m_objectScale->value());
    s->set(GalaxySettings::Spacing, m_spacing->value());
    s->set(GalaxySettings::ZoomSpeed, m_zoomSpeed->value());
    return true;
}
