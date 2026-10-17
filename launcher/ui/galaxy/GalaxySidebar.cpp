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

#include "GalaxySidebar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QRadialGradient>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

#include "Application.h"
#include "InstanceList.h"
#include "SunColor.h"
#include "SystemContent.h"
#include "minecraft/MinecraftInstance.h"

namespace {

class ProfileDelegate : public QStyledItemDelegate {
   public:
    explicit ProfileDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

    bool compact = false;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override
    {
        const int line = option.fontMetrics.height();
        return compact ? QSize(48, 44) : QSize(200, line * 2 + 16);
    }

    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        auto* inst = APPLICATION->instances()->getInstanceById(index.data(InstanceList::InstanceIDRole).toString());
        if (!inst) {
            return;
        }
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(option.rect).adjusted(6, 2, -6, -2);
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const QColor accent(167, 139, 250);

        if (selected || hovered) {
            p->setPen(selected ? QPen(QColor(167, 139, 250, 170), 1) : Qt::NoPen);
            p->setBrush(selected ? QColor(124, 77, 255, 70) : QColor(255, 255, 255, 14));
            p->drawRoundedRect(r, 9, 9);
        }
        if (selected) {
            // A bar on the left edge marks the selection without relying on color alone.
            p->setPen(Qt::NoPen);
            p->setBrush(accent);
            p->drawRoundedRect(QRectF(r.left(), r.top() + 8, 3, r.height() - 16), 1.5, 1.5);
        }

        // The profile's sun.
        const QColor sun = SunColor::get(inst);
        const qreal sunR = compact ? 9 : 8;
        const QPointF sunAt(r.left() + (compact ? r.width() / 2 : 20), r.center().y());
        QRadialGradient glow(sunAt, sunR * 2.2);
        glow.setColorAt(0, QColor(sun.red(), sun.green(), sun.blue(), 110));
        glow.setColorAt(1, QColor(sun.red(), sun.green(), sun.blue(), 0));
        p->setPen(Qt::NoPen);
        p->setBrush(glow);
        p->drawEllipse(sunAt, sunR * 2.2, sunR * 2.2);
        QRadialGradient core(sunAt, sunR, sunAt - QPointF(sunR * 0.3, sunR * 0.3));
        core.setColorAt(0, sun.lighter(150));
        core.setColorAt(1, sun);
        p->setBrush(core);
        p->drawEllipse(sunAt, sunR, sunR);

        if (inst->isRunning()) {
            p->setPen(QPen(QColor(134, 239, 172), 1.5));
            p->setBrush(Qt::NoBrush);
            p->drawEllipse(sunAt, sunR + 3.5, sunR + 3.5);
        }

        if (!compact) {
            const auto icon = index.data(Qt::DecorationRole).value<QIcon>();
            const QRectF iconRect(r.left() + 38, r.center().y() - 11, 22, 22);
            icon.paint(p, iconRect.toRect());

            const qreal textLeft = iconRect.right() + 8;
            const qreal textWidth = r.right() - textLeft - 6;
            QFont nameFont = option.font;
            nameFont.setBold(selected);
            const QFontMetricsF fm(nameFont);
            QString badge = inst->isRunning() ? QObject::tr("Running") : QString();
            const qreal badgeWidth = badge.isEmpty() ? 0 : QFontMetricsF(option.font).horizontalAdvance(badge) + 14;

            p->setFont(nameFont);
            p->setPen(QColor(236, 233, 255));
            const QRectF nameRect(textLeft, r.top() + 5, textWidth - badgeWidth, fm.height());
            p->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText(inst->name(), Qt::ElideRight, nameRect.width()));

            if (!badge.isEmpty()) {
                QFont badgeFont = option.font;
                badgeFont.setPointSizeF(badgeFont.pointSizeF() * 0.8);
                p->setFont(badgeFont);
                const QRectF badgeRect(r.right() - badgeWidth - 2, nameRect.top() + 1, badgeWidth, fm.height() - 2);
                p->setPen(QPen(QColor(134, 239, 172, 200), 1));
                p->setBrush(QColor(34, 80, 50, 160));
                p->drawRoundedRect(badgeRect, badgeRect.height() / 2, badgeRect.height() / 2);
                p->setPen(QColor(200, 255, 215));
                p->drawText(badgeRect, Qt::AlignCenter, badge);
            }

            QFont subFont = option.font;
            subFont.setPointSizeF(subFont.pointSizeF() * 0.86);
            p->setFont(subFont);
            p->setPen(QColor(170, 163, 205));
            const auto loaders = SystemContent::loaderSummary(inst);
            QString sub = SystemContent::minecraftVersion(inst);
            if (!loaders.isEmpty()) {
                sub += QStringLiteral(" · ") + loaders;
            }
            const QFontMetricsF sfm(subFont);
            const QRectF subRect(textLeft, nameRect.bottom() + 1, textWidth, sfm.height());
            p->drawText(subRect, Qt::AlignLeft | Qt::AlignVCenter, sfm.elidedText(sub, Qt::ElideRight, subRect.width()));
        }
        p->restore();
    }
};

}  // namespace

