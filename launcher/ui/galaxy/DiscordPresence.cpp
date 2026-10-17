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

#include "DiscordPresence.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUuid>
#include <QtEndian>

#include "Application.h"
#include "BuildConfig.h"
#include "GalaxySettings.h"
#include "InstanceList.h"
#include "SystemContent.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/auth/AccountList.h"
#include "settings/SettingsObject.h"

namespace {

const QString ApplicationId = QStringLiteral("1560321818802262186");
// Used until Discord tells us the current icon (it is the application icon set in the Discord developer portal).
const QString FallbackIconHash = QStringLiteral("275642b4415ea50ce19fa7055805646a");

enum Opcode : qint32 { Handshake = 0, Frame = 1, Close = 2, Ping = 3, Pong = 4 };

QString iconUrl(const QString& hash)
{
    return QStringLiteral("https://cdn.discordapp.com/app-icons/%1/%2.png?size=512").arg(ApplicationId, hash);
}

/** Where Discord (native, Flatpak, Canary, Vesktop, Snap) puts its IPC sockets. */
QStringList socketCandidates()
{
#ifdef Q_OS_WIN
    // Named pipes (\\.\pipe\discord-ipc-N); QLocalSocket takes the bare name.
    QStringList pipes;
    for (int i = 0; i < 10; i++) {
        pipes << QStringLiteral("discord-ipc-%1").arg(i);
    }
    return pipes;
#else
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR", QStringLiteral("/tmp"));
    const QStringList folders = { QString(), QStringLiteral("app/com.discordapp.Discord"), QStringLiteral("app/com.discordapp.DiscordCanary"),
                                  QStringLiteral(".flatpak/dev.vencord.Vesktop/xdg-run"), QStringLiteral("snap.discord") };
    QStringList paths;
    for (const auto& folder : folders) {
        for (int i = 0; i < 10; i++) {
            const QString dir = folder.isEmpty() ? runtime : runtime + '/' + folder;
            paths << QStringLiteral("%1/discord-ipc-%2").arg(dir).arg(i);
        }
    }
    for (int i = 0; i < 10; i++) {
        paths << QStringLiteral("/tmp/discord-ipc-%1").arg(i);
    }
    return paths;
#endif
}

}  // namespace

DiscordPresence::DiscordPresence(QObject* parent) : QObject(parent), m_launcherStart(QDateTime::currentDateTimeUtc())
{
    m_iconUrl = iconUrl(FallbackIconHash);
    m_retry.setInterval(20000);
    connect(&m_retry, &QTimer::timeout, this, &DiscordPresence::connectToDiscord);
    // Discord rate-limits activity updates; coalesce bursts of changes.
    m_update.setSingleShot(true);
    m_update.setInterval(1500);
    connect(&m_update, &QTimer::timeout, this, &DiscordPresence::pushActivity);

    auto* instances = APPLICATION->instances();
    connect(instances, &QAbstractItemModel::rowsInserted, this, &DiscordPresence::watchInstances);
    connect(instances, &QAbstractItemModel::modelReset, this, &DiscordPresence::watchInstances);
    connect(instances, &QAbstractItemModel::dataChanged, this, &DiscordPresence::scheduleUpdate);
    auto* accounts = APPLICATION->accounts();
    connect(accounts, &AccountList::defaultAccountChanged, this, &DiscordPresence::scheduleUpdate);
    connect(accounts, &AccountList::listChanged, this, &DiscordPresence::scheduleUpdate);
    watchInstances();
    fetchApplicationIcon();
    reloadSettings();
}

DiscordPresence::~DiscordPresence()
{
    if (m_socket && m_socket->state() == QLocalSocket::ConnectedState) {
        m_socket->disconnectFromServer();
    }
}

void DiscordPresence::reloadSettings()
{
    m_enabled = APPLICATION->settings()->get(GalaxySettings::DiscordPresence).toBool();
    if (!m_enabled) {
        m_retry.stop();
        if (m_socket) {
            m_socket->abort();  // closing the connection clears the presence
            m_socket->deleteLater();
            m_socket = nullptr;
        }
        m_ready = false;
        return;
    }
    m_retry.start();
    if (!m_socket) {
        connectToDiscord();
    } else {
        scheduleUpdate();
    }
}

void DiscordPresence::fetchApplicationIcon()
{
    auto* reply = APPLICATION->network()->get(
        QNetworkRequest(QUrl(QStringLiteral("https://discord.com/api/v10/applications/%1/rpc").arg(ApplicationId))));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const auto hash = QJsonDocument::fromJson(reply->readAll()).object().value("icon").toString();
        if (!hash.isEmpty()) {
            m_iconUrl = iconUrl(hash);
            scheduleUpdate();
        }
    });
}

void DiscordPresence::watchInstances()
{
    auto* list = APPLICATION->instances();
    for (int i = 0; i < list->count(); i++) {
        auto* instance = list->at(i);
        disconnect(instance, &BaseInstance::runningStatusChanged, this, nullptr);
        connect(instance, &BaseInstance::runningStatusChanged, this, [this, id = instance->id()](bool running) {
            if (running) {
                m_playingSince.insert(id, QDateTime::currentDateTimeUtc());
            } else {
                m_playingSince.remove(id);
            }
            scheduleUpdate();
        });
        if (instance->isRunning() && !m_playingSince.contains(instance->id())) {
            m_playingSince.insert(instance->id(), QDateTime::currentDateTimeUtc());
        }
    }
    scheduleUpdate();
}

