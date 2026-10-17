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

#include "ProjectActions.h"

#include "DiscoverWindow.h"
#include "UpdateChecker.h"
#include "UpdatesDialog.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QUrl>
#include <QVBoxLayout>

#include "Application.h"
#include "settings/SettingsObject.h"
#include "DesktopServices.h"
#include "ResourceDownloadTask.h"
#include "StringUtils.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/DataPackFolderModel.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ResourceFolderModel.h"
#include "minecraft/mod/ResourcePackFolderModel.h"
#include "minecraft/mod/ShaderPackFolderModel.h"
#include "tasks/ConcurrentTask.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/InstallLoaderDialog.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/ResourceDownloadDialog.h"
#include "ui/dialogs/ResourceUpdateDialog.h"
#include "ui/pages/modplatform/ResourcePage.h"

namespace ProjectActions {

namespace {

QString tr(const char* text)
{
    return QCoreApplication::translate("ProjectActions", text);
}

QString tr(const char* text, const char* disambiguation, int n)
{
    return QCoreApplication::translate("ProjectActions", text, disambiguation, n);
}

/** Finds the body's resource in its model again; snapshots never act on files directly. */
int rowOf(ResourceFolderModel* model, const QString& internalId)
{
    if (!model) {
        return -1;
    }
    for (int row = 0; row < model->size(); row++) {
        if (model->at(row).internalId() == internalId) {
            return row;
        }
    }
    return -1;
}

bool confirmWhileRunning(QWidget* parent, MinecraftInstance* instance, const QString& title, const QString& what)
{
    if (!instance || !instance->isRunning()) {
        return true;
    }
    const auto response = CustomMessageBox::selectable(parent, title,
                                                       tr("%1 while the game is running may crash the game, and files in use "
                                                          "may not be replaced until it exits.\n\nContinue anyway?")
                                                           .arg(what),
                                                       QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                              ->exec();
    return response == QMessageBox::Yes;
}

void reportMissing(QWidget* parent, const CelestialBody& body)
{
    CustomMessageBox::selectable(parent, tr("Not found"),
                                 tr("\"%1\" is no longer installed in this profile. The galaxy will refresh.").arg(body.name),
                                 QMessageBox::Information)
        ->show();
}

QString disableLabel(const CelestialBody& body)
{
    const bool enabling = !body.enabled;
    switch (body.kind) {
        case CelestialBody::Kind::Mod:
            return enabling ? tr("Enable mod") : tr("Disable mod (keeps the file)");
        case CelestialBody::Kind::ResourcePack:
        case CelestialBody::Kind::TexturePack:
            return enabling ? tr("Enable pack (show it in the game's pack list)") : tr("Disable pack (hide it from the game's pack list)");
        case CelestialBody::Kind::ShaderPack:
            return enabling ? tr("Enable shader pack (show it in the shader list)") : tr("Disable shader pack (hide it from the shader list)");
        case CelestialBody::Kind::DataPack:
            return enabling ? tr("Enable data pack") : tr("Disable data pack");
        case CelestialBody::Kind::BaseGame:
            break;
    }
    return {};
}

ResourceDownload::ResourceDownloadDialog* createDialog(QWidget* parent,
                                                       MinecraftInstance* instance,
                                                       CelestialBody::Kind kind,
                                                       ResourceFolderModel* model,
                                                       bool suppressInitialSearch)
{
    using Dialog = ResourceDownload::ResourceDownloadDialog;
    switch (kind) {
        case CelestialBody::Kind::Mod:
            return Dialog::createMod(parent, model, instance, suppressInitialSearch);
        case CelestialBody::Kind::ResourcePack:
            return Dialog::createResourcePack(parent, model, instance, suppressInitialSearch);
        case CelestialBody::Kind::TexturePack:
            return Dialog::createTexturePack(parent, model, instance, suppressInitialSearch);
        case CelestialBody::Kind::ShaderPack:
            return Dialog::createShaderPack(parent, model, instance, suppressInitialSearch);
        case CelestialBody::Kind::DataPack:
            return Dialog::createDataPack(parent, model, instance, suppressInitialSearch);
        case CelestialBody::Kind::BaseGame:
            break;
    }
    return nullptr;
}

/** Same behavior as the Mods page: offer to install a loader first. Returns true if mods can be installed now. */
/** A download replaces the old file with an enabled one; puts the ".disabled" back for content that was disabled. */
void keepDisabled(ResourceFolderModel* model, ResourceDownloadTask* download)
{
    if (!model || !download || download->getFilename().isEmpty()) {
        return;
    }
    const QString path = model->dir().filePath(download->getFilename());
    if (QFileInfo::exists(path) && !QFile::rename(path, path + ".disabled")) {
        qWarning() << "Could not keep" << path << "disabled after updating it";
    }
}

bool ensureModLoader(QWidget* parent, MinecraftInstance* instance)
{
    auto* profile = instance->getPackProfile();
    if (profile->getModLoaders().has_value()) {
        return true;
    }
    const int response = QMessageBox::question(parent, tr("Missing Mod Loader"),
                                               tr("You need to install a compatible mod loader before installing mods. Would you like to do so?"),
                                               QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (response != QMessageBox::Yes) {
        return false;
    }
    InstallLoaderDialog dialog(profile, QString(), parent);
    if (dialog.exec() == 0) {
        return false;
    }
    if (!profile->getModLoaders().has_value()) {
        CustomMessageBox::selectable(parent, tr("Error"), tr("No mod loader was installed. Please try again."), QMessageBox::Warning)->show();
        return false;
    }
    return true;
}

void openBaseGameMenu(QWidget* parent, MinecraftInstance* instance, const CelestialBody& body, const QPoint& globalPos)
{
    QMenu menu(parent);
    auto* header = menu.addAction(body.name);
    header->setEnabled(false);
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme("about"), tr("View details…"), parent, [parent, instance, body] { showDetails(parent, instance, body); });
    menu.addAction(QIcon::fromTheme("version"), tr("Change Minecraft version or mod loader…"), parent,
                   [instance] { APPLICATION->showInstanceWindow(instance, "version"); });
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme("loadermods"), tr("Add mods…"), parent,
                   [parent, instance] { openDownloadDialog(parent, instance, Category::Mods); });
    menu.addAction(QIcon::fromTheme("resourcepacks"), tr("Add resource packs…"), parent,
                   [parent, instance] { openDownloadDialog(parent, instance, Category::ResourcePacks); });
    menu.addAction(QIcon::fromTheme("shaderpacks"), tr("Add shader packs…"), parent,
                   [parent, instance] { openDownloadDialog(parent, instance, Category::ShaderPacks); });
    menu.addAction(QIcon::fromTheme("datapacks"), tr("Add data packs…"), parent,
                   [parent, instance] { openDownloadDialog(parent, instance, Category::DataPacks); });
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme("viewfolder"), tr("Open game folder"), parent,
                   [instance] { DesktopServices::openPath(instance->gameRoot(), true); });
    menu.exec(globalPos);
}

}  // namespace

