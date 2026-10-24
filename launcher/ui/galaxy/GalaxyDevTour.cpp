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

#include "GalaxyDevTour.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFrame>
#include <QLineF>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QNetworkReply>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTimer>
#include <QTreeView>
#include <QUrlQuery>

#include <functional>
#include <memory>

#include "Application.h"
#include "BuildConfig.h"
#include "DiscoverWindow.h"
#include "GalaxySettings.h"
#include "GalaxySidebar.h"
#include "GalaxyView.h"
#include "ui/dialogs/MSALoginDialog.h"
#include "MicrosoftLoginSetup.h"
#include "Notifications.h"
#include "ProfileOrganizer.h"
#include "ProjectActions.h"
#include "InstanceList.h"
#include "launch/LaunchTask.h"
#include "launch/LogModel.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/auth/AccountData.h"
#include "minecraft/auth/steps/MSAStep.h"
#include "settings/SettingsObject.h"
#include "ui/MainWindow.h"
#include "ui/pages/instance/ExternalResourcesPage.h"
#include "ui/dialogs/AboutDialog.h"
#include "ui/dialogs/ChooseOfflineNameDialog.h"
#include "ui/dialogs/ProgressDialog.h"

namespace {

struct Step {
    int delay;
    std::function<void()> action;
    QString shot;
};

void click(QWidget* widget, QPointF at)
{
    QMouseEvent press(QEvent::MouseButtonPress, at, widget->mapToGlobal(at), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, at, widget->mapToGlobal(at), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(widget, &press);
    QApplication::sendEvent(widget, &release);
}

/** Captures the dialog or menu that the action opens (they block), then dismisses it. */
void captureBlocking(const QString& file, const std::function<void()>& open)
{
    QTimer::singleShot(900, qApp, [file] {
        QWidget* shown = QApplication::activePopupWidget();
        if (!shown) {
            shown = QApplication::activeModalWidget();
        }
        if (!shown) {
            qWarning() << "Developer tour: nothing opened for" << file;
            return;
        }
        shown->grab().save(file);
        if (auto* dialog = qobject_cast<QDialog*>(shown)) {
            dialog->reject();
        } else {
            shown->close();
        }
    });
    open();
}

void hover(QWidget* widget, QPointF at)
{
    if (at.isNull()) {
        return;
    }
    QMouseEvent move(QEvent::MouseMove, at, widget->mapToGlobal(at), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(widget, &move);
}

/**
 * Answers the prompts of a launch without an account the way a user wanting the demo would. Every prompt is saved as a
 * screenshot first, so the run documents what the user would have seen.
 */
void answerPrompts(const QString& directory, int* counter)
{
    auto* modal = QApplication::activeModalWidget();
    if (!modal || qobject_cast<ProgressDialog*>(modal)) {
        return;  // progress dialogs belong to the launch itself
    }
    modal->grab().save(QDir(directory).filePath(QStringLiteral("prompt-%1.png").arg(++*counter, 2, 10, QChar('0'))));
    if (auto* box = qobject_cast<QMessageBox*>(modal)) {
        qInfo() << "Launch test: prompt" << box->windowTitle() << "-" << box->text().left(120);
        if (box->windowTitle().contains("Kill", Qt::CaseInsensitive)) {
            box->button(QMessageBox::Yes)->click();  // the test asked to stop the game
            return;
        }
        for (auto* button : box->buttons()) {
            if (button->text().contains("Demo", Qt::CaseInsensitive)) {
                button->click();
                return;
            }
        }
        if (auto* no = box->button(QMessageBox::No)) {
            no->click();  // "No Accounts": do not open the account manager, continue as demo
            return;
        }
        box->accept();
        return;
    }
    if (auto* name = qobject_cast<ChooseOfflineNameDialog*>(modal)) {
        qInfo() << "Launch test: accepting the default player name";
        name->accept();
        return;
    }
    if (auto* dialog = qobject_cast<QDialog*>(modal)) {
        qInfo() << "Launch test: closing unexpected dialog" << dialog->windowTitle();
        dialog->reject();
    }
}

void runLaunchTest(MainWindow* window, GalaxyView* galaxy, const QString& directory, const QString& target)
{
    auto* instance = APPLICATION->instances()->getInstanceById(target);
    if (!instance) {
        qWarning() << "Launch test: no profile" << target;
        window->close();
        return;
    }
    auto prompts = std::make_shared<int>(0);
    auto responder = new QTimer(window);
    QObject::connect(responder, &QTimer::timeout, window, [directory, prompts] { answerPrompts(directory, prompts.get()); });
    responder->start(700);

    // The real path: a click on the sun, or (GALAXY_DEV_LAUNCH_VIA=blackhole) on the black hole, set to launch the profile.
    if (qEnvironmentVariable("GALAXY_DEV_LAUNCH_VIA") == "blackhole") {
        galaxy->showGalaxy(false);
        const auto previous = APPLICATION->settings()->get(GalaxySettings::BlackHoleProfile);
        APPLICATION->settings()->set(GalaxySettings::BlackHoleProfile, instance->id());
        QTimer::singleShot(1200, window, [window, galaxy, directory, instance, previous] {
            qInfo() << "Launch test: clicking the black hole to launch" << instance->name();
            click(galaxy, galaxy->blackHoleScreenPosition());
            APPLICATION->settings()->set(GalaxySettings::BlackHoleProfile, previous);
            for (int i = 1; i <= 3; i++) {
                QTimer::singleShot(i * 200, window, [window, directory, i] {
                    window->grab().save(QDir(directory).filePath(QStringLiteral("15a-beam-%1.png").arg(i)));
                });
            }
        });
    } else {
        qInfo() << "Launch test: clicking the sun of" << instance->name();
        click(galaxy, galaxy->sunScreenPosition());
    }
    QTimer::singleShot(450, window, [window, directory] { window->grab().save(QDir(directory).filePath("15-launch-clicked.png")); });
    // The moment the nebula forms: project names must stay hidden while the sparkles appear.
    QObject::connect(
        instance, &BaseInstance::runningStatusChanged, window,
        [window, directory](bool running) {
            for (int i = 1; running && i <= 4; i++) {
                QTimer::singleShot(i * 250, window, [window, directory, i] {
                    window->grab().save(QDir(directory).filePath(QStringLiteral("16a-nebula-forming-%1.png").arg(i)));
                });
            }
        },
        Qt::SingleShotConnection);

    auto clock = std::make_shared<QElapsedTimer>();
    clock->start();
    auto gameStarted = std::make_shared<bool>(false);
    auto preparing = std::make_shared<bool>(false);
    auto gameLog = [instance]() {
        QStringList lines;
        if (auto* task = instance->getLaunchTask()) {
            auto log = task->getLogModel();
            for (int row = 0; row < log->rowCount(); row++) {
                lines << log->data(log->index(row), Qt::DisplayRole).toString();
            }
        }
        return lines;
    };
    auto poll = new QTimer(window);
    QObject::connect(poll, &QTimer::timeout, window, [=] {
        // Prism reports "running" from the start of launch preparation; the game itself has started once
        // Prism logs its process ID.
        if (!*gameStarted && gameLog().filter("Minecraft process ID").size() > 0) {
            *gameStarted = true;
            qInfo() << "Launch test: game process started after" << clock->elapsed() / 1000.0 << "s of preparation";
            clock->restart();
            window->grab().save(QDir(directory).filePath("16-running.png"));
            QTimer::singleShot(2500, window, [window, galaxy, directory] {
                window->grab().save(QDir(directory).filePath("16b-nebula.png"));
                // Every sparkle can be picked, even where it overlaps the sun.
                int sparkles = 0;
                int picked = 0;
                int overSun = 0;
                const QPointF sun = galaxy->sunScreenPosition();
                for (const auto& body : galaxy->currentBodies()) {
                    const QPointF at = galaxy->bodyScreenPosition(body.key);
                    if (at.isNull()) {
                        continue;
                    }
                    sparkles++;
                    if (const auto key = galaxy->bodyKeyAt(at); key == body.key) {
                        picked++;
                    } else {
                        qInfo() << "Nebula hit test:" << body.key << "at" << at << "picks" << (key.isEmpty() ? QStringLiteral("nothing") : key);
                    }
                    overSun += QLineF(at, sun).length() < 30 ? 1 : 0;
                }
                if (sparkles == 0) {
                    qInfo() << "Nebula hit test: skipped (the profile is not entered, so there are no sparkles to pick)";
                    return;
                }
                qInfo() << "Nebula hit test:" << picked << "of" << sparkles << "sparkles pick themselves," << overSun << "near the sun"
                        << (picked == sparkles ? "PASSED" : "FAILED");
                // Searching in the nebula turns the matching sparkle back into its body; clearing the search undoes it.
                const auto bodies = galaxy->currentBodies();
                const auto first = bodies.first();
                galaxy->setFilter(first.name);
                QTimer::singleShot(1000, window, [window, galaxy, directory, key = first.key] {
                    window->grab().save(QDir(directory).filePath("16c-nebula-search.png"));
                    const qreal shown = galaxy->bodyReveal(key);
                    galaxy->setFilter(QString());
                    QTimer::singleShot(1000, window, [window, galaxy, directory, key, shown] {
                        window->grab().save(QDir(directory).filePath("16d-nebula-search-cleared.png"));
                        const qreal back = galaxy->bodyReveal(key);
                        qInfo() << "Nebula search test: revealed" << shown << "then" << back
                                << (shown > 0.99 && back < 0.01 ? "PASSED" : "FAILED");
                    });
                });
            });
        }
        const bool running = instance->isRunning();
        *preparing = *preparing || running;
        // Not started: preparation ended without a game process, or nothing happened for 25 s.
        const bool timedOut = !*gameStarted && (clock->elapsed() > 1200000 || (!running && (*preparing || clock->elapsed() > 25000)));
        const bool ranLongEnough = *gameStarted && clock->elapsed() > 60000;
        const bool exitedEarly = *gameStarted && !running;
        if (!timedOut && !ranLongEnough && !exitedEarly) {
            return;
        }
        poll->stop();
        const auto lines = gameLog();
        qInfo() << "Launch test: evidence from the game log:";
        static const QStringList markers{ "Backend library", "OpenGL", "Created:", "Sound engine", "fps", "Exception", "Error", "Crash" };
        int shown = 0;
        for (const auto& line : lines) {
            if (shown < 15 && std::any_of(markers.begin(), markers.end(), [&line](const QString& m) { return line.contains(m); })) {
                qInfo().noquote() << "  |" << line.left(200);
                shown++;
            }
        }
        qInfo() << "Launch test: last game log lines:";
        for (int row = std::max<qsizetype>(0, lines.size() - 12); row < lines.size(); row++) {
            qInfo().noquote() << "  |" << lines[row].left(200);
        }
        if (timedOut) {
            qWarning() << "Launch test: FAILED, the game process never started";
            int n = 0;
            for (auto* top : QApplication::topLevelWidgets()) {
                if (top->isVisible() && top != window) {
                    qInfo() << "Launch test: window shown to the user:" << top->windowTitle();
                    top->grab().save(QDir(directory).filePath(QStringLiteral("failure-window-%1.png").arg(++n)));
                }
            }
        } else if (exitedEarly) {
            qWarning() << "Launch test: FAILED, the game exited after" << clock->elapsed() / 1000.0 << "s (see log above)";
        } else {
            qInfo() << "Launch test: PASSED, the game ran for" << clock->elapsed() / 1000.0 << "s; stopping it";
            const bool killed = APPLICATION->kill(instance);
            qInfo() << "Launch test: kill requested:" << killed;
        }
        auto finish = new QTimer(window);
        auto waited = std::make_shared<int>(0);
        QObject::connect(finish, &QTimer::timeout, window, [=] {
            if (instance->isRunning() && ++*waited < 40) {
                return;
            }
            finish->stop();
            responder->stop();
            qInfo() << "Launch test: instance running after stop:" << instance->isRunning();
            window->grab().save(QDir(directory).filePath("17a-reforming.png"));
            window->grab().save(QDir(directory).filePath("17-after-game.png"));
            if (auto* modal = QApplication::activeModalWidget()) {
                qInfo() << "Launch test: open dialog after the game:" << modal->windowTitle();
                modal->grab().save(QDir(directory).filePath("18-dialog-after-game.png"));
            }
            for (auto* top : QApplication::topLevelWidgets()) {
                if (top->isVisible() && top != window) {
                    qInfo() << "Launch test: window open after the game:" << top->windowTitle();
                }
            }
            // Prism opens the instance console after a killed game; close everything so the launcher exits.
            QTimer::singleShot(2500, window, [window, directory] {
                window->grab().save(QDir(directory).filePath("17b-reformed.png"));
                QApplication::closeAllWindows();
            });
        });
        finish->start(500);
    });
    poll->start(500);
}

}  // namespace

namespace {
/** A task that pretends to download for a second, for the progress window test. */
class DemoTask : public Task {
   protected:
    void executeTask() override
    {
        setStatus(QStringLiteral("Downloading example files"));
        setProgress(10, 100);
        QTimer::singleShot(400, this, [this] { setProgress(60, 100); });
        QTimer::singleShot(1000, this, [this] { emitSucceeded(); });
    }
};

/** Prism's progress window shows as a notification: no window opens, the call still waits and reports success. */
void runProgressWindowTest(MainWindow* window, const QString& directory)
{
    DemoTask task;
    auto seen = std::make_shared<bool>(false);
    QTimer::singleShot(500, window, [window, directory, seen] {
        window->grab().save(QDir(directory).filePath("20a-progress-notification.png"));
        const auto active = Notifications::instance()->active();
        const bool notification = std::any_of(active.begin(), active.end(), [](const Notifications::Notification& n) {
            return n.kind == Notifications::Kind::Progress && n.title == QStringLiteral("Downloading example files");
        });
        *seen = notification && !QApplication::activeModalWidget();
    });
    ProgressDialog dialog(window);
    const int result = dialog.execWithTask(&task);
    qInfo() << "Progress window test: notification instead of a window" << *seen << "result accepted" << (result == QDialog::Accepted)
            << (*seen && result == QDialog::Accepted ? "PASSED" : "FAILED");
}

/**
 * Drags (through the sidebar model's own drop handling) a profile without a category within its section and then onto
 * the first category header, checks the order and the category, and puts everything back. Needs a category.
 */
void runSidebarDropTest(QWidget* sidebar)
{
    auto* list = sidebar->findChild<QListView*>("galaxyProfileList");
    auto* model = list ? list->model() : nullptr;
    if (!model) {
        return;
    }
    auto isHeader = [model](int row) { return model->index(row, 0).data(GalaxySidebar::CategoryHeaderRole).toBool(); };
    QList<int> loose;
    int header = -1;
    for (int r = 0; r < model->rowCount(); r++) {
        if (isHeader(r)) {
            header = header < 0 ? r : header;
        } else if (header < 0) {
            loose << r;
        }
    }
    if (header < 0 || loose.size() < 3) {
        qInfo() << "Sidebar drop test: skipped (needs a category and three profiles without one before it)";
        return;
    }
    // The view must accept drops between rows, not only onto headers.
    const bool betweenRows = model->flags(QModelIndex()).testFlag(Qt::ItemIsDropEnabled);
    auto* organizer = ProfileOrganizer::instance();
    const auto orderBefore = organizer->order();
    const auto topBefore = organizer->topOrder();
    const QString first = model->index(loose[0], 0).data(InstanceList::InstanceIDRole).toString();
    const QString second = model->index(loose[1], 0).data(InstanceList::InstanceIDRole).toString();
    const QString category = model->index(header, 0).data(Qt::DisplayRole).toString();

    // Reorder: the first profile between the second and the third.
    std::unique_ptr<QMimeData> data(model->mimeData({ model->index(loose[0], 0) }));
    model->dropMimeData(data.get(), Qt::MoveAction, loose[2], 0, {});
    const bool reordered = organizer->order().indexOf(first) > organizer->order().indexOf(second);
    // Into a category: onto its header.
    model->dropMimeData(data.get(), Qt::MoveAction, -1, -1, model->index(header, 0));
    const bool moved = APPLICATION->instances()->getInstanceGroup(first) == category;
    // Out again, below everything (after the last category).
    model->dropMimeData(data.get(), Qt::MoveAction, model->rowCount(), 0, {});
    const bool outside = APPLICATION->instances()->getInstanceGroup(first).isEmpty() &&
                         organizer->topOrder().last() == ProfileOrganizer::profileToken(first);
    // A whole category to the top.
    QApplication::processEvents();  // the model rebuilds after a drop
    int headerNow = -1;
    for (int r = 0; r < model->rowCount() && headerNow < 0; r++) {
        if (isHeader(r) && model->index(r, 0).data(Qt::DisplayRole).toString() == category) {
            headerNow = r;
        }
    }
    std::unique_ptr<QMimeData> categoryData(model->mimeData({ model->index(headerNow, 0) }));
    model->dropMimeData(categoryData.get(), Qt::MoveAction, 0, 0, {});
    const bool categoryFirst = organizer->topOrder().value(0) == ProfileOrganizer::categoryToken(category);

    organizer->moveToCategory({ first }, QString());
    organizer->setOrder(orderBefore);
    organizer->setTopOrder(topBefore);
    const bool passed = betweenRows && reordered && moved && outside && categoryFirst;
    qInfo() << "Sidebar drop test: between rows" << betweenRows << "reordered" << reordered << "into" << category << moved << "out" << outside
            << "category moved" << categoryFirst << (passed ? "PASSED" : "FAILED");
}

/**
 * GALAXY_DEV_LOGIN_TEST=1: starts a real Microsoft sign-in, reads the URL the browser would open, then plays
 * Microsoft's part by sending "access denied" to the launcher's local redirect page. No browser, no account.
 */
void runLoginTest(QObject* context)
{
    auto data = std::make_shared<AccountData>();
    auto step = makeShared<MSAStep>(data.get(), false);
    auto checks = std::make_shared<QStringList>();
    QObject::connect(step.get(), &MSAStep::authorizeWithBrowser, context, [context, checks](const QUrl& url) {
        const QUrlQuery query(url);
        const QUrl redirect(query.queryItemValue("redirect_uri", QUrl::FullyDecoded));
        const auto clientId = query.queryItemValue("client_id");
        qInfo() << "Login test: client" << clientId << "redirect" << redirect.toString() << "override"
                << APPLICATION->settings()->get("MSAClientIDOverride").toString() << "built in" << BuildConfig.MSA_CLIENT_ID;
        if (clientId != APPLICATION->getMSAClientID() || clientId.isEmpty()) {
            *checks << "client ID";
        }
        // The built-in application is registered for localhost; an override uses Prism Launcher's 127.0.0.1.
        const QString host = clientId == BuildConfig.MSA_CLIENT_ID ? "localhost" : "127.0.0.1";
        if (redirect.scheme() != "http" || redirect.host() != host || redirect.port() <= 0) {
            *checks << "redirect URI";
        }
        QUrl back(redirect);
        QUrlQuery result;
        result.addQueryItem("error", "access_denied");
        result.addQueryItem("error_description", "Login test");
        result.addQueryItem("state", query.queryItemValue("state"));
        back.setQuery(result);
        auto* reply = APPLICATION->network()->get(QNetworkRequest(back));
        QObject::connect(reply, &QNetworkReply::finished, context, [reply, checks] {
            reply->deleteLater();
            if (!reply->readAll().contains("Galaxy Launcher sign-in")) {
                *checks << "local page (" + reply->errorString() + ")";
            }
        });
    });
    QObject::connect(step.get(), &AuthStep::finished, context, [step, data, checks](AccountTaskState state, QString message) {
        // A refused sign-in must end the flow as a failure, after the page was served.
        QTimer::singleShot(500, [state, message, checks] {
            const bool failed = state == AccountTaskState::STATE_FAILED_HARD;
            qInfo() << "Login test: the flow ended with" << message << (checks->isEmpty() && failed ? "PASSED" : "FAILED")
                    << *checks;
        });
    });
    step->perform();
}

/**
 * GALAXY_DEV_LOGIN_TEST=2: opens the sign-in dialog, checks the device code is polled while it is open, then closes
 * it and checks the polling stops. No browser, no account.
 */
void runLoginDialogTest(QObject* context)
{
    auto polls = std::make_shared<int>(0);
    QObject::connect(APPLICATION->network(), &QNetworkAccessManager::finished, context, [polls](QNetworkReply* reply) {
        if (reply->url().path().endsWith("/oauth2/v2.0/token")) {
            ++*polls;
        }
    });
    QTimer::singleShot(0, context, [] { MSALoginDialog::newAccount(nullptr); });
    QTimer::singleShot(20000, context, [context, polls] {
        const int whileOpen = *polls;
        for (auto* widget : QApplication::topLevelWidgets()) {
            if (auto* dialog = qobject_cast<MSALoginDialog*>(widget)) {
                dialog->reject();
            }
        }
        QTimer::singleShot(15000, context, [polls, whileOpen] {
            const int afterClose = *polls - whileOpen;
            qInfo() << "Login dialog test: polled" << whileOpen << "times while open," << afterClose << "after closing"
                    << (whileOpen > 0 && afterClose == 0 ? "PASSED" : "FAILED");
        });
    });
}
}  // namespace

void GalaxyDevTour::start(MainWindow* window, const QString& directory)
{
    QDir().mkpath(directory);
    if (qEnvironmentVariableIntValue("GALAXY_DEV_LOGIN_TEST") == 1) {
        runLoginTest(window);
    } else if (qEnvironmentVariableIntValue("GALAXY_DEV_LOGIN_TEST") == 2) {
        runLoginDialogTest(window);
    }
    if (const auto size = qEnvironmentVariable("GALAXY_DEV_WINDOW_SIZE").split('x'); size.size() == 2) {
        window->resize(size[0].toInt(), size[1].toInt());
    }
    auto* galaxy = window->m_galaxy;
    auto bodyKey = [galaxy](CelestialBody::Kind kind) {
        for (const auto& body : galaxy->currentBodies()) {
            if (body.kind == kind) {
                return body.key;
            }
        }
        return QString();
    };
    const auto firstId = APPLICATION->instances()->count() > 0 ? APPLICATION->instances()->at(0)->id() : QString();
    QString target = qEnvironmentVariable("GALAXY_DEV_PROFILE");
    if (target.isEmpty()) {
        target = firstId;  // dev.sh passes the variable even when it is unset
    }
    const bool launchTest = qEnvironmentVariableIntValue("GALAXY_DEV_LAUNCH") == 1;

    auto before = std::make_shared<QHash<QString, QPointF>>();
    auto steps = std::make_shared<QList<Step>>(QList<Step>{
        { 0, [] {}, "01-galaxy" },
        { 1500,
          [galaxy, target, before] {
              hover(galaxy, galaxy->systemScreenPosition(target));
              before->clear();
              for (int i = 0; i < APPLICATION->instances()->count(); i++) {
                  const auto id = APPLICATION->instances()->at(i)->id();
                  before->insert(id, galaxy->systemScreenPosition(id));
              }
          },
          QString() },
        { 300,
          [galaxy, target, before] {
              // Only the hovered profile stays still; the rest of the galaxy keeps turning.
              qreal hovered = 0;
              qreal others = 0;
              for (auto it = before->constBegin(); it != before->constEnd(); ++it) {
                  const qreal moved = QLineF(it.value(), galaxy->systemScreenPosition(it.key())).length();
                  if (it.key() == target) {
                      hovered = moved;
                  } else {
                      others = std::max(others, moved);
                  }
              }
              qInfo() << "Hover test: hovered profile moved" << hovered << "px, others up to" << others << "px"
                      << (hovered < 0.5 && others > 1.0 ? "PASSED" : "FAILED");
          },
          "02-galaxy-hover" },
        { 100, [galaxy, target] { click(galaxy, galaxy->systemScreenPosition(target)); }, QString() },
        { 450, [] {}, "03-entering" },
        { 1400, [] {}, "04-system" },
        { 0,
          [galaxy] {
              // Rough paint cost of the current system at full quality.
              QElapsedTimer timer;
              timer.start();
              for (int i = 0; i < 30; i++) {
                  galaxy->repaint();
              }
              qInfo() << "Galaxy paint:" << timer.elapsed() / 30.0 << "ms per frame at" << galaxy->size();
          },
          QString() },
        { 3600,
          [galaxy, target] {
              // A project added: every planet gets a new orbit and must glide there (it used to jerk). A file sorting
              // first appears in the mods folder; the planets' speed on screen is measured while they move.
              auto* instance = dynamic_cast<MinecraftInstance*>(APPLICATION->instances()->getInstanceById(target));
              if (!instance) {
                  return;
              }
              const QString path = QDir(instance->gameRoot() + "/mods").filePath("aaa-galaxy-tour-test.jar");
              QFile file(path);
              if (!file.open(QIODevice::WriteOnly)) {
                  return;
              }
              file.write(QByteArray(1536 * 1024, 'x'));
              file.close();
              QStringList keys;
              for (const auto& body : galaxy->currentBodies()) {
                  if (body.kind == CelestialBody::Kind::Mod) {
                      keys << body.key;
                  }
              }
              if (keys.isEmpty()) {
                  QFile::remove(path);
                  qInfo() << "Relayout smoothness test: skipped (the profile has no mods)";
                  return;
              }
              // Smooth means no sudden change of speed: the largest change of a planet's velocity from one frame to the
              // next (a planet that jumps into motion changes it by thousands of pixels per second at once).
              auto last = std::make_shared<QHash<QString, QPointF>>();
              auto velocity = std::make_shared<QHash<QString, QPointF>>();
              auto jolt = std::make_shared<qreal>(0);
              auto clock = std::make_shared<QElapsedTimer>();
              clock->start();
              auto previous = std::make_shared<qint64>(0);
              for (int k = 1; k <= 90; k++) {
                  QTimer::singleShot(k * 33, galaxy, [galaxy, keys, last, velocity, jolt, clock, previous, path, k] {
                      const qint64 at = clock->elapsed();
                      const qreal dt = std::max<qint64>(1, at - *previous) / 1000.0;
                      *previous = at;
                      for (const auto& key : keys) {
                          const QPointF now = galaxy->bodyScreenPosition(key);
                          if (!now.isNull() && last->contains(key)) {
                              const QPointF v = (now - last->value(key)) / dt;
                              if (velocity->contains(key)) {
                                  *jolt = std::max(*jolt, QLineF(velocity->value(key), v).length());
                              }
                              velocity->insert(key, v);
                          }
                          last->insert(key, now);
                      }
                      if (k == 90) {
                          QFile::remove(path);
                          qInfo() << "Relayout smoothness test: largest change of a planet's speed between frames" << *jolt
                                  << "px/s after a project was added" << (*jolt < 1500 ? "PASSED" : "FAILED");
                      }
                  });
              }
          },
          QString() },
        { 100,
          [window, galaxy] {
              // Focus another window, then come back: animations must resume.
              auto* other = new QWidget();
              other->setAttribute(Qt::WA_DeleteOnClose);
              other->resize(200, 100);
              other->show();
              other->activateWindow();
              QTimer::singleShot(1500, window, [window, galaxy, other] {
                  qInfo() << "Refocus test: another window active, launcher active:" << window->isActiveWindow()
                          << "animating:" << galaxy->isAnimating();
                  other->close();
                  window->activateWindow();
                  QTimer::singleShot(1500, window, [window, galaxy] {
                      qInfo() << "Refocus test: back in the launcher, active:" << window->isActiveWindow()
                              << "animating:" << galaxy->isAnimating() << (galaxy->isAnimating() ? "PASSED" : "FAILED");
                  });
              });
          },
          QString() },
        { 3200, [] {}, QString() },
        { 200, [galaxy, bodyKey] { hover(galaxy, galaxy->bodyScreenPosition(bodyKey(CelestialBody::Kind::Mod))); }, "05-planet-hover" },
        { 200, [galaxy, bodyKey] { hover(galaxy, galaxy->bodyScreenPosition(bodyKey(CelestialBody::Kind::ResourcePack))); },
          "06-moon-hover" },
        { 200, [galaxy, bodyKey] { hover(galaxy, galaxy->bodyScreenPosition(bodyKey(CelestialBody::Kind::ShaderPack))); },
          "07-ring-hover" },
        { 200, [galaxy] { hover(galaxy, galaxy->sunScreenPosition()); }, "08-sun-hover" },
        { 4500,
          [galaxy, bodyKey] {
              // A hovered planet stops; once the cursor leaves it catches up until it is back in its place.
              const auto key = bodyKey(CelestialBody::Kind::Mod);
              if (key.isEmpty()) {
                  qInfo() << "Catch-up test: skipped (the profile has no mods)";
                  return;
              }
              hover(galaxy, galaxy->bodyScreenPosition(key));
              QTimer::singleShot(1500, galaxy, [galaxy, key] {
                  const qreal behind = galaxy->bodyLag(key);
                  hover(galaxy, QPointF(5, 5));
                  QTimer::singleShot(2800, galaxy, [galaxy, key, behind] {
                      const qreal after = galaxy->bodyLag(key);
                      qInfo() << "Catch-up test: behind" << behind << "s while hovered," << after << "s after leaving"
                              << (behind > 0.5 && after < 0.01 ? "PASSED" : "FAILED");
                  });
              });
          },
          QString() },
        // Switching systems from the sidebar: a direct flight to the other system, never out to the whole galaxy.
        { 250,
          [galaxy, target] {
              hover(galaxy, QPointF(5, 5));
              for (int i = 0; i < APPLICATION->instances()->count(); i++) {
                  if (APPLICATION->instances()->at(i)->id() != target) {
                      galaxy->enterSystem(APPLICATION->instances()->at(i)->id());
                      break;
                  }
              }
              auto closest = std::make_shared<qreal>(1e9);
              auto inSystem = std::make_shared<bool>(true);
              const qreal start = galaxy->zoomOverGalaxy();
              for (int k = 1; k <= 15; k++) {
                  QTimer::singleShot(k * 100, galaxy, [galaxy, closest, inSystem, start, k] {
                      *closest = std::min(*closest, galaxy->zoomOverGalaxy() / start);
                      *inSystem = *inSystem && !galaxy->currentSystemId().isEmpty();
                      if (k == 15) {
                          // At the systems' scale all the way: at most a slight zoom out, never towards the galaxy view.
                          qInfo() << "Switch flight test: zoom never below" << *closest << "of the system's, always in a system" << *inSystem
                                  << (*closest > 0.6 && *inSystem ? "PASSED" : "FAILED");
                      }
                  });
              }
          },
          "08a-switch-leaving" },
        { 550, [] {}, "08b-switch-galaxy" },
        { 1300, [] {}, "08c-switch-entered" },
        { 1600, [galaxy, target] { galaxy->enterSystem(target); }, QString() },
        { 100,
          [window, galaxy, directory, target] {
              auto* instance = APPLICATION->instances()->getInstanceById(target);
              for (const auto& body : galaxy->currentBodies()) {
                  if (body.kind == CelestialBody::Kind::Mod) {
                      const QPoint at = galaxy->mapToGlobal(galaxy->bodyScreenPosition(body.key)).toPoint();
                      captureBlocking(QDir(directory).filePath("20-planet-menu.png"),
                                      [window, instance, body, at] { ProjectActions::showContextMenu(window, instance, body, at); });
                      captureBlocking(QDir(directory).filePath("21-details.png"),
                                      [window, instance, body] { ProjectActions::showDetails(window, instance, body); });
                      break;
                  }
              }
              const QPoint sun = galaxy->mapToGlobal(galaxy->sunScreenPosition()).toPoint();
              captureBlocking(QDir(directory).filePath("22-profile-menu.png"), [window, target, sun] { window->showProfileMenu(target, sun); });
              captureBlocking(QDir(directory).filePath("23-sun-color.png"), [window] { window->pickSunColor(); });
              // Settings are a page of the main window now.
              auto settingsShot = [window, directory](const QString& page, const QString& file) {
                  APPLICATION->ShowGlobalSettings(window, page);
                  QApplication::processEvents();
                  window->grab().save(QDir(directory).filePath(file));
                  const bool embedded = window->m_settingsPanel && window->m_stack->currentWidget() == window->m_settingsPanel;
                  const bool closed = window->closeSettingsPanel() && !window->panelShown();
                  qInfo() << "Settings page test:" << page << "embedded" << embedded << "closed" << closed
                          << (embedded && closed ? "PASSED" : "FAILED");
              };
              settingsShot("galaxy-settings", "24-galaxy-settings.png");
              captureBlocking(QDir(directory).filePath("25-microsoft-setup.png"), [window] { MicrosoftLoginSetup::ensureClientId(window); });
              captureBlocking(QDir(directory).filePath("28-help-menu.png"), [window] {
                  if (auto* help = window->findChild<QMenu*>("helpMenu")) {
                      help->exec(window->mapToGlobal(QPoint(300, 60)));
                  }
              });
              captureBlocking(QDir(directory).filePath("29-about-credits.png"), [window] {
                  AboutDialog about(window);
                  if (auto* tabs = about.findChild<QTabWidget*>()) {
                      for (int i = 0; i < tabs->count(); i++) {
                          if (tabs->tabText(i).contains("Credits")) {
                              tabs->setCurrentIndex(i);
                          }
                      }
                  }
                  about.exec();
              });
              settingsShot("apis", "30-services-settings.png");
              // Shown when GitHub has a newer release (GALAXY_DEV_FAKE_VERSION pretends this one is old).
              if (window->m_launcherUpdateButton && window->m_launcherUpdateButton->isVisible()) {
                  window->grab().save(QDir(directory).filePath("34-launcher-update-button.png"));
                  captureBlocking(QDir(directory).filePath("35-launcher-update.png"), [window] { window->m_launcherUpdateAction->trigger(); });
              }
              settingsShot("launcher-settings", "31-general-settings.png");

              // The profile window (with its logs) is a page of the main window too.
              if (auto* instance = APPLICATION->instances()->getInstanceById(target)) {
                  APPLICATION->showInstanceWindow(instance, "console");
                  QApplication::processEvents();
                  window->grab().save(QDir(directory).filePath("36-profile-window.png"));
                  const bool embedded = window->m_instancePanels && window->m_stack->currentWidget() == window->m_instancePanels;
                  const bool closed = window->leavePanels() && !window->panelShown();
                  qInfo() << "Profile window test: embedded" << embedded << "closed" << closed << (embedded && closed ? "PASSED" : "FAILED");
              }
          },
          QString() },
        { 6000, [window] { window->openDiscover(); }, QString() },
        { 100,
          [window, directory] {
              // Discover splits the main window: the galaxy and Discover side by side.
              auto* discover = window->findChild<QWidget*>("galaxyDiscoverWindow");
              qInfo() << "Discover split test:" << (discover && discover->isVisible() && !discover->isWindow() ? "PASSED" : "FAILED");
              window->grab().save(QDir(directory).filePath("26-discover-mods.png"));

              // Opening settings closes Discover; the settings button again closes settings; Add content toggles Discover.
              window->on_actionSettings_triggered();
              QApplication::processEvents();
              const bool discoverClosed = !DiscoverWindow::isOpenBeside();
              const bool settingsShown = window->panelShown();
              window->on_actionSettings_triggered();
              const bool settingsToggled = !window->panelShown();
              window->openDiscover();
              QApplication::processEvents();
              const bool discoverOpened = DiscoverWindow::isOpenBeside();
              window->openDiscover();
              QApplication::processEvents();
              const bool discoverToggled = !DiscoverWindow::isOpenBeside();
              const bool passed = discoverClosed && settingsShown && settingsToggled && discoverOpened && discoverToggled;
              qInfo() << "Panel toggle test: Discover closed by settings" << discoverClosed << "settings toggled" << settingsShown << settingsToggled
                      << "Discover toggled" << discoverOpened << discoverToggled << (passed ? "PASSED" : "FAILED");
          },
          QString() },
        { 6000,
          [window, target] {
              ProjectActions::openDownloadDialog(window, APPLICATION->instances()->getInstanceById(target),
                                                 ProjectActions::Category::ShaderPacks, "complementary");
          },
          QString() },
        { 100,
          [window, directory] {
              for (auto* top : window->findChildren<QWidget*>("galaxyDiscoverWindow")) {
                  if (top->isVisible()) {
                      window->grab().save(QDir(directory).filePath("27-discover-shaders.png"));
                      // The filter options, as the shader page has them (Minecraft version, shader loaders, categories).
                      for (auto* button : top->findChildren<QPushButton*>("resourceFilterButton")) {
                          if (button->isVisible()) {
                              button->click();
                              QApplication::processEvents();
                              window->grab().save(QDir(directory).filePath("27b-discover-shader-filters.png"));
                              break;
                          }
                      }
                      qInfo() << "Discover window closes:" << (top->close() ? "PASSED" : "FAILED");
                  }
              }
          },
          QString() },
        { 100,
          [galaxy, target] {
              hover(galaxy, QPointF(5, 5));
              galaxy->playLaunchAnimation(target);
          },
          QString() },
        { 550, [] {}, "09-launch-pull" },
        { 450, [] {}, "10-launch-flash" },
        { 900, [] {}, "11-launch-restored" },
        { 100, [galaxy] { galaxy->setFilter("sodium"); }, "12-filter" },
        { 100,
          [window, galaxy] {
              galaxy->setFilter(QString());
              window->setListView(true, false);
          },
          QString() },
        { 600, [] {}, "13-list-view" },
        { 100,
          [window, directory, target] {
              // The list view's right-click menu (the galaxy's) and the profile's updates.
              for (auto* page : window->findChildren<ExternalResourcesPage*>()) {
                  auto* tree = page->isVisible() ? page->findChild<QTreeView*>() : nullptr;
                  if (!tree || !tree->model() || tree->model()->rowCount() == 0) {
                      continue;
                  }
                  const auto first = tree->model()->index(0, 0);
                  tree->setCurrentIndex(first);
                  const QPoint at = tree->visualRect(first).center();
                  captureBlocking(QDir(directory).filePath("32-list-context-menu.png"), [page, at] {
                      QMetaObject::invokeMethod(page, "showContextMenu", Qt::DirectConnection, Q_ARG(QPoint, at));
                  });
                  break;
              }
              if (auto* instance = dynamic_cast<MinecraftInstance*>(APPLICATION->instances()->getInstanceById(target))) {
                  captureBlocking(QDir(directory).filePath("33-updates-dialog.png"), [window, instance] { ProjectActions::updateAll(window, instance); });
                  if (qEnvironmentVariableIntValue("GALAXY_DEV_APPLY_UPDATES") == 1) {
                      // Really install everything: accept the dialog, then list the mods folder.
                      const QDir mods(instance->gameRoot() + "/mods");
                      qInfo() << "Update test: mods before" << mods.entryList(QDir::Files);
                      QTimer::singleShot(900, window, [] {
                          if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
                              dialog->accept();
                          }
                      });
                      ProjectActions::updateAll(window, instance);
                      qInfo() << "Update test: mods after" << QDir(instance->gameRoot() + "/mods").entryList(QDir::Files);
                  }
              }
          },
          QString() },
        { 100,
          [window, galaxy, launchTest] {
              window->setListView(false, false);
              if (!launchTest) {
                  galaxy->showGalaxy();
              }
          },
          QString() },
        { 1300, [] {}, "14-back" },
        { 100, [window] { window->m_sidebar->setCollapsed(true); }, QString() },
        { 400, [] {}, "15-sidebar-collapsed" },
        { 100, [window] { window->m_sidebar->setCollapsed(false); }, QString() },
        { 300,
          [window, galaxy, directory, target] {
              captureBlocking(QDir(directory).filePath("16-new-category.png"),
                              [target] { ProfileOrganizer::instance()->createCategory(nullptr, target); });
              // The galaxy's menu on empty space (top left corner).
              captureBlocking(QDir(directory).filePath("17-galaxy-menu.png"), [galaxy] {
                  const QPointF at(6, 6);
                  QMouseEvent press(QEvent::MouseButtonPress, at, galaxy->mapToGlobal(at), Qt::RightButton, Qt::RightButton, Qt::NoModifier);
                  QMouseEvent release(QEvent::MouseButtonRelease, at, galaxy->mapToGlobal(at), Qt::RightButton, Qt::NoButton, Qt::NoModifier);
                  QApplication::sendEvent(galaxy, &press);
                  QApplication::sendEvent(galaxy, &release);
              });
              runSidebarDropTest(window->m_sidebar);
          },
          QString() },
        { 400, [] {}, "18-after-drop-test" },
        { 1500, [window, directory] { runProgressWindowTest(window, directory); }, QString() },
        { 0,
          [] {
              // What notifications look like (they are removed again right after the snapshot).
              auto* notifications = Notifications::instance();
              Notifications::Notification progress;
              progress.kind = Notifications::Kind::Progress;
              progress.title = QStringLiteral("Launching Fabric Adventure");
              progress.text = QStringLiteral("Downloading assets");
              progress.progress = 42;
              progress.actions = { { QStringLiteral("Skip"), [] {} } };
              notifications->post(progress);
              notifications->post(Notifications::Kind::Success, QStringLiteral("Installed 3 projects into Fabric Adventure"), QString());
              notifications->post(Notifications::Kind::Error, QStringLiteral("Big Modpack crashed"),
                                  QStringLiteral("Minecraft closed unexpectedly. The log shows what happened."), { { QStringLiteral("Show log"), [] {} } });
          },
          QString() },
        { 100,
          [window] {
              // Removing a profile's icon: the profile menu offers it for profiles with an icon of their own.
              for (int i = 0; i < APPLICATION->instances()->count(); i++) {
                  auto* instance = APPLICATION->instances()->at(i);
                  if (instance->iconKey() == QStringLiteral("default")) {
                      continue;
                  }
                  const auto before = instance->iconKey();
                  window->setSelectedInstanceById(instance->id());
                  QMenu menu;
                  window->populateProfileMenu(&menu);
                  QAction* remove = nullptr;
                  for (auto* action : menu.actions()) {
                      if (action->text() == QObject::tr("Remove Icon")) {
                          remove = action;
                      }
                  }
                  if (remove) {
                      remove->trigger();
                  }
                  const bool removed = instance->iconKey() == QStringLiteral("default");
                  instance->setIconKey(before);
                  qInfo() << "Remove icon test:" << instance->name() << "offered" << (remove != nullptr) << "removed" << removed
                          << (remove && removed ? "PASSED" : "FAILED");
                  break;
              }
          },
          QString() },
        { 120, [] {}, "19a-notifications-sliding-in" },
        { 500,
          [window] {
              // A card whose text and buttons change after it is shown must grow to fit them (they were cut off).
              auto* notifications = Notifications::instance();
              Notifications::Notification progress;
              progress.kind = Notifications::Kind::Progress;
              progress.title = QStringLiteral("Launching 1.8 hypixel");
              progress.text = QStringLiteral("Getting ready…");
              const int id = notifications->post(progress);
              progress.text = QStringLiteral("Encountered an error during authentication, and this line is long enough to wrap");
              progress.actions = { { QStringLiteral("Cancel"), [] {} } };
              notifications->replace(id, progress);
              QTimer::singleShot(400, window, [window, id] {
                  bool fits = true;
                  int cards = 0;
                  for (auto* card : window->findChildren<QFrame*>("galaxyNotification")) {
                      if (card->isVisible()) {
                          cards++;
                          fits = fits && card->height() >= card->heightForWidth(card->width());
                      }
                  }
                  qInfo() << "Notification size test:" << cards << "cards fit their content" << fits << (fits && cards > 0 ? "PASSED" : "FAILED");
                  Notifications::instance()->dismiss(id);
              });
          },
          "19-notifications" },
        { 120,
          [] {
              for (const auto& notification : Notifications::instance()->active()) {
                  Notifications::instance()->dismiss(notification.id);
              }
          },
          "19b-notifications-sliding-out" },
    });
    auto index = std::make_shared<int>(0);
    auto runner = std::make_shared<std::function<void()>>();
    *runner = [window, galaxy, steps, index, runner, directory, target, launchTest] {
        if (*index >= steps->size()) {
            qInfo() << "Developer snapshots written to" << directory;
            if (launchTest) {
                runLaunchTest(window, galaxy, directory, target);
            } else {
                QTimer::singleShot(200, window, &QWidget::close);
            }
            return;
        }
        const auto step = steps->at((*index)++);
        step.action();
        QTimer::singleShot(std::max(step.delay, 16), window, [window, step, runner, directory] {
            if (!step.shot.isEmpty()) {
                window->grab().save(QDir(directory).filePath(step.shot + ".png"));
            }
            (*runner)();
        });
    };
    (*runner)();
}