void DiscordPresence::connectToDiscord()
{
    if (!m_enabled || (m_socket && m_socket->state() != QLocalSocket::UnconnectedState)) {
        return;
    }
    if (m_socket) {
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    for (const auto& path : socketCandidates()) {
#ifndef Q_OS_WIN
        if (!QFileInfo::exists(path)) {
            continue;
        }
#endif
        auto* socket = new QLocalSocket(this);
        socket->connectToServer(path);
        if (!socket->waitForConnected(300)) {
            socket->deleteLater();
            continue;
        }
        m_socket = socket;
        m_ready = false;
        m_buffer.clear();
        connect(socket, &QLocalSocket::readyRead, this, &DiscordPresence::onReadyRead);
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            if (socket == m_socket) {
                m_ready = false;
                m_socket->deleteLater();
                m_socket = nullptr;
            }
        });
        send(Handshake, QJsonObject{ { "v", 1 }, { "client_id", ApplicationId } });
        qDebug() << "Discord: connected to" << path;
        return;
    }
}

void DiscordPresence::send(int opcode, const QJsonObject& payload)
{
    if (!m_socket || m_socket->state() != QLocalSocket::ConnectedState) {
        return;
    }
    const QByteArray json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QByteArray frame(8, Qt::Uninitialized);
    qToLittleEndian<qint32>(opcode, frame.data());
    qToLittleEndian<qint32>(static_cast<qint32>(json.size()), frame.data() + 4);
    m_socket->write(frame + json);
    m_socket->flush();
}

void DiscordPresence::onReadyRead()
{
    m_buffer += m_socket->readAll();
    while (m_buffer.size() >= 8) {
        const auto opcode = qFromLittleEndian<qint32>(m_buffer.constData());
        const auto length = qFromLittleEndian<qint32>(m_buffer.constData() + 4);
        if (length < 0 || m_buffer.size() < 8 + length) {
            return;
        }
        const auto message = QJsonDocument::fromJson(m_buffer.mid(8, length)).object();
        m_buffer.remove(0, 8 + length);
        switch (opcode) {
            case Frame:
                if (message.value("evt").toString() == "READY") {
                    m_ready = true;
                    pushActivity();
                } else if (message.value("evt").toString() == "ERROR") {
                    qWarning() << "Discord:" << message.value("data").toObject().value("message").toString();
                }
                break;
            case Ping:
                send(Pong, message);
                break;
            case Close:
                qDebug() << "Discord: connection closed:" << message.value("message").toString();
                m_socket->abort();
                return;
            default:
                break;
        }
    }
}

void DiscordPresence::scheduleUpdate()
{
    if (m_ready && !m_update.isActive()) {
        m_update.start();
    }
}

MinecraftInstance* DiscordPresence::playingInstance() const
{
    // The most recently started game.
    MinecraftInstance* latest = nullptr;
    QDateTime latestStart;
    for (auto it = m_playingSince.constBegin(); it != m_playingSince.constEnd(); ++it) {
        auto* instance = APPLICATION->instances()->getInstanceById(it.key());
        if (instance && instance->isRunning() && (!latest || it.value() > latestStart)) {
            latest = instance;
            latestStart = it.value();
        }
    }
    return latest;
}

void DiscordPresence::pushActivity()
{
    if (!m_ready) {
        return;
    }
    QJsonObject activity;
    QJsonObject assets{ { "large_image", m_iconUrl }, { "large_text", BuildConfig.LAUNCHER_DISPLAYNAME } };
    if (auto* playing = playingInstance()) {
        auto* profile = playing->getPackProfile();
        const auto version = profile ? profile->getComponentVersion("net.minecraft") : QString();
        activity.insert("details", tr("Playing %1").arg(playing->name()));
        activity.insert("state", QStringLiteral("%1 %2").arg(SystemContent::loaderName(playing), version).trimmed());
        activity.insert("timestamps", QJsonObject{ { "start", m_playingSince.value(playing->id()).toSecsSinceEpoch() } });
    } else {
        activity.insert("details", tr("In the launcher"));
        activity.insert("timestamps", QJsonObject{ { "start", m_launcherStart.toSecsSinceEpoch() } });
    }
    // The active account's Minecraft head, with its name on hover.
    if (auto account = APPLICATION->accounts()->defaultAccount(); account && !account->profileId().isEmpty()) {
        assets.insert("small_image", QStringLiteral("https://mc-heads.net/avatar/%1/64").arg(account->profileId()));
        assets.insert("small_text", account->profileName());
    }
    activity.insert("assets", assets);

    send(Frame, QJsonObject{ { "cmd", "SET_ACTIVITY" },
                             { "args", QJsonObject{ { "pid", static_cast<qint64>(QCoreApplication::applicationPid()) }, { "activity", activity } } },
                             { "nonce", QUuid::createUuid().toString(QUuid::WithoutBraces) } });
}
