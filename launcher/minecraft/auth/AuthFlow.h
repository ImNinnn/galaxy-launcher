#pragma once

#include <QImage>
#include <QList>
#include <QObject>
#include <QSet>

#include "minecraft/auth/AccountData.h"
#include "minecraft/auth/AuthStep.h"
#include "tasks/Task.h"

class AuthFlow : public Task {
    Q_OBJECT

   public:
    // Galaxy: Profile only fetches the Minecraft profile and skin with the saved token (no sign-in).
    enum class Action { Refresh, Login, DeviceCode, Profile };

    explicit AuthFlow(AccountData* data, Action action = Action::Refresh);
    virtual ~AuthFlow() = default;

    void executeTask() override;

    AccountTaskState taskState() { return m_taskState; }

   public slots:
    bool abort() override;

   signals:
    void authorizeWithBrowser(const QUrl& url);
    void authorizeWithBrowserWithExtra(QString url, QString code, int expiresIn);
    // Galaxy: the Microsoft sign-in step finished; the remaining steps talk to Xbox and Minecraft.
    void microsoftSignedIn();

   protected:
    void succeed();
    void nextStep();

   private slots:
    // NOTE: true -> non-terminal state, false -> terminal state
    bool changeState(AccountTaskState newState, QString reason = QString());
    void stepFinished(AccountTaskState resultingState, QString message);

   private:
    AccountTaskState m_taskState = AccountTaskState::STATE_CREATED;
    QList<AuthStep::Ptr> m_steps;
    AuthStep::Ptr m_currentStep;
    AuthStep* m_oauthStep = nullptr;
    AccountData* m_data = nullptr;
};
