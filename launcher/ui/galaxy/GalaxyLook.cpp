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

#include "GalaxyLook.h"

#include <QAbstractButton>
#include <QAction>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHash>
#include <QIconEngine>
#include <QImage>
#include <QPainter>
#include <QPointer>

#include <cmath>

#include "BuildConfig.h"

namespace GalaxyLook {

namespace {

bool s_orbit = false;

/** Glyphs Orbit's icon set does not have, drawn on the same 9x9 grid ('#' = set). */
const QHash<QString, QStringList>& extraGlyphs()
{
    static const QHash<QString, QStringList> glyphs{
        { "minus", { ".........", ".........", ".........", ".........", ".#######.", ".........", ".........", ".........", "........." } },
        { "stop", { ".........", ".........", "..#####..", "..#####..", "..#####..", "..#####..", "..#####..", ".........", "........." } },
        { "menu", { ".........", ".........", ".#######.", ".........", ".#######.", ".........", ".#######.", ".........", "........." } },
        { "window-minimize", { ".........", ".........", ".........", ".........", ".........", ".........", "..#####..", ".........", "........." } },
        { "maximize", { ".........", ".#######.", ".#######.", ".#.....#.", ".#.....#.", ".#.....#.", ".#.....#.", ".#######.", "........." } },
        { "restore", { ".........", "...#####.", "...#...#.", ".#####.#.", ".#...#.#.", ".#...###.", ".#...#...", ".#####...", "........." } },
        { "fit", { "###...###", "#.......#", "#.......#", ".........", "....#....", ".........", "#.......#", "#.......#", "###...###" } },
        { "help", { "..#####..", ".##...##.", ".......#.", "......##.", "....##...", "....#....", ".........", "....#....", "........." } },
        { "list", { ".........", "##.#####.", ".........", "##.#####.", ".........", "##.#####.", ".........", "##.#####.", "........." } },
        { "tag", { "#####....", "#....#...", "#.#...#..", "#......#.", "#.......#", ".#.....#.", "..#...#..", "...#.#...", "....#...." } },
        { "warning", { "....#....", "...#.#...", "...#.#...", "..#.#.#..", "..#.#.#..", ".#.....#.", ".#..#..#.", "#.......#", "#########" } },
        { "info", { "....#....", ".........", "...##....", "....#....", "....#....", "....#....", "....#....", "...###...", "........." } },
    };
    return glyphs;
}

/** The glyph as a white 9x9 (or so) mask. */
QImage glyph(const QString& name)
{
    static QHash<QString, QImage> cache;
    if (auto it = cache.constFind(name); it != cache.constEnd()) {
        return *it;
    }
    QImage image;
    if (const auto& extra = extraGlyphs(); extra.contains(name)) {
        const auto& rows = extra[name];
        image = QImage(9, 9, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        for (int y = 0; y < rows.size(); y++) {
            for (int x = 0; x < rows[y].size(); x++) {
                if (rows[y][x] == '#') {
                    image.setPixel(x, y, qRgba(255, 255, 255, 255));
                }
            }
        }
    } else {
        image = QImage(":/orbit/icons/" + name + ".png").convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
    cache.insert(name, image);
    return image;
}

QImage tinted(const QImage& mask, const QColor& color)
{
    QImage out = mask.copy();
    QPainter p(&out);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(out.rect(), color);
    return out;
}

/** Draws the glyph scaled by whole pixels to the largest size that fits, centred, without smoothing. */
class PixelIconEngine : public QIconEngine {
   public:
    PixelIconEngine(QString name, QColor color) : m_name(std::move(name)), m_color(std::move(color)) {}

    QColor colorFor(QIcon::Mode mode) const
    {
        if (m_color.isValid()) {
            return mode == QIcon::Disabled ? Orbit::Faint : m_color;
        }
        switch (mode) {
            case QIcon::Disabled:
                return Orbit::Faint;
            case QIcon::Active:
            case QIcon::Selected:
                return Orbit::Text;
            default:
                return Orbit::Muted;
        }
    }

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State) override
    {
        const QImage mask = glyph(m_name);
        if (mask.isNull()) {
            return;
        }
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        const int devicePixels = static_cast<int>(std::floor(std::min(rect.width(), rect.height()) * dpr));
        const int scale = std::max(1, devicePixels / std::max(mask.width(), mask.height()));
        const QSizeF size(mask.width() * scale / dpr, mask.height() * scale / dpr);
        QPointF at(rect.x() + (rect.width() - size.width()) / 2, rect.y() + (rect.height() - size.height()) / 2);
        // Whole device pixels, so every glyph pixel is the same size.
        at = QPointF(std::round(at.x() * dpr) / dpr, std::round(at.y() * dpr) / dpr);
        painter->save();
        painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter->drawImage(QRectF(at, size), tinted(mask, colorFor(mode)));
        painter->restore();
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override { return scaledPixmap(size, mode, state, 1.0); }

    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        QPixmap pixmap(size * scale);
        pixmap.setDevicePixelRatio(scale);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        paint(&p, QRect(QPoint(0, 0), size), mode, state);
        return pixmap;
    }

    QSize actualSize(const QSize& size, QIcon::Mode, QIcon::State) override { return size; }
    QIconEngine* clone() const override { return new PixelIconEngine(m_name, m_color); }
    QString key() const override { return QStringLiteral("GalaxyLookPixel"); }

   private:
    QString m_name;
    QColor m_color;
};

QString s_fontFamily;

void loadFonts()
{
    if (!s_fontFamily.isEmpty()) {
        return;
    }
    const int regular = QFontDatabase::addApplicationFont(":/orbit/Silkscreen-Regular.ttf");
    QFontDatabase::addApplicationFont(":/orbit/Silkscreen-Bold.ttf");
    const auto families = QFontDatabase::applicationFontFamilies(regular);
    s_fontFamily = families.isEmpty() ? QStringLiteral("Silkscreen") : families.first();
}

}  // namespace

Notifier* notifier()
{
    static auto* instance = new Notifier;
    return instance;
}

void setTheme(const QString& themeId)
{
    const bool orbit = themeId == QLatin1String("orbit");
    if (orbit) {
        loadFonts();  // before any style sheet asks for Silkscreen
    }
    if (orbit == s_orbit) {
        return;
    }
    s_orbit = orbit;
    // Window titles end with the display name; the application itself stays Galaxy Launcher.
    QGuiApplication::setApplicationDisplayName(QStringLiteral("%1 %2").arg(launcherName(), BuildConfig.GALAXY_VERSION));
    QGuiApplication::setWindowIcon(logo());
    emit notifier()->changed();
}

bool orbit()
{
    return s_orbit;
}

QString launcherName()
{
    return s_orbit ? QStringLiteral("Orbit Launcher") : BuildConfig.LAUNCHER_DISPLAYNAME;
}

QIcon logo()
{
    if (!s_orbit) {
        return QIcon(":/" + BuildConfig.LAUNCHER_SVGFILENAME);
    }
    QIcon icon;
    icon.addFile(":/orbit/logo-32.png", QSize(32, 32));
    icon.addFile(":/orbit/logo.png", QSize(512, 512));
    return icon;
}

QColor accent()
{
    return s_orbit ? Orbit::Red : QColor(167, 139, 250);
}

QColor textColor()
{
    return s_orbit ? Orbit::Text : QColor(236, 233, 255);
}

QColor mutedText()
{
    return s_orbit ? Orbit::Muted : QColor(184, 178, 217);
}

QColor orbitPath()
{
    return s_orbit ? Orbit::Muted : QColor(176, 160, 255);
}

QColor ringColor()
{
    return s_orbit ? Orbit::EdgeHot : QColor(214, 200, 255);
}

QColor panelColor()
{
    return s_orbit ? Orbit::Panel : QColor(14, 10, 34);
}

QFont pixelFont(int pixelSize)
{
    loadFonts();
    QFont font(s_fontFamily);
    font.setPixelSize(pixelSize);
    font.setStyleStrategy(QFont::NoAntialias);
    font.setHintingPreference(QFont::PreferNoHinting);
    return font;
}

QIcon pixelIcon(const QString& name, const QColor& color)
{
    return QIcon(new PixelIconEngine(name, color));
}

QIcon icon(const QIcon& galaxy, const QString& orbitName)
{
    return s_orbit ? pixelIcon(orbitName) : galaxy;
}

void onChange(QObject* context, const std::function<void()>& apply)
{
    apply();
    QObject::connect(notifier(), &Notifier::changed, context, apply);
}

void bindIcon(QObject* target, const QString& orbitName, const QColor& color)
{
    auto currentIcon = [target]() -> QIcon {
        if (auto* action = qobject_cast<QAction*>(target)) {
            return action->icon();
        }
        if (auto* button = qobject_cast<QAbstractButton*>(target)) {
            return button->icon();
        }
        return {};
    };
    auto setIcon = [target](const QIcon& icon) {
        if (auto* action = qobject_cast<QAction*>(target)) {
            action->setIcon(icon);
        } else if (auto* button = qobject_cast<QAbstractButton*>(target)) {
            button->setIcon(icon);
        }
    };
    if (!target->property("galaxyLookIcon").isValid()) {
        target->setProperty("galaxyLookIcon", currentIcon());
    }
    QPointer<QObject> guard(target);
    onChange(target, [guard, setIcon, orbitName, color] {
        if (!guard) {
            return;
        }
        setIcon(s_orbit ? pixelIcon(orbitName, color) : guard->property("galaxyLookIcon").value<QIcon>());
    });
}

}  // namespace GalaxyLook
