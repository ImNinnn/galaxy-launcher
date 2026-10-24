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

#include <QList>
#include <QObject>
#include <QString>

#include <cstdint>
#include <functional>
#include <optional>

class Task;

/**
 * The launcher's notifications: shown in the main window's top right corner (NotificationArea), and also as desktop
 * notifications when the launcher window is not active, for the ones that ask for it (a game that stopped or crashed,
 * a failed launch, finished downloads).
 *
 * Desktop notifications go through the notification portal in the Flatpak, the freedesktop notification service on
 * other Linux systems, and the system tray elsewhere. They can be turned off in Settings › Galaxy.
 */
class Notifications : public QObject {
    Q_OBJECT
   public:
    enum class Kind : std::uint8_t { Info, Success, Warning, Error, Progress };

    struct Action {
        QString label;
        std::function<void()> run;
    };

    struct Notification {
        int id = 0;
        Kind kind = Kind::Info;
        QString title;
        QString text;
        QList<Action> actions;
        int progress = -1;    ///< Progress notifications: 0 to 100, or -1 while it cannot be measured.
        int timeout = -1;     ///< Milliseconds until it hides itself; -1 picks the kind's default, 0 keeps it.
        bool system = false;  ///< Also a desktop notification when the launcher window is not active.
    };

    static Notifications* instance();

    /** Shows a notification and returns its ID. */
    int post(Notification notification);
    /** Shows a notification with just a title, a text and optional actions. */
    int post(Kind kind, const QString& title, const QString& text, QList<Action> actions = {}, bool system = false);
    /** Changes a shown notification (a progress update, or a progress notification turning into its result). */
    void replace(int id, Notification notification);
    void dismiss(int id);
    std::optional<Notification> find(int id) const;
    QList<Notification> active() const { return m_active; }

    /**
     * A progress notification that follows a task's status and progress, with a button that aborts it when
     * `abortLabel` is set. Updates the notification `id` when given, else posts a new one. Returns its ID.
     */
    int track(Task* task, const QString& title, const QString& abortLabel = {}, int id = 0);

    /**
     * Shows a task's progress as a notification and waits (in a local event loop) until it ends: what Prism's progress
     * window does, without the window. Returns QDialog::Accepted when the task succeeded, else QDialog::Rejected.
     * A success turns into a short "finished" notification; failures are reported by the caller, as before.
     */
    int waitFor(Task* task, const QString& title, const QString& abortLabel = {});

    /** True once a NotificationArea shows notifications (the main window exists). */
    bool isShown() const { return m_shown; }
    void setShown(bool shown) { m_shown = shown; }

    /** The default time a notification of this kind stays, in milliseconds (0: until closed). */
    static int defaultTimeout(Kind kind);

   signals:
    void posted(int id);
    void changed(int id);
    void dismissed(int id);

   private:
    Notifications() = default;
    void showOnDesktop(const Notification& notification);

    QList<Notification> m_active;
    int m_nextId = 1;
    bool m_shown = false;
};
