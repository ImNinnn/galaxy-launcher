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

#include <QObject>
#include <QStringList>

class QWidget;

/**
 * The user's own arrangement of profiles: the order they were dragged into in the sidebar (profiles within and
 * outside categories, and the categories themselves), and categories created before any profile was put in them. Categories themselves are Prism's instance groups.
 *
 * Both are stored as Galaxy settings. Profiles missing from the order (new ones) follow the ordered ones in Prism's
 * order; an empty category is forgotten as soon as a profile is in it.
 */
class ProfileOrganizer : public QObject {
    Q_OBJECT
   public:
    static ProfileOrganizer* instance();

    /** Instance IDs in the user's order; empty until profiles are first reordered. */
    QStringList order() const { return m_order; }
    void setOrder(const QStringList& ids);
    /** Sorts instance IDs: ordered ones first, in the user's order; the rest keep their relative order. */
    void sort(QStringList& ids) const;

    /**
     * The order of the sidebar's top level: profiles without a category ("p:<id>") and categories ("c:<name>"), in
     * any mix. Empty until something is first dragged; new profiles come first, new categories last.
     */
    QStringList topOrder() const { return m_top; }
    void setTopOrder(const QStringList& tokens);
    static QString profileToken(const QString& id) { return QStringLiteral("p:") + id; }
    static QString categoryToken(const QString& name) { return QStringLiteral("c:") + name; }
    /** Sorts top level tokens by the saved order (see topOrder()). */
    void sortTop(QStringList& tokens) const;
    /** Keeps a category's place in the order when it is renamed. */
    void renameInOrder(const QString& from, const QString& to);

    /** Categories created but not used yet. */
    QStringList emptyCategories() const { return m_empty; }
    /** Every category: Prism's groups and the empty ones, sorted by name. */
    QStringList categories() const;
    bool isEmptyCategory(const QString& name) const;
    void removeEmptyCategory(const QString& name);
    void renameEmptyCategory(const QString& from, const QString& to);

    /**
     * Asks for a new category's name and which profiles go in it (`selectedId` is ticked to start with). Without
     * profiles, the category is kept empty until one is moved in. Returns false when cancelled.
     */
    bool createCategory(QWidget* parent, const QString& selectedId = {});

    /** Moves profiles into a category (empty for none). */
    void moveToCategory(const QStringList& ids, const QString& category);

   signals:
    /** The order or the empty categories changed. */
    void changed();

   private:
    ProfileOrganizer();
    void save();

    QStringList m_order;
    QStringList m_empty;
    QStringList m_top;
};
