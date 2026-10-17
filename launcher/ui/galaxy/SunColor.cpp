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

#include "SunColor.h"

#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include "BaseInstance.h"
#include "settings/SettingsObject.h"
#include "CelestialPainter.h"
#include "GalaxySettings.h"

namespace SunColor {

namespace {

struct Preset {
    const char* name;
    QColor color;
};

const QList<Preset>& namedPresets()
{
    static const QList<Preset> list = {
        { QT_TRANSLATE_NOOP("SunColor", "Golden"), QColor(0xFF, 0xC8, 0x57) },
        { QT_TRANSLATE_NOOP("SunColor", "Solar white"), QColor(0xFF, 0xF1, 0xC9) },
        { QT_TRANSLATE_NOOP("SunColor", "Orange dwarf"), QColor(0xFF, 0x9F, 0x45) },
        { QT_TRANSLATE_NOOP("SunColor", "Red giant"), QColor(0xFF, 0x6B, 0x5B) },
        { QT_TRANSLATE_NOOP("SunColor", "Blue giant"), QColor(0x8E, 0xC5, 0xFF) },
        { QT_TRANSLATE_NOOP("SunColor", "Space purple"), QColor(0xB4, 0x8C, 0xFF) },
        { QT_TRANSLATE_NOOP("SunColor", "Magenta"), QColor(0xF4, 0x72, 0xD0) },
        { QT_TRANSLATE_NOOP("SunColor", "Teal"), QColor(0x5E, 0xE6, 0xD0) },
    };
    return list;
}

std::uint32_t stableHash(const QString& key)
{
    std::uint32_t hash = 2166136261U;
    for (const QChar c : key) {
        hash ^= c.unicode();
        hash *= 16777619U;
    }
    // Final avalanche (MurmurHash3 fmix32) so similar IDs still spread over the whole palette.
    hash ^= hash >> 16U;
    hash *= 0x85ebca6bU;
    hash ^= hash >> 13U;
    hash *= 0xc2b2ae35U;
    hash ^= hash >> 16U;
    return hash;
}

/** Live preview: the sun as it will appear in the galaxy. */
class SunPreview : public QWidget {
   public:
    explicit SunPreview(QWidget* parent) : QWidget(parent) { setFixedSize(150, 150); }
    void setColor(const QColor& color)
    {
        m_color = color;
        update();
    }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(rect()), 14, 14);
        p.setClipPath(clip);
        p.drawPixmap(0, 0, CelestialPainter::renderStarfield(size(), devicePixelRatioF(), 2));
        CelestialPainter::drawSun(p, QRectF(rect()).center(), 30, m_color, 1.0, 2);
    }

   private:
    QColor m_color;
};

}  // namespace

const QList<QColor>& presets()
{
    static const QList<QColor> colors = [] {
        QList<QColor> list;
        for (const auto& preset : namedPresets()) {
            list.append(preset.color);
        }
        return list;
    }();
    return colors;
}

QColor defaultFor(const QString& instanceId)
{
    const auto& list = presets();
    return list[static_cast<int>(stableHash(instanceId) % static_cast<std::uint32_t>(list.size()))];
}

bool isCustom(BaseInstance* instance)
{
    return instance && QColor::isValidColorName(instance->settings()->get(GalaxySettings::SunColor).toString());
}

QColor get(BaseInstance* instance)
{
    if (!instance) {
        return presets().first();
    }
    const auto stored = instance->settings()->get(GalaxySettings::SunColor).toString();
    if (QColor::isValidColorName(stored)) {
        return QColor::fromString(stored);
    }
    return defaultFor(instance->id());
}

void set(BaseInstance* instance, const QColor& color)
{
    if (!instance) {
        return;
    }
    instance->settings()->set(GalaxySettings::SunColor, color.isValid() ? color.name(QColor::HexRgb) : QString());
}

bool pick(BaseInstance* instance, QWidget* parent)
{
    if (!instance) {
        return false;
    }
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Sun color for %1").arg(instance->name()));

    auto* layout = new QHBoxLayout(&dialog);
    auto* left = new QVBoxLayout();
    auto* preview = new SunPreview(&dialog);
    left->addWidget(preview, 0, Qt::AlignHCenter);

    auto* presetLabel = new QLabel(QObject::tr("Presets"), &dialog);
    left->addWidget(presetLabel);
    auto* grid = new QGridLayout();
    grid->setSpacing(6);

    auto* picker = new QColorDialog(&dialog);
    picker->setWindowFlags(Qt::Widget);
    picker->setOptions(QColorDialog::NoButtons | QColorDialog::DontUseNativeDialog);
    picker->setCurrentColor(get(instance));

    int i = 0;
    for (const auto& preset : namedPresets()) {
        auto* button = new QToolButton(&dialog);
        QPixmap swatch(22, 22);
        swatch.fill(Qt::transparent);
        {
            QPainter p(&swatch);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(QColor(255, 255, 255, 120), 1));
            p.setBrush(preset.color);
            p.drawEllipse(QRectF(1.5, 1.5, 19, 19));
        }
        button->setIcon(swatch);
        button->setIconSize(swatch.size());
        // Named, so the choice does not depend on telling colors apart.
        const auto name = QCoreApplication::translate("SunColor", preset.name);
        button->setToolTip(name);
        button->setAccessibleName(name);
        button->setAutoRaise(true);
        QObject::connect(button, &QToolButton::clicked, picker, [picker, color = preset.color] { picker->setCurrentColor(color); });
        grid->addWidget(button, i / 4, i % 4);
        i++;
    }
    left->addLayout(grid);
    left->addStretch();

    auto* hint = new QLabel(QObject::tr("Only the launcher uses this color.\nThe game and its files are not changed."), &dialog);
    hint->setObjectName("galaxyHint");
    hint->setWordWrap(true);
    left->addWidget(hint);

    auto* right = new QVBoxLayout();
    right->addWidget(picker);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    auto* reset = buttons->addButton(QObject::tr("Reset to default"), QDialogButtonBox::ResetRole);
    right->addWidget(buttons);

    layout->addLayout(left);
    layout->addLayout(right, 1);

    preview->setColor(picker->currentColor());
    QObject::connect(picker, &QColorDialog::currentColorChanged, preview, &SunPreview::setColor);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    bool resetRequested = false;
    QObject::connect(reset, &QPushButton::clicked, &dialog, [&] {
        resetRequested = true;
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    set(instance, resetRequested ? QColor() : picker->currentColor());
    return true;
}

}  // namespace SunColor
