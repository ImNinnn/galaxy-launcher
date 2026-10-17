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

#include <QDialog>
#include <QIcon>

#include "UpdateChecker.h"

class QLabel;
class QPushButton;
class QTextBrowser;
class QTreeWidget;

/**
 * Every update available in one profile (mods, resource packs, shader packs, data packs), each with a checkbox and
 * its changelog. Accepting it only reports the selection; ProjectActions::updateAll() installs it.
 */
class UpdatesDialog : public QDialog {
    Q_OBJECT
   public:
    UpdatesDialog(QWidget* parent, const QString& profileName, const QList<UpdateChecker::PendingUpdate>& updates);

    /** The updates that are checked. */
    QList<UpdateChecker::PendingUpdate> selected() const;

    /** The red "updates available" download arrow (galaxy bottom bar and list view). */
    static QIcon icon();

   private:
    void refreshButton();
    void showChangelog();

    QList<UpdateChecker::PendingUpdate> m_updates;
    QTreeWidget* m_list = nullptr;
    QTextBrowser* m_changelog = nullptr;
    QPushButton* m_updateButton = nullptr;
};
