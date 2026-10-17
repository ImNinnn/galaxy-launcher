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

#include "GalaxySearchBar.h"

#include "UpdatesDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QToolButton>

GalaxySearchBar::GalaxySearchBar(QWidget* parent) : QWidget(parent)
{
    setObjectName("galaxySearchBar");
    setAttribute(Qt::WA_StyledBackground);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    m_edit = new QLineEdit(this);
    m_edit->setObjectName("galaxySearchEdit");
    m_edit->setClearButtonEnabled(true);
    m_edit->setAccessibleName(tr("Search installed content"));
    m_edit->setToolTip(tr("Highlights matching installed content. Enter jumps to the next match."));

    m_matches = new QLabel(this);
    m_matches->setObjectName("galaxyHint");
    m_matches->setMinimumWidth(80);

    m_add = new QPushButton(tr("Add content"), this);
    m_add->setObjectName("galaxyPrimaryButton");
    m_add->setIcon(QIcon::fromTheme("new"));
    m_add->setToolTip(tr("Browse Modrinth and install mods, resource packs, shaders and data packs"));
    m_add->setAccessibleName(tr("Add content"));

    m_updates = new QToolButton(this);
    m_updates->setObjectName("galaxyUpdatesButton");
    m_updates->setIcon(UpdatesDialog::icon());
    m_updates->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_updates->setStyleSheet(QStringLiteral("QToolButton#galaxyUpdatesButton { color: #F85149; font-weight: 600; }"));
    m_updates->hide();

    m_viewToggle = new QToolButton(this);
    m_viewToggle->setObjectName("galaxyViewToggle");
    m_viewToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    layout->addWidget(m_edit, 1);
    layout->addWidget(m_matches);
    layout->addWidget(m_updates);
    layout->addStretch();
    layout->addWidget(m_add);
    layout->addSpacing(8);
    layout->addWidget(m_viewToggle);

    connect(m_edit, &QLineEdit::textChanged, this, &GalaxySearchBar::filterChanged);
    connect(m_edit, &QLineEdit::returnPressed, this, &GalaxySearchBar::nextMatchRequested);
    connect(m_add, &QPushButton::clicked, this, &GalaxySearchBar::addContentRequested);
    connect(m_viewToggle, &QToolButton::clicked, this, &GalaxySearchBar::viewToggleRequested);
    connect(m_updates, &QToolButton::clicked, this, &GalaxySearchBar::updatesRequested);

    setMatchCount(-1);
    updatePlaceholder();
    setListView(false);
}

void GalaxySearchBar::setMatchCount(int matches)
{
    if (matches < 0 || m_edit->text().trimmed().isEmpty()) {
        m_matches->clear();
        return;
    }
    m_matches->setText(matches == 0 ? tr("No matches") : tr("%n match(es)", "", matches));
}

void GalaxySearchBar::setUpdateCount(int count)
{
    m_updateCount = count;
    m_updates->setVisible(count > 0);
    const QString label = count == 1 ? tr("1 update") : tr("%1 updates").arg(count);
    m_updates->setText(m_compact ? QString() : label);
    m_updates->setToolTip(tr("%1 available in this profile: review and install them").arg(label));
    m_updates->setAccessibleName(tr("%1 available").arg(label));
}

void GalaxySearchBar::setScope(bool insideSystem)
{
    m_insideSystem = insideSystem;
    updatePlaceholder();
}

void GalaxySearchBar::updatePlaceholder()
{
    if (m_compact) {
        m_edit->setPlaceholderText(tr("Search installed content…"));
    } else if (m_insideSystem) {
        m_edit->setPlaceholderText(tr("Search this profile's mods, packs and shaders…"));
    } else {
        m_edit->setPlaceholderText(tr("Search profiles and installed mods, packs and shaders…"));
    }
}

void GalaxySearchBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    const bool compact = event->size().width() < 820;
    if (compact == m_compact) {
        return;
    }
    m_compact = compact;
    // Narrow windows keep the search field usable; the buttons keep their tooltips and accessible names.
    m_add->setText(compact ? QString() : tr("Add content"));
    setUpdateCount(m_updateCount);
    updatePlaceholder();
    setListView(m_list);
}

void GalaxySearchBar::setListView(bool list)
{
    m_list = list;
    // Label the destination, with text, so the button does not rely on its icon.
    m_viewToggle->setToolButtonStyle(m_compact ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
    m_viewToggle->setAccessibleName(list ? tr("Galaxy view") : tr("List view"));
    m_viewToggle->setText(list ? tr("Galaxy view") : tr("List view"));
    m_viewToggle->setIcon(QIcon::fromTheme(list ? "viewfolder" : "notes"));
    m_viewToggle->setToolTip(list ? tr("Switch to the interactive galaxy") : tr("Switch to a conventional list of profiles and content"));
    // The list view has its own per-page filters.
    m_edit->setVisible(!list);
    m_matches->setVisible(!list);
}

void GalaxySearchBar::focusSearch()
{
    m_edit->setFocus(Qt::ShortcutFocusReason);
    m_edit->selectAll();
}

QString GalaxySearchBar::text() const
{
    return m_edit->text();
}
