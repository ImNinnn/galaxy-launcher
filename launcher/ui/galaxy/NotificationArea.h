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

#include <QHash>
#include <QList>
#include <QWidget>

#include <functional>

class NotificationCard;

/**
 * Shows Notifications as cards in the top right corner of its parent (the column right of the sidebar, under the
 * top bar), newest at the bottom. Cards slide in from the right, slide out when they close, and the others glide into
 * place (no motion with Reduce motion). Cards hide themselves after their timeout; pointing at one keeps it.
 *
 * The area covers its parent but is masked to its cards, so everything else stays clickable.
 */
class NotificationArea : public QWidget {
    Q_OBJECT
   public:
    explicit NotificationArea(QWidget* parent);

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

   private:
    void add(int id);
    void refresh(int id);
    void remove(int id);
    /** Moves every card to its place; `animate` glides them there. */
    void arrange(bool animate);
    void slide(QWidget* card, const QPoint& to, bool animate, const std::function<void()>& done = {});
    void updateMask();
    bool reducedMotion() const;

    QList<int> m_order;  ///< Card IDs, oldest first (top).
    QHash<int, NotificationCard*> m_cards;
};
