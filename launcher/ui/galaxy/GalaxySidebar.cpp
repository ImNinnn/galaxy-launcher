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

#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QRadialGradient>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <array>
#include <functional>

#include "Application.h"
#include "InstanceList.h"
#include "ProfileOrganizer.h"
#include "CelestialPainter.h"
#include "SunColor.h"
#include "SystemContent.h"
#include "minecraft/MinecraftInstance.h"

/**
 * The sidebar's rows: profiles without a category, then a header per category followed by its profiles unless the
 * category is collapsed. Profile rows forward everything to Prism's instance model. Collapsing uses Prism's own group
 * state, so it is remembered.
 */
class SidebarCategoryModel : public QAbstractListModel {
   public:
    enum Role { HeaderRole = GalaxySidebar::CategoryHeaderRole, CollapsedRole, CountRole };

    SidebarCategoryModel(QAbstractItemModel* source, QObject* parent) : QAbstractListModel(parent), m_source(source)
    {
        m_rebuildTimer.setSingleShot(true);
        m_rebuildTimer.setInterval(0);
        connect(&m_rebuildTimer, &QTimer::timeout, this, &SidebarCategoryModel::rebuild);
        auto later = [this] { m_rebuildTimer.start(); };
        connect(source, &QAbstractItemModel::rowsInserted, this, later);
        connect(source, &QAbstractItemModel::rowsRemoved, this, later);
        connect(source, &QAbstractItemModel::rowsMoved, this, later);
        connect(source, &QAbstractItemModel::modelReset, this, later);
        connect(source, &QAbstractItemModel::layoutChanged, this, later);
        connect(ProfileOrganizer::instance(), &ProfileOrganizer::changed, this, later);
        connect(source, &QAbstractItemModel::dataChanged, this,
                [this, later](const QModelIndex&, const QModelIndex&, const QList<int>& roles) {
                    if (roles.isEmpty() || roles.contains(InstanceList::GroupRole)) {
                        later();
                    } else if (!m_rows.isEmpty()) {
                        emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1), roles);
                    }
                });
        rebuild();
    }

    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : static_cast<int>(m_rows.size()); }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() >= m_rows.size()) {
            return {};
        }
        const auto& row = m_rows[index.row()];
        if (row.source.isValid()) {
            return role == HeaderRole ? QVariant(false) : row.source.data(role);
        }
        const bool collapsed = APPLICATION->instances()->isGroupCollapsed(row.category);
        switch (role) {
            case Qt::DisplayRole:
                return row.category;
            case Qt::ToolTipRole:
                return collapsed ? QObject::tr("%1: click to show its profiles").arg(row.category)
                                 : QObject::tr("%1: click to hide its profiles").arg(row.category);
            case Qt::AccessibleTextRole:
                return QObject::tr("Category %1, %n profile(s), %2", nullptr, row.count)
                    .arg(row.category, collapsed ? QObject::tr("collapsed") : QObject::tr("expanded"));
            case HeaderRole:
                return true;
            case CollapsedRole:
                return collapsed;
            case CountRole:
                return row.count;
            default:
                return {};
        }
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        if (!index.isValid()) {
            return Qt::ItemIsDropEnabled;  // between rows
        }
        if (index.row() >= m_rows.size()) {
            return Qt::NoItemFlags;
        }
        const auto& row = m_rows[index.row()];
        if (row.source.isValid()) {
            return (m_source->flags(row.source) & ~(Qt::ItemIsEditable | Qt::ItemIsDropEnabled)) | Qt::ItemIsDragEnabled;
        }
        // Headers can be dragged too (Qt only drags selected rows, so they are selectable; they never look selected),
        // and dropping a profile onto one puts it in that category.
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    }

    QStringList mimeTypes() const override { return { QString::fromLatin1(ProfileMime), QString::fromLatin1(CategoryMime) }; }
    Qt::DropActions supportedDragActions() const override { return Qt::MoveAction; }
    Qt::DropActions supportedDropActions() const override { return Qt::MoveAction; }

    QMimeData* mimeData(const QModelIndexList& indexes) const override
    {
        auto* data = new QMimeData();
        if (!indexes.isEmpty()) {
            if (const auto category = headerAt(indexes.first().row()); !category.isEmpty()) {
                data->setData(QString::fromLatin1(CategoryMime), category.toUtf8());
                return data;
            }
        }
        QStringList ids;
        for (const auto& index : indexes) {
            if (const auto source = sourceIndex(index.row()); source.isValid()) {
                ids << source.data(InstanceList::InstanceIDRole).toString();
            }
        }
        data->setData(QString::fromLatin1(ProfileMime), QJsonDocument(QJsonArray::fromStringList(ids)).toJson(QJsonDocument::Compact));
        return data;
    }

    bool canDropMimeData(const QMimeData* data, Qt::DropAction, int, int, const QModelIndex&) const override
    {
        return data && (data->hasFormat(QString::fromLatin1(ProfileMime)) || data->hasFormat(QString::fromLatin1(CategoryMime)));
    }

    /**
     * Profiles: dropped onto a header, they go into that category (at its end); between two profiles of a category,
     * into that category there; anywhere else (above a header, between profiles without a category, at the bottom),
     * out of any category, at that place. Categories: moved with all their profiles to the place they are dropped,
     * never inside another category.
     */
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int, const QModelIndex& parent) override
    {
        if (!canDropMimeData(data, action, row, 0, parent)) {
            return false;
        }
        if (row < 0 || row > m_rows.size()) {
            row = static_cast<int>(m_rows.size());
        }
        if (data->hasFormat(QString::fromLatin1(CategoryMime))) {
            return dropCategory(QString::fromUtf8(data->data(QString::fromLatin1(CategoryMime))), row, parent);
        }
        QStringList moved;
        for (const auto& value : QJsonDocument::fromJson(data->data(QString::fromLatin1(ProfileMime))).array()) {
            moved << value.toString();
        }
        moved.removeAll(QString());
        if (moved.isEmpty()) {
            return false;
        }
        QString category;
        QString before;     // in a category: the profile they go before (empty: its end)
        QString topBefore;  // outside: the top level item they go before (empty: the bottom)
        if (parent.isValid()) {
            category = headerAt(parent.row());
        } else if (row < m_rows.size() && m_rows[row].source.isValid() && !m_rows[row].category.isEmpty()) {
            category = m_rows[row].category;
            before = idAt(row);
        } else {
            topBefore = row < m_rows.size() ? tokenAt(row) : QString();
        }
        if (moved.contains(before)) {
            before.clear();
        }

        auto* list = APPLICATION->instances();
        // The profile order (within categories, and what the galaxy follows).
        QStringList all = m_allIds;
        for (const auto& id : moved) {
            all.removeAll(id);
        }
        qsizetype at = -1;
        if (!category.isEmpty()) {
            at = before.isEmpty() ? -1 : all.indexOf(before);
            if (at < 0) {
                at = all.size();
                for (qsizetype i = 0; i < all.size(); i++) {
                    if (list->getInstanceGroup(all[i]) == category) {
                        at = i + 1;
                    }
                }
            }
        } else {
            at = all.size();
            if (topBefore.startsWith(QStringLiteral("p:"))) {
                at = std::max<qsizetype>(0, all.indexOf(topBefore.mid(2)));
            } else if (topBefore.startsWith(QStringLiteral("c:"))) {
                for (qsizetype i = 0; i < all.size(); i++) {
                    if (list->getInstanceGroup(all[i]) == topBefore.mid(2)) {
                        at = i;
                        break;
                    }
                }
            }
        }
        for (qsizetype i = 0; i < moved.size(); i++) {
            all.insert(at + i, moved[i]);
        }
        // The top level: profiles outside categories take their place there; ones moved into a category leave it.
        QStringList top = m_topTokens;
        for (const auto& id : moved) {
            top.removeAll(ProfileOrganizer::profileToken(id));
        }
        if (category.isEmpty()) {
            qsizetype place = topBefore.isEmpty() ? top.size() : top.indexOf(topBefore);
            if (place < 0) {
                place = top.size();
            }
            for (qsizetype i = 0; i < moved.size(); i++) {
                top.insert(place + i, ProfileOrganizer::profileToken(moved[i]));
            }
        }
        QStringList changing;
        for (const auto& id : moved) {
            if (list->getInstanceGroup(id) != category) {
                changing << id;
            }
        }
        auto* organizer = ProfileOrganizer::instance();
        organizer->moveToCategory(changing, category);
        organizer->setOrder(all);
        organizer->setTopOrder(top);
        m_rebuildTimer.start();
        return true;
    }

    QModelIndex sourceIndex(int row) const { return row >= 0 && row < m_rows.size() ? QModelIndex(m_rows[row].source) : QModelIndex(); }

    int rowOf(const QModelIndex& source) const
    {
        for (int r = 0; r < m_rows.size(); r++) {
            if (m_rows[r].source.isValid() && m_rows[r].source == source) {
                return r;
            }
        }
        return -1;
    }

    /** The category of a header row; empty for profile rows. */
    QString headerAt(int row) const
    {
        return row >= 0 && row < m_rows.size() && !m_rows[row].source.isValid() ? m_rows[row].category : QString();
    }

    void toggle(const QString& category)
    {
        if (ProfileOrganizer::instance()->isEmptyCategory(category)) {
            return;  // nothing to show or hide
        }
        auto* list = APPLICATION->instances();
        list->on_GroupStateChanged(category, !list->isGroupCollapsed(category));
        rebuild();
    }

    std::function<void()> rebuilt;

   private:
    QString idAt(int row) const { return m_rows[row].source.data(InstanceList::InstanceIDRole).toString(); }

    /** The top level item a row belongs to: its category, or the profile itself outside categories. */
    QString tokenAt(int row) const
    {
        const auto& r = m_rows[row];
        return r.category.isEmpty() ? ProfileOrganizer::profileToken(idAt(row)) : ProfileOrganizer::categoryToken(r.category);
    }

    bool dropCategory(const QString& name, int row, const QModelIndex& parent)
    {
        if (name.isEmpty()) {
            return false;
        }
        QString before;
        if (parent.isValid()) {
            before = ProfileOrganizer::categoryToken(headerAt(parent.row()));  // onto another header: just above it
        } else {
            // Never inside a category: below its last profile at the earliest.
            int k = row;
            while (k < m_rows.size() && m_rows[k].source.isValid() && !m_rows[k].category.isEmpty() &&
                   (k == 0 || m_rows[k - 1].category == m_rows[k].category)) {
                k++;
            }
            before = k < m_rows.size() ? tokenAt(k) : QString();
        }
        const auto token = ProfileOrganizer::categoryToken(name);
        if (before == token) {
            return false;
        }
        QStringList top = m_topTokens;
        top.removeAll(token);
        qsizetype place = before.isEmpty() ? top.size() : top.indexOf(before);
        if (place < 0) {
            place = top.size();
        }
        top.insert(place, token);
        ProfileOrganizer::instance()->setTopOrder(top);
        m_rebuildTimer.start();
        return true;
    }

    void rebuild()
    {
        m_rebuildTimer.stop();
        auto* list = APPLICATION->instances();
        auto* organizer = ProfileOrganizer::instance();
        // Prism's order (by name or last played), then the order the user dragged profiles into.
        QStringList ids;
        QHash<QString, QPersistentModelIndex> indexOf;
        for (int r = 0; r < m_source->rowCount(); r++) {
            const auto index = m_source->index(r, 0);
            const auto id = index.data(InstanceList::InstanceIDRole).toString();
            ids << id;
            indexOf.insert(id, index);
        }
        organizer->sort(ids);
        QStringList loose;
        QHash<QString, QStringList> byCategory;
        for (const auto& id : ids) {
            const auto category = list->getInstanceGroup(id);
            (category.isEmpty() ? loose : byCategory[category]).append(id);
        }
        QStringList categories = byCategory.keys();
        for (const auto& name : organizer->emptyCategories()) {
            if (!categories.contains(name)) {
                categories << name;  // created, waiting for profiles
            }
        }
        std::sort(categories.begin(), categories.end(),
                  [](const QString& a, const QString& b) { return QString::localeAwareCompare(a.toLower(), b.toLower()) < 0; });

        // The top level: profiles without a category and categories, in the order they were dragged into.
        QStringList top;
        for (const auto& id : loose) {
            top << ProfileOrganizer::profileToken(id);
        }
        for (const auto& category : categories) {
            top << ProfileOrganizer::categoryToken(category);
        }
        organizer->sortTop(top);

        beginResetModel();
        m_rows.clear();
        m_allIds.clear();
        m_topTokens = top;
        for (const auto& token : top) {
            const auto name = token.mid(2);
            if (token.startsWith(QStringLiteral("p:"))) {
                m_allIds << name;
                m_rows.append({ {}, indexOf[name], 0 });
                continue;
            }
            const auto members = byCategory.value(name);
            m_allIds << members;
            m_rows.append({ name, {}, static_cast<int>(members.size()) });
            if (!list->isGroupCollapsed(name)) {
                for (const auto& id : members) {
                    m_rows.append({ name, indexOf[id], 0 });
                }
            }
        }
        endResetModel();
        if (rebuilt) {
            rebuilt();
        }
    }

    struct Row {
        QString category;
        QPersistentModelIndex source;  ///< Invalid for a header.
        int count = 0;                 ///< Headers: profiles in the category.
    };
    static constexpr const char* ProfileMime = "application/x-galaxylauncher-profile-ids";
    static constexpr const char* CategoryMime = "application/x-galaxylauncher-category";
    QAbstractItemModel* m_source;
    QList<Row> m_rows;
    QStringList m_allIds;  ///< Every profile in the sidebar's order, collapsed categories included.
    QStringList m_topTokens;  ///< The top level items in the sidebar's order (see ProfileOrganizer::topOrder()).
    QTimer m_rebuildTimer;
};

