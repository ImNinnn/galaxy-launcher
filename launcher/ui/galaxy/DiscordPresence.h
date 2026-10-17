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

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QTimer>

class QLocalSocket;
class MinecraftInstance;

/**
 * Discord Rich Presence over Discord's local IPC socket.
 *
 * In the launcher: "In the launcher". While a profile plays: "Playing <profile>" and "<loader> <Minecraft version>",
 * with the elapsed time. The large image is the Discord application's icon, the small image the active account's
 * Minecraft head (its name on hover). Reconnects quietly when Discord starts later or restarts; does nothing when
 * Discord is not running or the setting is off.
 */
class DiscordPresence : public QObject {
    Q_OBJECT
   public:
    explicit DiscordPresence(QObject* parent = nullptr);
    ~DiscordPresence() override;

    /** Re-reads the setting (Settings > Galaxy). */
    void reloadSettings();

   private:
    void connectToDiscord();
    void onReadyRead();
    void send(int opcode, const QJsonObject& payload);
    void scheduleUpdate();
    void pushActivity();
    void watchInstances();
    void fetchApplicationIcon();
    MinecraftInstance* playingInstance() const;

    QLocalSocket* m_socket = nullptr;
    QByteArray m_buffer;
    bool m_ready = false;
    bool m_enabled = true;
    QTimer m_retry;
    QTimer m_update;
    QString m_iconUrl;
    QDateTime m_launcherStart;
    QHash<QString, QDateTime> m_playingSince;  ///< instance id -> when the game started
};