/** Runs the download tasks of an accepted ResourceDownloadDialog, like the instance window's pages do. */
void runDownloads(QWidget* parent, ResourceDownload::ResourceDownloadDialog* dialog, ResourceFolderModel* rawModel)
{
    QPointer<ResourceFolderModel> model(rawModel);
    ConcurrentTask tasks(tr("Download"), APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
    QObject::connect(&tasks, &Task::failed, parent, [parent](const QString& reason) {
        CustomMessageBox::selectable(parent, tr("Error"), reason, QMessageBox::Critical)->show();
    });
    QObject::connect(&tasks, &Task::succeeded, parent, [parent, &tasks] {
        const auto warnings = tasks.warnings();
        if (!warnings.isEmpty()) {
            CustomMessageBox::selectable(parent, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();
        }
    });
    for (auto& task : dialog->getTasks()) {
        tasks.addTask(task);
    }
    ProgressDialog progress(parent);
    progress.showSkipButton();
    progress.execWithTask(&tasks);
    if (model) {
        model->update();
    }
}

void activate(QWidget* parent, MinecraftInstance* instance, const CelestialBody& body)
{
    // The details dialog links to the Modrinth page when there is one.
    showDetails(parent, instance, body);
}

void showContextMenu(QWidget* parent, MinecraftInstance* instance, const CelestialBody& body, const QPoint& globalPos)
{
    if (!instance) {
        return;
    }
    if (body.kind == CelestialBody::Kind::BaseGame) {
        openBaseGameMenu(parent, instance, body, globalPos);
        return;
    }

    QMenu menu(parent);
    auto* header = menu.addAction(QStringLiteral("%1 · %2").arg(body.name, CelestialBody::kindName(body.kind)));
    header->setEnabled(false);
    menu.addSeparator();

    if (!body.projectUrl.isEmpty()) {
        menu.addAction(QIcon::fromTheme("viewfolder"), tr("View project page on %1").arg(body.provider), parent,
                       [url = body.projectUrl] { DesktopServices::openUrl(QUrl(url)); });
    } else if (!body.homepage.isEmpty()) {
        // Declared by the file's own metadata: not a source platform page, so it is labelled as such.
        menu.addAction(tr("Open homepage declared by the file"), parent, [url = body.homepage] { DesktopServices::openUrl(QUrl(url)); });
    }
    menu.addAction(QIcon::fromTheme("about"), tr("View details…"), parent, [parent, instance, body] { showDetails(parent, instance, body); });
    menu.addSeparator();

    if (body.updateAvailable) {
        menu.addAction(QIcon::fromTheme("checkupdate"), tr("Update to %1…").arg(body.latestVersion), parent,
                       [parent, instance, body] { update(parent, instance, body); });
    }
    auto* changeVersionAction =
        menu.addAction(QIcon::fromTheme("checkupdate"), tr("Change version…"), parent, [parent, instance, body] { changeVersion(parent, instance, body); });
    if (!body.hasMetadata) {
        changeVersionAction->setEnabled(false);
        changeVersionAction->setText(tr("Change version… (no source platform metadata)"));
    } else if (APPLICATION->settings()->get("ModMetadataDisabled").toBool()) {
        changeVersionAction->setEnabled(false);
        changeVersionAction->setText(tr("Change version… (metadata is disabled in settings)"));
    }

    menu.addAction(QIcon::fromTheme(body.enabled ? "status-yellow" : "status-good"), disableLabel(body), parent,
                   [parent, instance, body] { toggleEnabled(parent, instance, body); });
    menu.addAction(QIcon::fromTheme("viewfolder"), tr("Show in folder"), parent, [path = body.filePath] {
        DesktopServices::openPath(QFileInfo(path).absolutePath(), true);
    });
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme("delete"), tr("Delete…"), parent, [parent, instance, body] { remove(parent, instance, body); });
    menu.exec(globalPos);
}

void showDetails(QWidget* parent, MinecraftInstance* instance, const CelestialBody& body)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(body.name);
    dialog.setMinimumWidth(460);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);

    auto add = [&dialog, form](const QString& label, const QString& value, bool link = false) {
        if (value.isEmpty()) {
            return;
        }
        auto* field = new QLabel(&dialog);
        field->setWordWrap(true);
        field->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
        if (link) {
            field->setTextFormat(Qt::RichText);
            field->setText(QStringLiteral("<a href=\"%1\">%1</a>").arg(value.toHtmlEscaped()));
            field->setOpenExternalLinks(false);
            QObject::connect(field, &QLabel::linkActivated, field, [](const QString& url) { DesktopServices::openUrl(QUrl(url)); });
        } else {
            field->setTextFormat(Qt::PlainText);
            field->setText(value);
        }
        form->addRow(label, field);
    };

    add(tr("Name:"), body.name);
    add(tr("Type:"), CelestialBody::kindName(body.kind));
    if (body.kind == CelestialBody::Kind::BaseGame) {
        add(tr("Minecraft:"), body.version);
        add(tr("Mod loader:"), body.description.isEmpty() ? tr("None (vanilla)") : body.description);
        add(tr("Game folder:"), body.filePath);
    } else {
        add(tr("Version:"), body.version.isEmpty() ? tr("Unknown") : body.version);
        add(tr("Authors:"), body.authors.join(", "));
        add(tr("Source:"), body.hasMetadata ? body.provider : tr("Local file (no source platform metadata)"));
        add(tr("Project page:"), body.projectUrl, true);
        add(tr("Declared homepage:"), body.homepage, true);
        add(tr("File:"), body.filePath);
        add(tr("Size:"), body.bytes >= 0 ? StringUtils::humanReadableFileSize(static_cast<double>(body.bytes), true) : tr("Unknown"));
        add(tr("Status:"), body.enabled ? tr("Enabled") : tr("Disabled"));
        add(tr("Problems:"), body.issues.join('\n'));
        add(tr("Description:"), body.description);
    }
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    if (!body.projectUrl.isEmpty()) {
        auto* open = buttons->addButton(tr("Open project page"), QDialogButtonBox::ActionRole);
        QObject::connect(open, &QPushButton::clicked, open, [url = body.projectUrl] { DesktopServices::openUrl(QUrl(url)); });
    }
    auto* folder = buttons->addButton(tr("Show in folder"), QDialogButtonBox::ActionRole);
    QObject::connect(folder, &QPushButton::clicked, folder, [body] {
        DesktopServices::openPath(body.kind == CelestialBody::Kind::BaseGame ? body.filePath : QFileInfo(body.filePath).absolutePath(), true);
    });
    if (body.kind == CelestialBody::Kind::BaseGame && instance) {
        auto* version = buttons->addButton(tr("Edit versions"), QDialogButtonBox::ActionRole);
        QObject::connect(version, &QPushButton::clicked, &dialog, [&dialog, instance] {
            dialog.accept();
            APPLICATION->showInstanceWindow(instance, "version");
        });
    }
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

