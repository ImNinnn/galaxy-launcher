
#include "GetSkinStep.h"

#include <QNetworkRequest>

#include "Application.h"

GetSkinStep::GetSkinStep(AccountData* data) : AuthStep(data) {}

QString GetSkinStep::describe()
{
    return tr("Getting skin.");
}

void GetSkinStep::perform()
{
    QUrl url(m_data->minecraftProfile.skin.url);

    auto [request, response] = Net::Request::makeByteArray(url);
    m_request = request;
    // Galaxy: sign-in requests never retry after a refusal or a rate limit, so they cannot pile up on the account.
    m_request->enableAutoRetry(false);

    m_task.reset(new NetJob("GetSkinStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->setRetryRefused(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });

    m_task->start();
}

void GetSkinStep::onRequestDone(QByteArray* response)
{
    if (m_request->error() == QNetworkReply::NoError)
        m_data->minecraftProfile.skin.data = *response;
    emit finished(AccountTaskState::STATE_WORKING, tr("Got skin"));
}
