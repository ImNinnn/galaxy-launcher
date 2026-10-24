#include "EntitlementsStep.h"

#include <QList>
#include <QNetworkRequest>
#include <QUrl>
#include <QUuid>
#include <memory>

#include "Application.h"
#include "Logging.h"
#include "minecraft/auth/Parsers.h"
#include "net/Request.h"
#include "net/NetJob.h"
#include "net/RawHeaderProxy.h"
#include "tasks/Task.h"

EntitlementsStep::EntitlementsStep(AccountData* data) : AuthStep(data) {}

QString EntitlementsStep::describe()
{
    return tr("Determining game ownership.");
}

void EntitlementsStep::perform()
{
    m_entitlements_request_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QUrl url("https://api.minecraftservices.com/entitlements/license?requestId=" + m_entitlements_request_id);
    auto headers = QList<Net::HeaderPair>{ { "Content-Type", "application/json" },
                                           { "Accept", "application/json" },
                                           { "Authorization", QString("Bearer %1").arg(m_data->yggdrasilToken.token).toUtf8() } };

    auto [request, response] = Net::Request::makeByteArray(url);
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    // Galaxy: sign-in requests never retry after a refusal or a rate limit, so they cannot pile up on the account.
    m_request->enableAutoRetry(false);

    m_task.reset(new NetJob("EntitlementsStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->setRetryRefused(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });

    m_task->start();
    qDebug() << "Getting entitlements...";
}

void EntitlementsStep::onRequestDone(QByteArray* response)
{
    qCDebug(authCredentials()) << *response;

    // TODO: check presence of same entitlementsRequestId?
    // TODO: validate JWTs?
    Parsers::parseMinecraftEntitlements(*response, m_data->minecraftEntitlement);

    emit finished(AccountTaskState::STATE_WORKING, tr("Got entitlements"));
}