void toggleEnabled(QWidget* parent, MinecraftInstance* instance, const CelestialBody& body)
{
    auto* model = body.model.data();
    const int row = rowOf(model, body.internalId);
    if (row < 0) {
        reportMissing(parent, body);
        return;
    }
    if (!confirmWhileRunning(parent, instance, tr("Confirm change"), body.enabled ? tr("Disabling content") : tr("Enabling content"))) {
        return;
    }
    const auto index = model->index(row, 0);
    if (body.enabled) {
        if (auto* mods = dynamic_cast<ModFolderModel*>(model)) {
            // Same safeguard as the Mods page: tell the user which mods depend on this one.
            const auto affected = mods->getAffectedMods({ index }, EnableAction::DISABLE);
            if (!affected.isEmpty()) {
                auto* box = CustomMessageBox::selectable(parent, tr("Confirm Disable"),
                                                         tr("%n other mod(s) require \"%1\". Disable them as well?", "", affected.length())
                                                             .arg(body.name),
                                                         QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                                         QMessageBox::Cancel);
                QString details = tr("These mods depend on it:");
                for (const auto& dependent : affected) {
                    details += QStringLiteral("\n- %1").arg(mods->at(dependent.row()).name());
                }
                box->setDetailedText(details);
                const auto response = box->exec();
                if (response == QMessageBox::Cancel) {
                    return;
                }
                if (response == QMessageBox::Yes) {
                    mods->setResourceEnabled(affected, EnableAction::DISABLE);
                }
            }
        }
    }
    if (!model->setResourceEnabled({ index }, body.enabled ? EnableAction::DISABLE : EnableAction::ENABLE)) {
        CustomMessageBox::selectable(parent, tr("Error"), tr("Could not change \"%1\". Check that the file is not read-only.").arg(body.name),
                                     QMessageBox::Warning)
            ->show();
    }
}