namespace {

/** The collapse and expand chevrons, drawn at the size of the sidebar's other icons. */
QIcon chevronIcon(bool pointsRight)
{
    QIcon icon;
    for (const int size : { 16, 32, 48 }) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal unit = size / 16.0;
        QPen pen(QColor(236, 233, 255), 1.7 * unit, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        for (const qreal x : { 4.5, 9.0 }) {
            const qreal tip = pointsRight ? x + 3.0 : x;
            const qreal back = pointsRight ? x : x + 3.0;
            const QPointF points[] = { QPointF(back * unit, 4 * unit), QPointF(tip * unit, 8 * unit), QPointF(back * unit, 12 * unit) };
            p.drawPolyline(points, 3);
        }
        p.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

class ProfileDelegate : public QStyledItemDelegate {
   public:
    explicit ProfileDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

    bool compact = false;
    /** Asks for another frame while a sun fades into its sparkle (or back). */
    std::function<void()> animate;

    /** 0: the sun, 1: the sparkle. When a game starts or stops it fades from one to the other. */
    qreal sparkleAmount(const QString& id, bool running) const
    {
        constexpr qint64 duration = 700;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        auto it = m_fades.find(id);
        if (it == m_fades.end()) {
            m_fades.insert(id, { running, 0 });  // first seen: no fade
            return running ? 1.0 : 0.0;
        }
        auto amount = [&](const Fade& fade) {
            const qreal t = std::clamp(static_cast<qreal>(now - fade.since) / duration, 0.0, 1.0);
            return fade.running ? t : 1 - t;
        };
        if (it->running != running) {
            const qreal current = amount(*it);  // turn around from wherever it is
            it->running = running;
            it->since = now - static_cast<qint64>((running ? current : 1 - current) * duration);
        }
        const qreal value = amount(*it);
        if (now - it->since < duration && animate) {
            animate();
        }
        return value * value * (3 - 2 * value);  // smoothstep
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const int line = option.fontMetrics.height();
        if (index.data(SidebarCategoryModel::HeaderRole).toBool()) {
            return compact ? QSize(48, 20) : QSize(200, line + 14);
        }
        return compact ? QSize(48, 44) : QSize(200, line * 2 + 16);
    }

    void paintHeader(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(option.rect).adjusted(6, 2, -6, -2);
        const bool collapsed = index.data(SidebarCategoryModel::CollapsedRole).toBool();
        const QColor muted(170, 163, 205);
        if (option.state & (QStyle::State_MouseOver | QStyle::State_HasFocus)) {
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(255, 255, 255, 14));
            p->drawRoundedRect(r, 7, 7);
        }
        // A chevron that points down when open and right when collapsed.
        auto chevron = [p](QPointF c, qreal s, bool open) {
            const auto points = open ? std::array<QPointF, 3>{ c + QPointF(-s, -s / 2), c + QPointF(0, s / 2), c + QPointF(s, -s / 2) }
                                     : std::array<QPointF, 3>{ c + QPointF(-s / 2, -s), c + QPointF(s / 2, 0), c + QPointF(-s / 2, s) };
            p->drawPolyline(points.data(), static_cast<int>(points.size()));
        };
        p->setPen(QPen(muted, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->setBrush(Qt::NoBrush);
        if (compact) {
            // Collapsed sidebar: a divider with the chevron in the middle; the name is in the tooltip.
            const qreal y = r.center().y();
            p->setPen(QPen(QColor(124, 104, 200, 120), 1));
            p->drawLine(QPointF(r.left() + 2, y), QPointF(r.center().x() - 9, y));
            p->drawLine(QPointF(r.center().x() + 9, y), QPointF(r.right() - 2, y));
            p->setPen(QPen(muted, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            chevron(r.center(), 3.5, !collapsed);
            p->restore();
            return;
        }
        chevron(QPointF(r.left() + 12, r.center().y()), 3.5, !collapsed);
        QFont font = option.font;
        font.setBold(true);
        font.setPointSizeF(font.pointSizeF() * 0.9);
        const QFontMetricsF fm(font);
        const QString count = QString::number(index.data(SidebarCategoryModel::CountRole).toInt());
        const qreal countWidth = fm.horizontalAdvance(count) + 14;
        const QRectF nameRect(r.left() + 24, r.top(), r.width() - 24 - countWidth - 8, r.height());
        p->setFont(font);
        p->setPen(QColor(196, 181, 253));
        p->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                    fm.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, nameRect.width()));
        const QRectF countRect(r.right() - countWidth - 2, r.center().y() - fm.height() / 2, countWidth, fm.height());
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(124, 77, 255, 50));
        p->drawRoundedRect(countRect, countRect.height() / 2, countRect.height() / 2);
        p->setPen(muted);
        p->drawText(countRect, Qt::AlignCenter, count);
        p->restore();
    }

    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (index.data(SidebarCategoryModel::HeaderRole).toBool()) {
            paintHeader(p, option, index);
            return;
        }
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

        // The profile's sun; while the game runs, a sparkle in the sun's color (as its system is a nebula then).
        const QColor sun = SunColor::get(inst);
        const QPointF sunAt(r.left() + (compact ? r.width() / 2 : 20), r.center().y());
        const qreal sparkle = sparkleAmount(inst->id(), inst->isRunning());
        if (sparkle > 0.01) {
            CelestialPainter::drawSparkle(*p, sunAt, (compact ? 3.0 : 2.7) * (0.6 + 0.4 * sparkle), sun, 0.75, sparkle);
        }
        if (sparkle < 0.99) {
            // The sun shrinks and fades as the sparkle appears.
            p->save();
            p->setOpacity(p->opacity() * (1 - sparkle));
            const qreal sunR = (compact ? 9 : 8) * (1 - 0.45 * sparkle);
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
            p->restore();
        }

        if (!compact) {
            // Profiles without an icon of their own (Prism's "default") show none, and their name uses the room.
            qreal textLeft = r.left() + 38;
            const auto iconKey = inst->iconKey();
            if (!iconKey.isEmpty() && iconKey != QStringLiteral("default")) {
                const auto icon = index.data(Qt::DecorationRole).value<QIcon>();
                const QRectF iconRect(textLeft, r.center().y() - 11, 22, 22);
                icon.paint(p, iconRect.toRect());
                textLeft = iconRect.right() + 8;
            }
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

   private:
    struct Fade {
        bool running = false;
        qint64 since = 0;  ///< When the last fade started (ms since the epoch).
    };
    mutable QHash<QString, Fade> m_fades;
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
    m_collapse->setIconSize(m_add->iconSize());
    m_collapse->setToolTip(tr("Collapse sidebar"));
    m_collapse->setAccessibleName(tr("Collapse sidebar"));
    m_addCategory = new QToolButton(this);
    m_addCategory->setIcon(QIcon::fromTheme("tag"));
    m_addCategory->setToolTip(tr("New category"));
    m_addCategory->setAccessibleName(tr("New category"));
    m_addCategory->setAutoRaise(true);
    header->addWidget(m_title, 1);
    header->addWidget(m_addCategory);
    header->addWidget(m_add);
    header->addWidget(m_collapse);
    layout->addLayout(header);

    m_shared = selection;
    m_model = new SidebarCategoryModel(profiles, this);
    m_list = new QListView(this);
    m_list->setObjectName("galaxyProfileList");
    m_list->setModel(m_model);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    auto* delegate = new ProfileDelegate(m_list);
    m_list->setItemDelegate(delegate);
    auto* frame = new QTimer(this);
    frame->setSingleShot(true);
    frame->setInterval(16);
    connect(frame, &QTimer::timeout, m_list->viewport(), qOverload<>(&QWidget::update));
    delegate->animate = [frame] {
        if (!frame->isActive()) {
            frame->start();
        }
    };
    m_list->setMouseTracking(true);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setUniformItemSizes(false);  // category headers are shorter than profiles
    // Drag profiles to reorder them or to move them into another category.
    m_list->setDragDropMode(QAbstractItemView::DragDrop);
    m_list->setDefaultDropAction(Qt::MoveAction);
    m_list->setDragEnabled(true);
    m_list->setAcceptDrops(true);
    m_list->setDropIndicatorShown(true);
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
    // Clicking (or pressing Enter on) a category header collapses or expands it.
    auto activate = [this, idOf](const QModelIndex& index) {
        if (const auto category = m_model->headerAt(index.row()); !category.isEmpty()) {
            m_model->toggle(category);
        } else {
            emit profileActivated(idOf(index));
        }
    };
    connect(m_list, &QListView::clicked, this, activate);
    connect(m_list, &QListView::activated, this, activate);
    connect(m_list, &QListView::doubleClicked, this, [this, idOf](const QModelIndex& index) {
        if (m_model->headerAt(index.row()).isEmpty()) {
            emit profileDoubleClicked(idOf(index));
        }
    });
    connect(m_list, &QWidget::customContextMenuRequested, this, [this, idOf](const QPoint& pos) {
        const auto index = m_list->indexAt(pos);
        if (!index.isValid()) {
            return;
        }
        if (const auto category = m_model->headerAt(index.row()); !category.isEmpty()) {
            emit categoryContextMenuRequested(category, m_list->viewport()->mapToGlobal(pos));
            return;
        }
        m_list->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
        emit profileContextMenuRequested(idOf(index), m_list->viewport()->mapToGlobal(pos));
    });

    // The list keeps its own selection (its rows include headers); keep it and Prism's in step.
    connect(m_list->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current) {
        const auto source = m_model->sourceIndex(current.row());
        if (m_syncing || !source.isValid()) {
            return;
        }
        m_syncing = true;
        m_shared->setCurrentIndex(source, QItemSelectionModel::ClearAndSelect);
        m_syncing = false;
    });
    connect(m_shared, &QItemSelectionModel::currentChanged, this, &GalaxySidebar::selectFromShared);
    connect(m_shared, &QItemSelectionModel::selectionChanged, this, &GalaxySidebar::selectFromShared);
    m_model->rebuilt = [this] { selectFromShared(); };
    selectFromShared();
    connect(m_add, &QToolButton::clicked, this, &GalaxySidebar::createProfileRequested);
    connect(m_addCategory, &QToolButton::clicked, this, &GalaxySidebar::createCategoryRequested);
    connect(m_settings, &QToolButton::clicked, this, &GalaxySidebar::settingsRequested);
    connect(m_collapse, &QToolButton::clicked, this, [this] {
        setCollapsed(!m_collapsed);
        emit collapsedChanged(m_collapsed);
    });
    // Repaint running badges and sun colors when instances change.
    connect(profiles, &QAbstractItemModel::dataChanged, m_list->viewport(), qOverload<>(&QWidget::update));
    // Prism's list view can collapse categories too.
    connect(APPLICATION->instances(), &InstanceList::groupsChanged, m_list->viewport(), qOverload<>(&QWidget::update));

    setCollapsed(false);
}

void GalaxySidebar::selectFromShared()
{
    if (m_syncing) {
        return;
    }
    m_syncing = true;
    const int row = m_model->rowOf(m_shared->currentIndex());
    if (row >= 0) {
        m_list->selectionModel()->setCurrentIndex(m_model->index(row), QItemSelectionModel::ClearAndSelect);
    } else {
        m_list->selectionModel()->clearSelection();  // the selected profile is in a collapsed category
    }
    m_syncing = false;
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
    m_addCategory->setVisible(!collapsed);
    m_collapse->setIcon(chevronIcon(collapsed));
    m_collapse->setToolTip(collapsed ? tr("Expand sidebar") : tr("Collapse sidebar"));
    m_settings->setToolButtonStyle(collapsed ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
    m_account->setToolButtonStyle(collapsed ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
    m_account->setText(collapsed ? QString() : m_account->property("accountName").toString());
    setFixedWidth(collapsed ? 64 : 248);
    m_list->doItemsLayout();
    m_list->viewport()->update();
}
