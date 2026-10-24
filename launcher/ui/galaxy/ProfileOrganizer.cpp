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

#include "ProfileOrganizer.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <climits>

#include "Application.h"
#include "GalaxySettings.h"
#include "InstanceList.h"
#include "icons/IconList.h"
#include "settings/SettingsObject.h"

namespace {

QStringList readList(const QString& key)
{
    QStringList out;
    for (const auto& value : QJsonDocument::fromJson(APPLICATION->settings()->get(key).toString().toUtf8()).array()) {
        if (const auto text = value.toString(); !text.isEmpty()) {
            out << text;
        }
    }
    return out;
}

void writeList(const QString& key, const QStringList& list)
{
    APPLICATION->settings()->set(key, QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(list)).toJson(QJsonDocument::Compact)));
}

}  // namespace

ProfileOrganizer* ProfileOrganizer::instance()
{
    static auto* organizer = new ProfileOrganizer();
    return organizer;
}

ProfileOrganizer::ProfileOrganizer()
{
    m_order = readList(GalaxySettings::ProfileOrder);
    m_empty = readList(GalaxySettings::EmptyCategories);
    m_top = readList(GalaxySettings::SidebarOrder);
}

void ProfileOrganizer::save()
{
    writeList(GalaxySettings::ProfileOrder, m_order);
    writeList(GalaxySettings::EmptyCategories, m_empty);
    writeList(GalaxySettings::SidebarOrder, m_top);
    emit changed();
}

void ProfileOrganizer::setOrder(const QStringList& ids)
{
    if (ids == m_order) {
        return;
    }
    m_order = ids;
    save();
}

void ProfileOrganizer::sort(QStringList& ids) const
{
    if (m_order.isEmpty()) {
        return;
    }
    auto rank = [this](const QString& id) {
        const auto index = m_order.indexOf(id);
        return index < 0 ? INT_MAX : static_cast<int>(index);
    };
    std::stable_sort(ids.begin(), ids.end(), [&rank](const QString& a, const QString& b) { return rank(a) < rank(b); });
}

void ProfileOrganizer::setTopOrder(const QStringList& tokens)
{
    if (tokens == m_top) {
        return;
    }
    m_top = tokens;
    save();
}

void ProfileOrganizer::sortTop(QStringList& tokens) const
{
    if (m_top.isEmpty()) {
        return;
    }
    auto rank = [this](const QString& token) {
        const auto index = m_top.indexOf(token);
        if (index >= 0) {
            return static_cast<int>(index);
        }
        return token.startsWith(QStringLiteral("p:")) ? -1 : INT_MAX;  // new profiles first, new categories last
    };
    std::stable_sort(tokens.begin(), tokens.end(), [&rank](const QString& a, const QString& b) { return rank(a) < rank(b); });
}

void ProfileOrganizer::renameInOrder(const QString& from, const QString& to)
{
    const auto index = m_top.indexOf(categoryToken(from));
    if (index >= 0) {
        m_top[index] = categoryToken(to);
        save();
    }
}

QStringList ProfileOrganizer::categories() const
{
    QStringList all = APPLICATION->instances()->getGroups();
    for (const auto& name : m_empty) {
        if (!all.contains(name, Qt::CaseInsensitive)) {
            all << name;
        }
    }
    all.removeAll(QString());
    std::sort(all.begin(), all.end(), [](const QString& a, const QString& b) { return QString::localeAwareCompare(a.toLower(), b.toLower()) < 0; });
    return all;
}

bool ProfileOrganizer::isEmptyCategory(const QString& name) const
{
    return m_empty.contains(name) && !APPLICATION->instances()->getGroups().contains(name);
}

void ProfileOrganizer::removeEmptyCategory(const QString& name)
{
    if (m_empty.removeAll(name) > 0) {
        save();
    }
}

void ProfileOrganizer::renameEmptyCategory(const QString& from, const QString& to)
{
    const auto index = m_empty.indexOf(from);
    if (index >= 0) {
        m_empty[index] = to;
        save();
    }
}

void ProfileOrganizer::moveToCategory(const QStringList& ids, const QString& category)
{
    auto* list = APPLICATION->instances();
    for (const auto& id : ids) {
        list->setInstanceGroup(id, category);
    }
    if (!category.isEmpty() && !ids.isEmpty()) {
        removeEmptyCategory(category);  // it has profiles now
    }
}

bool ProfileOrganizer::createCategory(QWidget* parent, const QString& selectedId)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(tr("New category"));
    dialog.setMinimumWidth(380);
    auto* layout = new QVBoxLayout(&dialog);

    auto* nameLabel = new QLabel(tr("Name:"), &dialog);
    auto* name = new QLineEdit(&dialog);
    name->setPlaceholderText(tr("For example: Modded, Servers, Old versions"));
    nameLabel->setBuddy(name);
    auto* problem = new QLabel(&dialog);
    problem->setStyleSheet(QStringLiteral("color: #F87171;"));
    problem->hide();

    auto* listLabel = new QLabel(tr("Profiles to put in it (you can also drag profiles onto it in the sidebar later):"), &dialog);
    listLabel->setWordWrap(true);
    auto* profiles = new QListWidget(&dialog);
    listLabel->setBuddy(profiles);
    auto* instances = APPLICATION->instances();
    QStringList ids;
    for (int i = 0; i < instances->count(); i++) {
        ids << instances->at(i)->id();
    }
    std::sort(ids.begin(), ids.end(), [instances](const QString& a, const QString& b) {
        return QString::localeAwareCompare(instances->getInstanceById(a)->name().toLower(), instances->getInstanceById(b)->name().toLower()) < 0;
    });
    sort(ids);
    for (const auto& id : ids) {
        auto* inst = instances->getInstanceById(id);
        const auto current = instances->getInstanceGroup(id);
        auto* item = new QListWidgetItem(APPLICATION->icons()->getIcon(inst->iconKey()),
                                         current.isEmpty() ? inst->name() : tr("%1 (now in %2)").arg(inst->name(), current), profiles);
        item->setData(Qt::UserRole, id);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        item->setCheckState(id == selectedId ? Qt::Checked : Qt::Unchecked);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Create"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    layout->addWidget(nameLabel);
    layout->addWidget(name);
    layout->addWidget(problem);
    layout->addSpacing(6);
    layout->addWidget(listLabel);
    layout->addWidget(profiles, 1);
    layout->addWidget(buttons);

    auto validate = [this, name, problem, buttons] {
        const auto text = name->text().simplified();
        const bool exists = categories().contains(text, Qt::CaseInsensitive);
        problem->setText(exists ? tr("A category with this name already exists.") : QString());
        problem->setVisible(exists);
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!text.isEmpty() && !exists);
    };
    connect(name, &QLineEdit::textChanged, &dialog, validate);
    validate();
    name->setFocus();

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    const auto category = name->text().simplified();
    QStringList chosen;
    for (int i = 0; i < profiles->count(); i++) {
        if (profiles->item(i)->checkState() == Qt::Checked) {
            chosen << profiles->item(i)->data(Qt::UserRole).toString();
        }
    }
    if (chosen.isEmpty()) {
        m_empty << category;
        save();
    } else {
        moveToCategory(chosen, category);
    }
    return true;
}