GalaxySidebar::GalaxySidebar(QAbstractItemModel* profiles, QItemSelectionModel* selection, QWidget* parent) : QWidget(parent)
{
    setObjectName("galaxySidebar");
    setAttribute(Qt::WA_StyledBackground);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 10, 8, 10);
    layout->setSpacing(6);

    auto* header = new QHBoxLayout();
    header->setSpacing(4);
    m_title = new QLabel(tr("Profiles"), this);
    m_title->setObjectName("galaxySidebarTitle");
    m_add = new QToolButton(this);
    m_add->setIcon(QIcon::fromTheme("new"));
    m_add->setToolTip(tr("New profile"));
    m_add->setAccessibleName(tr("New profile"));
    m_add->setAutoRaise(true);
    m_collapse = new QToolButton(this);
    m_collapse->setAutoRaise(true);
    m_collapse->setText("«");
    m_collapse->setToolTip(tr("Collapse sidebar"));
    m_collapse->setAccessibleName(tr("Collapse sidebar"));
    header->addWidget(m_title, 1);
    header->addWidget(m_add);
    header->addWidget(m_collapse);
    layout->addLayout(header);

    m_list = new QListView(this);
    m_list->setObjectName("galaxyProfileList");
    m_list->setModel(profiles);
    m_list->setSelectionModel(selection);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setItemDelegate(new ProfileDelegate(m_list));
    m_list->setMouseTracking(true);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setUniformItemSizes(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setAccessibleName(tr("Profiles"));
    layout->addWidget(m_list, 1);

    m_account = new QToolButton(this);
    m_account->setObjectName("galaxyAccountButton");
    m_account->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_account->setPopupMode(QToolButton::InstantPopup);
    m_account->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_account->setIconSize(QSize(22, 22));
    m_account->setToolTip(tr("Minecraft accounts"));
    m_account->setAccessibleName(tr("Minecraft accounts"));
    layout->addWidget(m_account);

    m_settings = new QToolButton(this);
    m_settings->setObjectName("galaxySettingsButton");
    m_settings->setIcon(QIcon::fromTheme("settings"));
    m_settings->setText(tr("Settings"));
    m_settings->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_settings->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_settings->setToolTip(tr("Launcher settings"));
    layout->addWidget(m_settings);

    auto idOf = [](const QModelIndex& index) { return index.data(InstanceList::InstanceIDRole).toString(); };
    connect(m_list, &QListView::clicked, this, [this, idOf](const QModelIndex& index) { emit profileActivated(idOf(index)); });
    connect(m_list, &QListView::activated, this, [this, idOf](const QModelIndex& index) {
        // Keyboard activation (Enter) behaves like a click.
        emit profileActivated(idOf(index));
    });
    connect(m_list, &QListView::doubleClicked, this, [this, idOf](const QModelIndex& index) { emit profileDoubleClicked(idOf(index)); });
    connect(m_list, &QWidget::customContextMenuRequested, this, [this, idOf](const QPoint& pos) {
        const auto index = m_list->indexAt(pos);
        if (index.isValid()) {
            m_list->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
            emit profileContextMenuRequested(idOf(index), m_list->viewport()->mapToGlobal(pos));
        }
    });
    connect(m_add, &QToolButton::clicked, this, &GalaxySidebar::createProfileRequested);
    connect(m_settings, &QToolButton::clicked, this, &GalaxySidebar::settingsRequested);
    connect(m_collapse, &QToolButton::clicked, this, [this] {
        setCollapsed(!m_collapsed);
        emit collapsedChanged(m_collapsed);
    });
    // Repaint running badges and sun colors when instances change.
    connect(profiles, &QAbstractItemModel::dataChanged, m_list->viewport(), qOverload<>(&QWidget::update));

    setCollapsed(false);
}

void GalaxySidebar::setAccountMenu(QMenu* menu)
{
    m_account->setMenu(menu);
}

void GalaxySidebar::setAccount(const QString& name, const QIcon& face)
{
    m_account->setText(m_collapsed ? QString() : name);
    m_account->setIcon(face);
    m_account->setToolTip(tr("Minecraft accounts: %1").arg(name));
    m_account->setProperty("accountName", name);
}

void GalaxySidebar::refresh()
{
    m_list->viewport()->update();
}

void GalaxySidebar::setCollapsed(bool collapsed)
{
    m_collapsed = collapsed;
    static_cast<ProfileDelegate*>(m_list->itemDelegate())->compact = collapsed;
    m_title->setVisible(!collapsed);
    m_add->setVisible(!collapsed);
    m_collapse->setText(collapsed ? "»" : "«");
    m_collapse->setToolTip(collapsed ? tr("Expand sidebar") : tr("Collapse sidebar"));
    m_settings->setToolButtonStyle(collapsed ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
    m_account->setToolButtonStyle(collapsed ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
    m_account->setText(collapsed ? QString() : m_account->property("accountName").toString());
    setFixedWidth(collapsed ? 64 : 248);
    m_list->doItemsLayout();
    m_list->viewport()->update();
}
