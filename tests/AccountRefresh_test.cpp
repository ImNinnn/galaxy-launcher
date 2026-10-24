#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

#include <minecraft/auth/MinecraftAccount.h>

// Galaxy: saved sign-in tokens are reused while fresh, so a launcher restart makes no sign-in requests.
class AccountRefreshTest : public QObject {
    Q_OBJECT

    static QJsonObject savedAccount(qint64 secondsLeft, bool withToken = true)
    {
        const auto now = QDateTime::currentDateTimeUtc();
        QJsonObject ygg{ { "iat", now.addSecs(secondsLeft - 24 * 3600).toSecsSinceEpoch() },
                         { "exp", now.addSecs(secondsLeft).toSecsSinceEpoch() } };
        if (withToken) {
            ygg["token"] = "minecraft-token";
        }
        QJsonObject skin{ { "id", "" }, { "url", "" }, { "variant", "" } };
        QJsonObject profile{ { "id", "0123456789abcdef0123456789abcdef" }, { "name", "Steve" }, { "skin", skin }, { "capes", QJsonArray() } };
        return { { "type", "MSA" },
                 { "msa-client-id", "client" },
                 { "msa", QJsonObject{ { "token", "msa-token" }, { "refresh_token", "refresh" } } },
                 { "ygg", ygg },
                 { "profile", profile },
                 { "entitlement", QJsonObject{ { "ownsMinecraft", true }, { "canPlayMinecraft", true } } } };
    }

   private slots:
    void freshTokenIsReused()
    {
        auto account = MinecraftAccount::loadFromJsonV3(savedAccount(20 * 3600));
        QVERIFY(account);
        QVERIFY(!account->shouldRefresh());
        QCOMPARE(account->accountState(), AccountState::Online);
        QCOMPARE(account->accessToken(), QString("minecraft-token"));
    }

    void expiringTokenIsRefreshed()
    {
        auto account = MinecraftAccount::loadFromJsonV3(savedAccount(2 * 3600));
        QVERIFY(account);
        QVERIFY(account->shouldRefresh());
        QCOMPARE(account->accountState(), AccountState::Unchecked);
    }

    void missingTokenIsRefreshed()
    {
        auto account = MinecraftAccount::loadFromJsonV3(savedAccount(20 * 3600, false));
        QVERIFY(account);
        QVERIFY(account->shouldRefresh());
        QCOMPARE(account->accountState(), AccountState::Unchecked);
    }

    void refusedRefreshIsNotRetriedWhileTheTokenLasts()
    {
        // A refresh Minecraft refused leaves the account errored; its saved token is used until it nearly expires.
        auto lasting = MinecraftAccount::loadFromJsonV3(savedAccount(5 * 3600));
        QVERIFY(lasting);
        lasting->accountData()->accountState = AccountState::Errored;
        QVERIFY(!lasting->shouldRefresh());
        QVERIFY(lasting->tokenSecondsLeft() > 4 * 3600);

        auto expiring = MinecraftAccount::loadFromJsonV3(savedAccount(30 * 60));
        QVERIFY(expiring);
        expiring->accountData()->accountState = AccountState::Errored;
        QVERIFY(expiring->shouldRefresh());
    }
};

QTEST_GUILESS_MAIN(AccountRefreshTest)

#include "AccountRefresh_test.moc"
