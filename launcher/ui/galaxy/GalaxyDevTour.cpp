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
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QNetworkReply>
#include <QTabWidget>
#include <QTimer>
#include <QTreeView>
#include <QUrlQuery>

#include <functional>
#include <memory>

#include "Application.h"
#include "BuildConfig.h"
#include "GalaxySettings.h"
#include "GalaxyView.h"
#include "MicrosoftLoginSetup.h"
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
        qInfo() << "Login test: client" << clientId << "redirect" << redirect.toString();
        if (clientId != BuildConfig.MSA_CLIENT_ID || clientId.isEmpty()) {
            *checks << "client ID";
        }
        if (redirect.scheme() != "http" || redirect.host() != "localhost" || redirect.port() <= 0) {
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
}  // namespace

void GalaxyDevTour::start(MainWindow* window, const QString& directory)
{
    QDir().mkpath(directory);
    if (qEnvironmentVariableIntValue("GALAXY_DEV_LOGIN_TEST") == 1) {
        runLoginTest(window);
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
        // Switching systems from the sidebar: out to the galaxy first, then into the other system.
        { 250,
          [galaxy, target] {
              hover(galaxy, QPointF(5, 5));
              for (int i = 0; i < APPLICATION->instances()->count(); i++) {
                  if (APPLICATION->instances()->at(i)->id() != target) {
                      galaxy->enterSystem(APPLICATION->instances()->at(i)->id());
                      break;
                  }
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
              captureBlocking(QDir(directory).filePath("24-galaxy-settings.png"),
                              [window] { APPLICATION->ShowGlobalSettings(window, "galaxy-settings"); });
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
              captureBlocking(QDir(directory).filePath("30-services-settings.png"), [window] { APPLICATION->ShowGlobalSettings(window, "apis"); });
              captureBlocking(QDir(directory).filePath("31-general-settings.png"),
                              [window] { APPLICATION->ShowGlobalSettings(window, "launcher-settings"); });
          },
          QString() },
        { 6000, [window] { window->openDiscover(); }, QString() },
        { 100,
          [directory] {
              for (auto* top : QApplication::topLevelWidgets()) {
                  if (top->isVisible() && top->objectName() == "galaxyDiscoverWindow") {
                      top->grab().save(QDir(directory).filePath("26-discover-mods.png"));
                  }
              }
          },
          QString() },
        { 6000,
          [window, target] {
              ProjectActions::openDownloadDialog(window, APPLICATION->instances()->getInstanceById(target),
                                                 ProjectActions::Category::ShaderPacks, "complementary");
          },
          QString() },
        { 100,
          [directory] {
              for (auto* top : QApplication::topLevelWidgets()) {
                  if (top->isVisible() && top->objectName() == "galaxyDiscoverWindow") {
                      top->grab().save(QDir(directory).filePath("27-discover-shaders.png"));
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
