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

#include "LauncherUpdate.h"

#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "Application.h"
#include "BuildConfig.h"
#include "DesktopServices.h"
#include "GalaxySettings.h"
#include "Markdown.h"
#include "StringUtils.h"
#include "Version.h"
#include "settings/SettingsObject.h"

namespace {
QString withoutPrefix(QString tag)
{
    if (tag.startsWith('v', Qt::CaseInsensitive)) {
        tag.remove(0, 1);
    }
    return tag;
}
}  // namespace

LauncherUpdate::LauncherUpdate(QObject* parent) : QObject(parent) {}

QString LauncherUpdate::currentVersion()
{
    const auto fake = qEnvironmentVariable("GALAXY_DEV_FAKE_VERSION");  // developer tooling
    return fake.isEmpty() ? BuildConfig.GALAXY_VERSION : fake;
}

void LauncherUpdate::check()
{
    if (BuildConfig.GALAXY_RELEASES_REPO.isEmpty() || !APPLICATION->settings()->get(GalaxySettings::LauncherUpdates).toBool()) {
        return;
    }
    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/%1/releases?per_page=10").arg(BuildConfig.GALAXY_RELEASES_REPO)));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("GalaxyLauncher/%1").arg(currentVersion()));
    auto* reply = APPLICATION->network()->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Galaxy Launcher update check failed:" << reply->errorString();
            return;
        }
        // Newest first; the first published one is what users would get.
        for (const auto& value : QJsonDocument::fromJson(reply->readAll()).array()) {
            const auto release = value.toObject();
            if (release.value("draft").toBool()) {
                continue;
            }
            Release found;
            found.version = withoutPrefix(release.value("tag_name").toString());
            found.name = release.value("name").toString();
            found.notes = release.value("body").toString();
            found.page = QUrl(release.value("html_url").toString());
            for (const auto& asset : release.value("assets").toArray()) {
                if (asset.toObject().value("name").toString() == "GalaxyLauncher-Setup.exe") {
                    found.installer = QUrl(asset.toObject().value("browser_download_url").toString());
                }
            }
            if (!found.version.isEmpty() && Version(found.version) > Version(currentVersion())) {
                qInfo() << "Galaxy Launcher" << found.version << "is available (this is" << currentVersion() << ")";
                emit available(found);
            }
            return;
        }
    });
}

void LauncherUpdate::show(QWidget* parent, const Release& release)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Galaxy Launcher update"));
    dialog.resize(560, 480);
    auto* layout = new QVBoxLayout(&dialog);

    auto* title = new QLabel(QObject::tr("<b>Galaxy Launcher %1 is available.</b> You have %2.").arg(release.version, currentVersion()), &dialog);
    layout->addWidget(title);

    auto* notes = new QTextBrowser(&dialog);
    notes->setOpenExternalLinks(true);
    notes->setHtml(release.notes.trimmed().isEmpty() ? QObject::tr("No release notes were published.")
                                                     : StringUtils::htmlListPatch(markdownToHTML(release.notes)));
    layout->addWidget(notes, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto* page = buttons->addButton(QObject::tr("Release page"), QDialogButtonBox::ActionRole);
    QObject::connect(page, &QPushButton::clicked, &dialog, [&release] { DesktopServices::openUrl(release.page); });

#ifdef Q_OS_WIN
    auto* how = new QLabel(QObject::tr("Download the new installer and run it. Your profiles, accounts and settings are kept."), &dialog);
    how->setWordWrap(true);
    layout->addWidget(how);
    auto* download = buttons->addButton(QObject::tr("Download the installer"), QDialogButtonBox::AcceptRole);
    download->setDefault(true);
    QObject::connect(download, &QPushButton::clicked, &dialog, [&release] {
        DesktopServices::openUrl(release.installer.isValid() ? release.installer : release.page);
    });
#else
    // Linux: the Flatpak cannot update itself; the installer script updates it in place.
    const QString command = QStringLiteral("bash -c \"$(curl -fsSL https://raw.githubusercontent.com/%1/main/install-linux.sh)\"")
                                .arg(BuildConfig.GALAXY_RELEASES_REPO);
    auto* how = new QLabel(QObject::tr("To update, close Galaxy Launcher and run this in a terminal. Your profiles, accounts and settings "
                                       "are kept."),
                           &dialog);
    how->setWordWrap(true);
    layout->addWidget(how);
    auto* row = new QHBoxLayout();
    auto* field = new QLineEdit(command, &dialog);
    field->setReadOnly(true);
    field->setCursorPosition(0);
    auto* copy = new QPushButton(QObject::tr("Copy"), &dialog);
    QObject::connect(copy, &QPushButton::clicked, &dialog, [command, copy] {
        QGuiApplication::clipboard()->setText(command);
        copy->setText(QObject::tr("Copied"));
    });
    row->addWidget(field, 1);
    row->addWidget(copy);
    layout->addLayout(row);
    page->setDefault(true);
#endif

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}
