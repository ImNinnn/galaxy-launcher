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

#include "Notifications.h"

#include <QApplication>
#include <QDialog>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QPointer>
#include <QSystemTrayIcon>
#include <QTimer>

#if defined(Q_OS_LINUX) && defined(WITH_QTDBUS)
#include <QDBusConnection>
#include <QDBusMessage>
#endif

#include "Application.h"
#include "BuildConfig.h"
#include "GalaxySettings.h"
#include "settings/SettingsObject.h"
#include "tasks/Task.h"

namespace {
constexpr int MaxShown = 6;
}

Notifications* Notifications::instance()
{
    static auto* notifications = new Notifications();
    return notifications;
}

int Notifications::defaultTimeout(Kind kind)
{
    switch (kind) {
        case Kind::Info:
        case Kind::Success:
            return 6000;
        case Kind::Warning:
            return 12000;
        case Kind::Error:
        case Kind::Progress:
            break;
    }
    return 0;  // errors stay until closed; progress until its task ends
}

int Notifications::post(Notification notification)
{
    notification.id = m_nextId++;
    // Too many at once: the oldest finished one makes room.
    if (m_active.size() >= MaxShown) {
        for (const auto& old : std::as_const(m_active)) {
            if (old.kind != Kind::Progress) {
                dismiss(old.id);
                break;
            }
        }
    }
    m_active.append(notification);
    emit posted(notification.id);
    if (notification.system) {
        showOnDesktop(notification);
    }
    return notification.id;
}

int Notifications::post(Kind kind, const QString& title, const QString& text, QList<Action> actions, bool system)
{
    Notification notification;
    notification.kind = kind;
    notification.title = title;
    notification.text = text;
    notification.actions = std::move(actions);
    notification.system = system;
    return post(notification);
}

void Notifications::replace(int id, Notification notification)
{
    for (auto& active : m_active) {
        if (active.id == id) {
            const bool shownOnDesktop = active.system;
            notification.id = id;
            active = notification;
            emit changed(id);
            if (notification.system && !shownOnDesktop) {
                showOnDesktop(notification);  // e.g. a launch or download that just ended
            }
            return;
        }
    }
    if (notification.kind != Kind::Progress) {
        post(notification);  // closed meanwhile: the result still matters
    }
}

void Notifications::dismiss(int id)
{
    for (int i = 0; i < m_active.size(); i++) {
        if (m_active[i].id == id) {
            m_active.removeAt(i);
            emit dismissed(id);
            return;
        }
    }
}

std::optional<Notifications::Notification> Notifications::find(int id) const
{
    for (const auto& active : m_active) {
        if (active.id == id) {
            return active;
        }
    }
    return std::nullopt;
}

int Notifications::track(Task* task, const QString& title, const QString& abortLabel, int id)
{
    Notification notification;
    notification.kind = Kind::Progress;
    notification.title = title;
    notification.text = task->getStatus();
    if (task->getTotalProgress() > 0) {
        notification.progress = static_cast<int>(100 * task->getProgress() / task->getTotalProgress());
    }
    if (!abortLabel.isEmpty()) {
        notification.actions.append({ abortLabel, [task = QPointer<Task>(task)] {
                                         if (task && task->canAbort()) {
                                             task->abort();
                                         }
                                     } });
    }
    if (id > 0 && find(id)) {
        replace(id, notification);
    } else {
        id = post(notification);
    }
    connect(task, &Task::status, this, [this, id](const QString& status) {
        if (auto current = find(id); current && current->kind == Kind::Progress && !status.isEmpty()) {
            current->text = status;
            replace(id, *current);
        }
    });
    // A finished (or no longer abortable) task has nothing left to skip or cancel.
    auto dropAbort = [this, id] {
        if (auto current = find(id); current && current->kind == Kind::Progress && !current->actions.isEmpty()) {
            current->actions.clear();
            replace(id, *current);
        }
    };
    connect(task, &Task::finished, this, dropAbort);
    connect(task, &Task::abortStatusChanged, this, [dropAbort](bool canAbort) {
        if (!canAbort) {
            dropAbort();
        }
    });
    connect(task, &Task::progress, this, [this, id](qint64 done, qint64 total) {
        if (auto current = find(id); current && current->kind == Kind::Progress) {
            const int percent = total > 0 ? static_cast<int>(std::clamp<qint64>(100 * done / total, 0, 100)) : -1;
            if (percent != current->progress) {
                current->progress = percent;
                replace(id, *current);
            }
        }
    });
    return id;
}

