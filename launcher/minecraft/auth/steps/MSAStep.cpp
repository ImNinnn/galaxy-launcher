// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "MSAStep.h"

#include <QAbstractOAuth2>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QHostAddress>
#include <QOAuthHttpServerReplyHandler>

#include "Application.h"
#include "BuildConfig.h"
#include "FileSystem.h"

class LoggingOAuthHttpServerReplyHandler final : public QOAuthHttpServerReplyHandler {
    Q_OBJECT

   public:
    // Galaxy Launcher: only reachable from this computer (the redirect carries a one-time sign-in code).
    explicit LoggingOAuthHttpServerReplyHandler(QObject* parent = nullptr)
        : QOAuthHttpServerReplyHandler(QHostAddress::LocalHost, 0, parent)
    {}

   protected:
    void networkReplyFinished(QNetworkReply* reply) override
    {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "OAuth2 request failed:" << reply->readAll();
        }

        QOAuthHttpServerReplyHandler::networkReplyFinished(reply);
    }
};

MSAStep::MSAStep(AccountData* data, bool silent) : AuthStep(data), m_silent(silent)
{
    m_clientId = APPLICATION->getMSAClientID();
    // Galaxy Launcher: always sign in through the browser and a local page. The launcher's own Microsoft application
    // is registered for http://localhost:<port>/; another application ID (Settings > APIs, e.g. Prism Launcher's) gets
    // Qt's default http://127.0.0.1:<port>/, which is what Prism Launcher uses. The page reads the result from its own
    // address, so it shows success or the error Microsoft sent back.
    auto replyHandler = new LoggingOAuthHttpServerReplyHandler(this);
    const bool builtInApplication = m_clientId == BuildConfig.MSA_CLIENT_ID;
    replyHandler->setCallbackHost(builtInApplication ? QStringLiteral("localhost") : QStringLiteral("127.0.0.1"));
    QFile page(QStringLiteral(":/galaxy/login-callback.html"));
    if (page.open(QIODevice::ReadOnly)) {
        replyHandler->setCallbackText(QString::fromUtf8(page.readAll()));
    }
    m_oauth2.setReplyHandler(replyHandler);
    m_oauth2.setAuthorizationUrl(QUrl("https://login.microsoftonline.com/consumers/oauth2/v2.0/authorize"));
    m_oauth2.setAccessTokenUrl(QUrl("https://login.microsoftonline.com/consumers/oauth2/v2.0/token"));
    m_oauth2.setScope("XboxLive.SignIn XboxLive.offline_access");
    m_oauth2.setClientIdentifier(m_clientId);
    m_oauth2.setNetworkAccessManager(APPLICATION->network());

    connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::granted, this, [this] {
        m_data->msaClientID = m_oauth2.clientIdentifier();
        m_data->msaToken.issueInstant = QDateTime::currentDateTimeUtc();
        m_data->msaToken.notAfter = m_oauth2.expirationAt();
        m_data->msaToken.extra = m_oauth2.extraTokens();
        m_data->msaToken.refresh_token = m_oauth2.refreshToken();
        m_data->msaToken.token = m_oauth2.token();
        emit finished(AccountTaskState::STATE_WORKING, tr("Got MSA token"));
    });
    connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, this, &MSAStep::authorizeWithBrowser);
    connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::requestFailed, this, [this, silent](const QAbstractOAuth2::Error err) {
        auto state = AccountTaskState::STATE_FAILED_HARD;
        if (m_oauth2.status() == QAbstractOAuth::Status::Granted || silent) {
            if (err == QAbstractOAuth2::Error::NetworkError) {
                state = AccountTaskState::STATE_OFFLINE;
            } else {
                state = AccountTaskState::STATE_FAILED_SOFT;
            }
        }
        auto message = tr("Microsoft user authentication failed.");
        if (silent) {
            message = tr("Failed to refresh token.");
        }
        qWarning() << message;
        emit finished(state, message);
    });
    connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::error, this,
            [this](const QString& error, const QString& errorDescription, const QUrl& uri) {
                qWarning() << "Failed to login because" << error << errorDescription;
                emit finished(AccountTaskState::STATE_FAILED_HARD, errorDescription);
            });

    connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::extraTokensChanged, this,
            [this](const QVariantMap& tokens) { m_data->msaToken.extra = tokens; });

    connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::clientIdentifierChanged, this,
            [this](const QString& clientIdentifier) { m_data->msaClientID = clientIdentifier; });
}

QString MSAStep::describe()
{
    return tr("Logging in with Microsoft account.");
}

void MSAStep::perform()
{
    if (m_silent) {
        if (m_data->msaClientID != m_clientId) {
            emit finished(AccountTaskState::STATE_DISABLED,
                          tr("Microsoft user authentication failed - client identification has changed."));
            return;
        }
        if (m_data->msaToken.refresh_token.isEmpty()) {
            emit finished(AccountTaskState::STATE_DISABLED, tr("Microsoft user authentication failed - refresh token is empty."));
            return;
        }
        m_oauth2.setRefreshToken(m_data->msaToken.refresh_token);
        m_oauth2.refreshAccessToken();
    } else {
        m_oauth2.setModifyParametersFunction(
            [](QAbstractOAuth::Stage stage, QMultiMap<QString, QVariant>* map) { map->insert("prompt", "select_account"); });

        *m_data = AccountData();
        m_data->msaClientID = m_clientId;
        m_oauth2.grant();
    }
}

#include "MSAStep.moc"
