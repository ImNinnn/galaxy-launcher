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

#include "UpdatesDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSplitter>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <numeric>

#include "Markdown.h"
#include "StringUtils.h"
#include "SystemContent.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/mod/ResourceFolderModel.h"

namespace {
enum Column { NameColumn, TypeColumn, InstalledColumn, NewColumn };
constexpr int IndexRole = Qt::UserRole + 1;
}  // namespace

UpdatesDialog::UpdatesDialog(QWidget* parent, const QString& profileName, const QList<UpdateChecker::PendingUpdate>& updates)
    : QDialog(parent), m_updates(updates)
{
    setWindowTitle(tr("Updates for %1").arg(profileName));
    resize(860, 560);

    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(updates.size() == 1 ? tr("1 update available on Modrinth. Choose what to update:")
                                                 : tr("%1 updates available on Modrinth. Choose what to update:").arg(updates.size()),
                             this);
    layout->addWidget(intro);

    const auto lockedCount = std::count_if(updates.begin(), updates.end(), [](const auto& update) { return update.locked; });
    if (lockedCount > 0) {
        auto* note = new QLabel(tr("%1 of these came with a modpack. Updating them unlocks them, so they no longer follow the modpack's "
                                   "versions.")
                                    .arg(lockedCount),
                                this);
        note->setWordWrap(true);
        layout->addWidget(note);
    }

    auto* selectAll = new QCheckBox(tr("Select all"), this);
    selectAll->setChecked(true);
    layout->addWidget(selectAll);

    auto* splitter = new QSplitter(Qt::Vertical, this);
    m_list = new QTreeWidget(splitter);
    m_list->setRootIsDecorated(false);
    m_list->setHeaderLabels({ tr("Name"), tr("Type"), tr("Installed"), tr("Update") });
    m_list->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    for (int column : { TypeColumn, InstalledColumn, NewColumn }) {
        m_list->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }

    // Grouped by type, then by name.
    QList<int> order(m_updates.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [this](int a, int b) {
        const auto kindA = static_cast<int>(SystemContent::kindFor(m_updates[a].model));
        const auto kindB = static_cast<int>(SystemContent::kindFor(m_updates[b].model));
        return kindA != kindB ? kindA < kindB : m_updates[a].name.compare(m_updates[b].name, Qt::CaseInsensitive) < 0;
    });
    for (int i : order) {
        const auto& update = m_updates[i];
        auto* item = new QTreeWidgetItem(m_list);
        item->setText(NameColumn, update.locked ? tr("%1 (modpack)").arg(update.name) : update.name);
        item->setText(TypeColumn, CelestialBody::kindName(SystemContent::kindFor(update.model)));
        item->setText(InstalledColumn, update.oldVersion);
        item->setText(NewColumn, update.newVersion);
        item->setCheckState(NameColumn, Qt::Checked);
        item->setData(NameColumn, IndexRole, i);
    }

    m_changelog = new QTextBrowser(splitter);
    m_changelog->setOpenExternalLinks(true);
    m_changelog->setPlaceholderText(tr("Select an update to see its changelog."));
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_updateButton = buttons->addButton(QString(), QDialogButtonBox::AcceptRole);
    m_updateButton->setDefault(true);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(selectAll, &QCheckBox::toggled, this, [this](bool checked) {
        for (int i = 0; i < m_list->topLevelItemCount(); i++) {
            m_list->topLevelItem(i)->setCheckState(NameColumn, checked ? Qt::Checked : Qt::Unchecked);
        }
    });
    connect(m_list, &QTreeWidget::itemChanged, this, &UpdatesDialog::refreshButton);
    connect(m_list, &QTreeWidget::currentItemChanged, this, &UpdatesDialog::showChangelog);
    m_list->setCurrentItem(m_list->topLevelItem(0));
    refreshButton();
}

QIcon UpdatesDialog::icon()
{
    QIcon icon;
    for (int size : { 16, 24, 32, 48 }) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal s = size / 16.0;
        QPen pen(QColor(248, 81, 73), 1.9 * s, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        // Arrow down into a tray.
        p.drawLine(QPointF(8 * s, 2 * s), QPointF(8 * s, 10 * s));
        QPainterPath head;
        head.moveTo(4.5 * s, 7 * s);
        head.lineTo(8 * s, 10.5 * s);
        head.lineTo(11.5 * s, 7 * s);
        p.drawPath(head);
        QPainterPath tray;
        tray.moveTo(2.5 * s, 11 * s);
        tray.lineTo(2.5 * s, 13.5 * s);
        tray.lineTo(13.5 * s, 13.5 * s);
        tray.lineTo(13.5 * s, 11 * s);
        p.drawPath(tray);
        icon.addPixmap(pixmap);
    }
    return icon;
}

QList<UpdateChecker::PendingUpdate> UpdatesDialog::selected() const
{
    QList<UpdateChecker::PendingUpdate> result;
    for (int i = 0; i < m_list->topLevelItemCount(); i++) {
        const auto* item = m_list->topLevelItem(i);
        if (item->checkState(NameColumn) == Qt::Checked) {
            result.append(m_updates[item->data(NameColumn, IndexRole).toInt()]);
        }
    }
    return result;
}

void UpdatesDialog::refreshButton()
{
    const auto count = static_cast<int>(selected().size());
    m_updateButton->setText(count == 1 ? tr("Update 1 project") : tr("Update %1 projects").arg(count));
    m_updateButton->setEnabled(count > 0);
}

void UpdatesDialog::showChangelog()
{
    const auto* item = m_list->currentItem();
    if (!item) {
        m_changelog->clear();
        return;
    }
    const auto& update = m_updates[item->data(NameColumn, IndexRole).toInt()];
    const QString text = update.changelog.trimmed().isEmpty() ? tr("No changelog was published for this version.")
                                                              : StringUtils::htmlListPatch(markdownToHTML(update.changelog));
    m_changelog->setHtml(QStringLiteral("<h3>%1 %2</h3>%3").arg(update.name.toHtmlEscaped(), update.newVersion.toHtmlEscaped(), text));
}