int Notifications::waitFor(Task* task, const QString& title, const QString& abortLabel)
{
    // Without a title from the caller, the task's first status names it.
    const bool named = !title.isEmpty();
    const int id = track(task, named ? title : tr("Working…"), abortLabel);
    if (!named) {
        connect(task, &Task::status, this, [this, id](const QString& status) {
            if (auto current = find(id); current && current->kind == Kind::Progress && current->title == tr("Working…") && !status.isEmpty()) {
                current->title = status;
                current->text.clear();
                replace(id, *current);
            }
        });
    }
    if (!task->isRunning() && !task->isFinished()) {
        QMetaObject::invokeMethod(task, &Task::start, Qt::QueuedConnection);
    }
    if (!task->isFinished()) {
        QEventLoop wait;
        connect(task, &Task::finished, &wait, &QEventLoop::quit);
        wait.exec();
    }
    const bool succeeded = task->wasSuccessful();
    if (const auto current = find(id); current && succeeded) {
        Notification done;
        done.kind = Kind::Success;
        done.title = current->title;
        done.text = tr("Finished.");
        done.timeout = 3500;
        replace(id, done);
    } else {
        dismiss(id);
    }
    return succeeded ? QDialog::Accepted : QDialog::Rejected;
}

void Notifications::showOnDesktop(const Notification& notification)
{
    // Only when the launcher is in the background, and never from the developer tour (it must not reach the desktop).
    if (QGuiApplication::applicationState() == Qt::ApplicationActive || !APPLICATION->settings()->get(GalaxySettings::SystemNotifications).toBool() ||
        qEnvironmentVariableIsSet("GALAXY_DEV_SNAPSHOTS")) {
        return;
    }
    const bool urgent = notification.kind == Kind::Error;
#if defined(Q_OS_LINUX) && defined(WITH_QTDBUS)
    auto bus = QDBusConnection::sessionBus();
    if (QFile::exists(QStringLiteral("/.flatpak-info"))) {
        auto message = QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
                                                      "org.freedesktop.portal.Notification", "AddNotification");
        const QVariantMap content{ { "title", notification.title },
                                   { "body", notification.text },
                                   { "priority", urgent ? QStringLiteral("high") : QStringLiteral("normal") } };
        message << QStringLiteral("galaxy-%1").arg(notification.id) << content;
        bus.send(message);  // no reply needed
    } else {
        auto message = QDBusMessage::createMethodCall("org.freedesktop.Notifications", "/org/freedesktop/Notifications",
                                                      "org.freedesktop.Notifications", "Notify");
        const QVariantMap hints{ { "urgency", QVariant::fromValue<uchar>(urgent ? 2 : 1) } };
        message << BuildConfig.LAUNCHER_DISPLAYNAME << 0U << BuildConfig.LAUNCHER_APPID << notification.title << notification.text
                << QStringList() << hints << -1;
        bus.send(message);  // no reply needed
    }
#else
    // A tray icon that only exists while its message is shown.
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }
    static QPointer<QSystemTrayIcon> tray;
    if (!tray) {
        tray = new QSystemTrayIcon(QApplication::windowIcon(), qApp);
        tray->setToolTip(BuildConfig.LAUNCHER_DISPLAYNAME);
    }
    tray->show();
    tray->showMessage(notification.title, notification.text, urgent ? QSystemTrayIcon::Critical : QSystemTrayIcon::Information, 8000);
    QTimer::singleShot(10000, tray, [] {
        if (tray) {
            tray->hide();
        }
    });
#endif
}