void remove(QWidget* parent, MinecraftInstance* instance, const CelestialBody& body)
{
    auto* model = body.model.data();
    const int row = rowOf(model, body.internalId);
    if (row < 0) {
        reportMissing(parent, body);
        return;
    }
    const auto response = CustomMessageBox::selectable(
                              parent, tr("Confirm Delete"),
                              tr("Delete %1 \"%2\" from \"%3\"?\n\nThis removes:\n%4\n\nThe file is moved to the trash when possible. "
                                 "Other profiles are not affected.")
                                  .arg(CelestialBody::kindName(body.kind).toLower(), body.name, instance ? instance->name() : QString(),
                                       body.filePath),
                              QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                              ->exec();
    if (response != QMessageBox::Yes) {
        return;
    }
    if (!confirmWhileRunning(parent, instance, tr("Confirm Delete"), tr("Deleting content"))) {
        return;
    }
    const auto index = model->index(row, 0);
    if (auto* mods = dynamic_cast<ModFolderModel*>(model)) {
        const auto affected = mods->getAffectedMods({ index }, EnableAction::DISABLE);
        if (!affected.isEmpty()) {
            auto* box = CustomMessageBox::selectable(
                parent, tr("Confirm Disable"),
                tr("%n other mod(s) require \"%1\". Disable them so the game can still start?", "", affected.length()).arg(body.name),
                QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Cancel);
            const auto answer = box->exec();
            if (answer == QMessageBox::Cancel) {
                return;
            }
            if (answer == QMessageBox::Yes) {
                mods->setResourceEnabled(affected, EnableAction::DISABLE);
            }
        }
    }
    if (!model->deleteResources({ index })) {
        CustomMessageBox::selectable(parent, tr("Error"), tr("Could not delete \"%1\".").arg(body.name), QMessageBox::Warning)->show();
    }
}

void changeVersion(QWidget* parent, MinecraftInstance* instance, const CelestialBody& body)
{
    auto* model = body.model.data();
    const int row = rowOf(model, body.internalId);
    if (row < 0) {
        reportMissing(parent, body);
        return;
    }
    auto metadata = model->at(row).metadata();
    if (!metadata) {
        CustomMessageBox::selectable(parent, tr("Change version"),
                                     tr("\"%1\" was not installed from Modrinth, so there are no other versions to choose from.")
                                         .arg(body.name),
                                     QMessageBox::Information)
            ->show();
        return;
    }
    if (APPLICATION->settings()->get("ModMetadataDisabled").toBool()) {
        QMessageBox::critical(parent, tr("Error"), tr("Version changes are unavailable when metadata is disabled!"));
        return;
    }
    if (!confirmWhileRunning(parent, instance, tr("Confirm Change Version"), tr("Changing versions"))) {
        return;
    }
    if (body.kind == CelestialBody::Kind::Mod && !ensureModLoader(parent, instance)) {
        return;
    }

    auto* dialog = createDialog(parent, instance, body.kind, model, true);
    if (!dialog) {
        return;
    }
    QPointer<ResourceFolderModel> guardedModel(model);
    QObject::connect(dialog, &QDialog::finished, parent, [parent, dialog, guardedModel](int result) {
        if (result != 0) {
            runDownloads(parent, dialog, guardedModel.data());
        }
        dialog->deleteLater();
    });
    // Opens the project's version list; the old file is only replaced after the user picks a version and confirms.
    dialog->setResourceMetadata(metadata);
    dialog->open();
}

void update(QWidget* parent, MinecraftInstance* instance, const CelestialBody& body)
{
    auto* model = body.model.data();
    const int row = rowOf(model, body.internalId);
    if (row < 0) {
        reportMissing(parent, body);
        return;
    }
    if (model->at(row).lockUpdate()) {
        // Prism locks the files of an imported modpack so the pack stays as it was published.
        const auto response =
            CustomMessageBox::selectable(parent, tr("Updates locked"),
                                         tr("\"%1\" came with a modpack, so its updates are locked to keep the pack as it was "
                                            "published.\n\nUnlock it and update it anyway?")
                                             .arg(body.name),
                                         QMessageBox::Question, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                ->exec();
        if (response != QMessageBox::Yes) {
            return;
        }
        const auto updateColumn = static_cast<int>(model->columnNames(false).indexOf("Update"));
        model->setUpdateLock({ model->index(row, updateColumn) }, EnableAction::DISABLE);
    }
    if (!confirmWhileRunning(parent, instance, tr("Confirm Update"), tr("Updating content"))) {
        return;
    }
    QList<Resource*> resources{ &model->at(row) };
    QList<ModPlatform::ModLoaderType> loaders;
    if (body.kind == CelestialBody::Kind::Mod) {
        loaders = instance->getPackProfile()->getModLoadersList();
    } else if (body.kind == CelestialBody::Kind::DataPack) {
        loaders = { ModPlatform::ModLoaderType::DataPack };
    }
    ResourceUpdateDialog dialog(parent, instance, model, resources, body.kind == CelestialBody::Kind::Mod, loaders);
    dialog.checkCandidates();
    if (dialog.aborted()) {
        return;
    }
    if (dialog.noUpdates()) {
        CustomMessageBox::selectable(parent, tr("Update checker"), tr("\"%1\" is up to date.").arg(body.name))->exec();
        return;
    }
    if (dialog.exec() == 0) {
        return;
    }
    ConcurrentTask tasks(tr("Update"), APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
    QObject::connect(&tasks, &Task::failed, parent, [parent](const QString& reason) {
        CustomMessageBox::selectable(parent, tr("Error"), reason, QMessageBox::Critical)->show();
    });
    const auto downloads = dialog.getTasks();
    for (const auto& task : downloads) {
        tasks.addTask(task);
    }
    ProgressDialog progress(parent);
    progress.showSkipButton();
    progress.execWithTask(&tasks);
    if (!body.enabled) {
        for (const auto& task : downloads) {
            auto* download = dynamic_cast<ResourceDownloadTask*>(task.get());
            if (download && download->getPack() && download->getPack()->name == body.name && download->wasSuccessful()) {
                keepDisabled(model, download);
            }
        }
    }
    model->update();
    UpdateChecker::instance()->check(instance, true);  // once the folder has reloaded
}

void updateAll(QWidget* parent, MinecraftInstance* instance)
{
    if (!instance) {
        return;
    }
    const auto updates = UpdateChecker::instance()->updatesFor(instance->id());
    if (updates.isEmpty()) {
        CustomMessageBox::selectable(parent, tr("Updates"), tr("Everything in \"%1\" is up to date.").arg(instance->name()))->exec();
        return;
    }
    UpdatesDialog dialog(parent, instance->name(), updates);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto chosen = dialog.selected();
    if (chosen.isEmpty() || !confirmWhileRunning(parent, instance, tr("Confirm Update"), tr("Updating content"))) {
        return;
    }

    ConcurrentTask tasks(tr("Update"), APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
    QObject::connect(&tasks, &Task::failed, parent, [parent](const QString& reason) {
        CustomMessageBox::selectable(parent, tr("Error"), reason, QMessageBox::Critical)->show();
    });
    QSet<ResourceFolderModel*> models;
    for (const auto& update : chosen) {
        if (update.model && update.download) {
            tasks.addTask(update.download);
            models.insert(update.model);
        }
    }
    // The download tasks run once; a fresh check (after the folders reload) finds anything left.
    UpdateChecker::instance()->discardPending(instance->id());
    ProgressDialog progress(parent);
    progress.showSkipButton();
    progress.execWithTask(&tasks);
    for (const auto& update : chosen) {
        if (!update.enabled && update.download && update.download->wasSuccessful()) {
            keepDisabled(update.model, update.download.get());
        }
    }
    for (auto* model : models) {
        model->update();
    }
    UpdateChecker::instance()->check(instance, true);
}

void openDownloadDialog(QWidget* parent, MinecraftInstance* instance, Category category, const QString& searchTerm)
{
    DiscoverWindow::showFor(parent, instance, category, searchTerm);
}

}  // namespace ProjectActions
