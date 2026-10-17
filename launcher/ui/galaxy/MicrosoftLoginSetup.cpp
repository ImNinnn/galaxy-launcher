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

#include "MicrosoftLoginSetup.h"

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QUrl>
#include <QVBoxLayout>

#include "Application.h"
#include "DesktopServices.h"
#include "settings/SettingsObject.h"

namespace MicrosoftLoginSetup {

namespace {

QString tr(const char* text)
{
    return QCoreApplication::translate("MicrosoftLoginSetup", text);
}

// Same format check as Settings > APIs.
const QRegularExpression& clientIdPattern()
{
    static const QRegularExpression pattern(
        QRegularExpression::anchoredPattern("[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}"));
    return pattern;
}

}  // namespace

bool ensureClientId(QWidget* parent)
{
    if (APPLICATION->capabilities() & Application::SupportsMSA) {
        return true;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(tr("Set up Microsoft sign-in"));
    dialog.setMinimumWidth(560);
    auto* layout = new QVBoxLayout(&dialog);

    auto* text = new QLabel(&dialog);
    text->setWordWrap(true);
    text->setTextFormat(Qt::RichText);
    text->setOpenExternalLinks(false);
    text->setText(
        tr("<p>Minecraft accounts sign in through Microsoft, and every launcher needs its own <b>Microsoft application "
           "(client) ID</b> for that. Galaxy Launcher does not include one: Prism Launcher, which it is built on, asks "
           "rebranded launchers not to reuse its ID.</p>"
           "<p>To sign in:</p>"
           "<ol>"
           "<li>In <a href=\"https://portal.azure.com/#view/Microsoft_AAD_RegisteredApps/ApplicationsListBlade\">Microsoft Entra "
           "app registrations</a>, register a new application. Supported account types: <i>Personal Microsoft accounts "
           "only</i>. Under <i>Authentication</i>, turn on <i>Allow public client flows</i>.</li>"
           "<li>New applications must be allowed by Mojang to access Minecraft accounts: request this with the "
           "<a href=\"https://aka.ms/mce-reviewappid\">Minecraft API access form</a>. Approval can take some time; "
           "until then Microsoft sign-in fails.</li>"
           "<li>Paste the application's <i>Application (client) ID</i> below.</li>"
           "</ol>"
           "<p>You can change it later in Settings › APIs.</p>"));
    QObject::connect(text, &QLabel::linkActivated, text, [](const QString& link) { DesktopServices::openUrl(QUrl(link)); });
    layout->addWidget(text);

    auto* field = new QLineEdit(&dialog);
    field->setPlaceholderText(QStringLiteral("00000000-0000-4000-8000-000000000000"));
    field->setAccessibleName(tr("Application (client) ID"));
    layout->addWidget(field);

    auto* problem = new QLabel(&dialog);
    problem->setObjectName("galaxyHint");
    layout->addWidget(problem);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    auto* ok = buttons->button(QDialogButtonBox::Ok);
    ok->setText(tr("Save and sign in"));
    ok->setEnabled(false);
    layout->addWidget(buttons);

    auto normalized = [field] { return field->text().trimmed().toLower(); };
    QObject::connect(field, &QLineEdit::textChanged, &dialog, [=] {
        const bool valid = clientIdPattern().match(normalized()).hasMatch();
        ok->setEnabled(valid);
        problem->setText(valid || field->text().trimmed().isEmpty()
                             ? QString()
                             : tr("That does not look like an application (client) ID. It has the form "
                                  "xxxxxxxx-xxxx-4xxx-xxxx-xxxxxxxxxxxx."));
    });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    APPLICATION->settings()->set("MSAClientIDOverride", normalized());
    APPLICATION->updateCapabilities();
    return (APPLICATION->capabilities() & Application::SupportsMSA) != 0;
}

}  // namespace MicrosoftLoginSetup
