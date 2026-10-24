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

#include "CelestialPainter.h"

#include <QConicalGradient>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QtMath>

namespace CelestialPainter {

namespace {

/** FNV-1a: stable across runs, unlike qHash which is seeded per process. */
std::uint32_t stableHash(const QString& key)
{
    std::uint32_t hash = 2166136261U;
    for (const QChar c : key) {
        hash ^= c.unicode();
        hash *= 16777619U;
    }
    return hash;
}

/** Tiny deterministic generator for per-body surface details. */
struct DetailRandom {
    std::uint32_t state;
    explicit DetailRandom(std::uint32_t seed) : state(seed ? seed : 0x9e3779b9U) {}
    qreal next()
    {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        return static_cast<qreal>(state % 100000U) / 100000.0;
    }
};

QColor withAlpha(QColor color, int alpha)
{
    color.setAlpha(std::clamp(alpha, 0, 255));
    return color;
}

QColor mix(const QColor& a, const QColor& b, qreal t)
{
    return QColor::fromRgbF(static_cast<float>(a.redF() + (b.redF() - a.redF()) * t),
                            static_cast<float>(a.greenF() + (b.greenF() - a.greenF()) * t),
                            static_cast<float>(a.blueF() + (b.blueF() - a.blueF()) * t),
                            static_cast<float>(a.alphaF() + (b.alphaF() - a.alphaF()) * t));
}

QPointF normalized(QPointF v)
{
    const qreal len = std::hypot(v.x(), v.y());
    return len > 1e-6 ? v / len : QPointF(-0.6, -0.8);
}

void drawSurfaceDetail(QPainter& p, QPointF c, qreal r, const SurfaceStyle& style)
{
    DetailRandom rnd(style.seed);
    QPainterPath clip;
    clip.addEllipse(c, r, r);
    p.save();
    p.setClipPath(clip);
    p.setPen(Qt::NoPen);

    switch (style.surface) {
        case Surface::Banded: {
            const int bands = 3 + static_cast<int>(rnd.next() * 3);
            for (int i = 0; i < bands; i++) {
                const qreal y = c.y() - r + (2 * r) * ((i + 0.5 + (rnd.next() - 0.5) * 0.6) / bands);
                const qreal h = r * (0.08 + rnd.next() * 0.16);
                p.setBrush(withAlpha(i % 2 ? style.accent : style.base.lighter(125), 70 + static_cast<int>(rnd.next() * 50)));
                p.drawEllipse(QPointF(c.x() + (rnd.next() - 0.5) * r * 0.3, y), r * 1.5, h);
            }
            break;
        }
        case Surface::Rocky: {
            const int craters = 3 + static_cast<int>(rnd.next() * 4);
            for (int i = 0; i < craters; i++) {
                const qreal angle = rnd.next() * 2 * M_PI;
                const qreal dist = std::sqrt(rnd.next()) * r * 0.75;
                const qreal cr = r * (0.08 + rnd.next() * 0.16);
                const QPointF at = c + QPointF(std::cos(angle), std::sin(angle)) * dist;
                p.setBrush(withAlpha(style.accent.darker(130), 90));
                p.drawEllipse(at, cr, cr * 0.9);
                p.setBrush(withAlpha(style.base.lighter(140), 55));
                p.drawEllipse(at + QPointF(-cr * 0.25, -cr * 0.25), cr * 0.55, cr * 0.5);
            }
            break;
        }
        case Surface::Terran: {
            // Blocky continents, in keeping with the game the planet stands for.
            const QColor land(98, 168, 72);
            const int continents = 4;
            for (int i = 0; i < continents; i++) {
                const qreal x = c.x() + (rnd.next() - 0.5) * r * 1.4;
                const qreal y = c.y() + (rnd.next() - 0.5) * r * 1.3;
                const qreal w = r * (0.35 + rnd.next() * 0.45);
                const qreal h = r * (0.2 + rnd.next() * 0.3);
                p.setBrush(withAlpha(land, 210));
                p.drawRect(QRectF(x - w / 2, y - h / 2, w, h));
                p.setBrush(withAlpha(land.darker(125), 200));
                p.drawRect(QRectF(x - w / 2 + w * 0.15, y + h / 2 - h * 0.3, w * 0.5, h * 0.5));
            }
            p.setBrush(withAlpha(QColor(240, 248, 255), 200));
            p.drawRect(QRectF(c.x() - r, c.y() - r, 2 * r, r * 0.22));
            p.drawRect(QRectF(c.x() - r, c.y() + r * 0.82, 2 * r, r * 0.2));
            break;
        }
        case Surface::Smooth: {
            QLinearGradient sheen(c + QPointF(-r, -r), c + QPointF(r, r));
            sheen.setColorAt(0, withAlpha(style.accent, 0));
            sheen.setColorAt(0.5, withAlpha(style.accent, 70));
            sheen.setColorAt(1, withAlpha(style.accent, 0));
            p.setBrush(sheen);
            p.drawEllipse(c, r, r);
            break;
        }
    }
    p.restore();
}

}  // namespace

SurfaceStyle styleForKey(const QString& key, bool moon)
{
    const auto hash = stableHash(key);
    SurfaceStyle style;
    style.seed = hash;
    const int hue = static_cast<int>(hash % 360U);
    if (moon) {
        const qreal sat = 0.06 + ((hash >> 9U) % 100U) / 100.0 * 0.2;
        const qreal val = 0.68 + ((hash >> 17U) % 100U) / 100.0 * 0.2;
        style.base = QColor::fromHsvF(hue / 360.0F, static_cast<float>(sat), static_cast<float>(val));
        style.accent = style.base.darker(135);
        style.surface = ((hash >> 5U) % 3U) == 0 ? Surface::Smooth : Surface::Rocky;
    } else {
        const qreal sat = 0.32 + ((hash >> 9U) % 100U) / 100.0 * 0.3;
        const qreal val = 0.62 + ((hash >> 17U) % 100U) / 100.0 * 0.28;
        style.base = QColor::fromHsvF(hue / 360.0F, static_cast<float>(sat), static_cast<float>(val));
        style.accent = QColor::fromHsvF(((hue + 25 + static_cast<int>((hash >> 3U) % 40U)) % 360) / 360.0F,
                                        static_cast<float>(std::min(0.8, sat + 0.15)), static_cast<float>(val * 0.8));
        switch ((hash >> 24U) % 5U) {
            case 0:
            case 1:
                style.surface = Surface::Banded;
                break;
            case 2:
            case 3:
                style.surface = Surface::Rocky;
                break;
            default:
                style.surface = Surface::Smooth;
                break;
        }
    }
    return style;
}

SurfaceStyle styleForMod(const QString& key, ModKind kind)
{
    SurfaceStyle style = styleForKey(key, false);  // keeps the surface type
    const auto hash = stableHash(key);
    // Cached sprites are keyed by the seed: a new kind must not reuse the old colors.
    style.seed = hash ^ (static_cast<std::uint32_t>(kind) * 0x9e3779b9U);
    // Independent hash bits for hue, saturation and brightness, so neighbours of one kind still differ.
    auto unit = [hash](unsigned shift) { return ((hash >> shift) % 1000U) / 1000.0; };
    const qreal val = 0.58 + unit(17U) * 0.35;  // never too dark or too bright
    if (kind == ModKind::Unknown) {
        const qreal sat = unit(9U) * 0.12;
        style.base = QColor::fromHsvF(static_cast<float>(unit(3U)), static_cast<float>(sat), static_cast<float>(val));
        style.accent = style.base.darker(135);
        return style;
    }
    // Each kind has a central hue and a reach; most planets stay near the centre, a few reach into the neighbouring
    // colors (a pinkish library, a teal optimization mod), so together the kinds cover every hue.
    struct Family {
        qreal centre;
        qreal reach;
    };
    Family family{};
    switch (kind) {
        case ModKind::Library:
            family = { 12, 48 };  // magenta and pink to red, orange and amber
            break;
        case ModKind::Utility:
            family = { 48, 24 };  // amber to gold and yellow
            break;
        case ModKind::Optimization:
            family = { 120, 58 };  // lime to green and teal
            break;
        case ModKind::Content:
            family = { 248, 80 };  // cyan to blue, purple and violet
            break;
        case ModKind::Unknown:
            break;
    }
    const qreal x = 2 * unit(0U) - 1;
    const qreal offset = family.reach * (x < 0 ? -1 : 1) * std::pow(std::abs(x), 1.5);
    const qreal hue = std::fmod(family.centre + offset + 360.0, 360.0);
    const qreal sat = 0.24 + unit(9U) * 0.32;  // muted, like real planets (not neon)
    style.base = QColor::fromHsvF(static_cast<float>(hue / 360.0), static_cast<float>(sat), static_cast<float>(val));
    // The accent stays in the same family, a little shifted and darker.
    const qreal accentHue = std::fmod(hue + (((hash >> 30U) & 1U) != 0U ? -12.0 : 12.0) + 360.0, 360.0);
    style.accent = QColor::fromHsvF(static_cast<float>(accentHue / 360.0), static_cast<float>(std::min(0.65, sat + 0.1)),
                                    static_cast<float>(val * 0.78));
    return style;
}

SurfaceStyle baseGameStyle()
{
    SurfaceStyle style;
    style.base = QColor(58, 112, 196);
    style.accent = QColor(98, 168, 72);
    style.surface = Surface::Terran;
    style.seed = 0x51ab3e7U;
    return style;
}

void drawSphere(QPainter& p, QPointF c, qreal r, const SurfaceStyle& style, QPointF light, qreal lightFacing, int quality, qreal opacity)
{
    if (r <= 0.2 || opacity <= 0.001) {
        return;
    }
    p.save();
    p.setOpacity(p.opacity() * opacity);
    p.setPen(Qt::NoPen);

    if (quality <= 0 || r < 2.5) {
        p.setBrush(style.base);
        p.drawEllipse(c, r, r);
        if (quality <= 0) {
            p.setPen(QPen(style.base.darker(160), 1));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, r, r);
        }
        p.restore();
        return;
    }

    const QPointF l = normalized(light);

    // Body color, brightest towards the light.
    QRadialGradient body(c, r, c + l * r * 0.45);
    body.setColorAt(0, style.base.lighter(150));
    body.setColorAt(0.4, style.base);
    body.setColorAt(1, style.base.darker(230));
    p.setBrush(body);
    p.drawEllipse(c, r, r);

    if (quality >= 2 && r >= 5) {
        drawSurfaceDetail(p, c, r, style);
    }

    // Night side: darker the more the lit hemisphere faces away from the viewer.
    const qreal facing = std::clamp(lightFacing, -1.0, 1.0);
    const int nightAlpha = static_cast<int>(130 + 95 * (1 - (facing + 1) / 2));
    QLinearGradient night(c + l * r, c - l * r);
    night.setColorAt(0, QColor(0, 0, 0, 0));
    night.setColorAt(std::clamp(0.42 + facing * 0.2, 0.15, 0.7), QColor(0, 0, 0, 0));
    night.setColorAt(1, QColor(4, 2, 16, nightAlpha));
    p.setBrush(night);
    p.drawEllipse(c, r, r);

    // Thin atmosphere rim on the lit side.
    QRadialGradient rim(c, r);
    rim.setColorAt(0.82, withAlpha(style.base.lighter(170), 0));
    rim.setColorAt(1.0, withAlpha(style.base.lighter(170), quality >= 2 ? 70 : 40));
    p.setBrush(rim);
    p.drawEllipse(c, r, r);

    p.restore();
}

void drawSun(QPainter& p, QPointF c, qreal r, const QColor& color, qreal intensity, int quality, qreal opacity)
{
    if (r <= 0.2 || opacity <= 0.001) {
        return;
    }
    p.save();
    p.setOpacity(p.opacity() * opacity);
    p.setPen(Qt::NoPen);

    const qreal boost = std::clamp(intensity - 1.0, 0.0, 2.0);
    const QColor hot = mix(color, Qt::white, std::min(1.0, 0.25 + boost * 0.35));

    if (quality >= 1) {
        const qreal glowRadius = r * (2.7 + boost * 1.2);
        QRadialGradient corona(c, glowRadius);
        corona.setColorAt(0, withAlpha(hot, static_cast<int>(150 + boost * 50)));
        corona.setColorAt(0.33, withAlpha(color, static_cast<int>(70 + boost * 40)));
        corona.setColorAt(0.6, withAlpha(color, static_cast<int>(18 + boost * 20)));
        corona.setColorAt(1, withAlpha(color, 0));
        p.setBrush(corona);
        p.drawEllipse(c, glowRadius, glowRadius);
    }

    QRadialGradient core(c, r, c + QPointF(-r * 0.25, -r * 0.3));
    core.setColorAt(0, mix(hot, Qt::white, 0.6));
    core.setColorAt(0.55, hot);
    core.setColorAt(1, color.darker(quality >= 1 ? 118 : 100));
    p.setBrush(quality >= 1 ? QBrush(core) : QBrush(color));
    p.drawEllipse(c, r, r);

    if (quality >= 2 && r > 8) {
        // Faint granulation so the surface does not look like a flat disc.
        DetailRandom rnd(stableHash(color.name()));
        QPainterPath clip;
        clip.addEllipse(c, r, r);
        p.setClipPath(clip);
        for (int i = 0; i < 9; i++) {
            const qreal angle = rnd.next() * 2 * M_PI;
            const qreal dist = std::sqrt(rnd.next()) * r * 0.8;
            const qreal cr = r * (0.12 + rnd.next() * 0.18);
            p.setBrush(withAlpha(mix(hot, Qt::white, 0.5), 28));
            p.drawEllipse(c + QPointF(std::cos(angle), std::sin(angle)) * dist, cr, cr);
        }
    }
    p.restore();
}

void drawShadow(QPainter& p, QPointF c, qreal rx, qreal ry, qreal opacity)
{
    if (rx <= 0.5 || ry <= 0.3 || opacity <= 0.001) {
        return;
    }
    p.save();
    p.setPen(Qt::NoPen);
    p.translate(c);
    p.scale(1.0, ry / rx);
    QRadialGradient shadow(QPointF(0, 0), rx);
    shadow.setColorAt(0, QColor(0, 0, 8, static_cast<int>(110 * opacity)));
    shadow.setColorAt(1, QColor(0, 0, 8, 0));
    p.setBrush(shadow);
    p.drawEllipse(QPointF(0, 0), rx, rx);
    p.restore();
}

void drawRingHalf(QPainter& p,
                  QPointF c,
                  qreal rx,
                  qreal ry,
                  qreal width,
                  qreal rotationDeg,
                  const QColor& color,
                  bool front,
                  int quality,
                  qreal opacity,
                  bool dashed)
{
    if (rx <= 0.5 || opacity <= 0.001) {
        return;
    }
    p.save();
    p.setOpacity(p.opacity() * opacity);
    p.translate(c);
    p.rotate(rotationDeg);
    p.setBrush(Qt::NoBrush);
    const QRectF rect(-rx, -ry, 2 * rx, 2 * ry);
    // Qt angles: counter-clockwise from 3 o'clock in 1/16 degrees. The upper half is the far side.
    const int start = front ? 180 * 16 : 0;
    const int span = 180 * 16;
    const qreal w = std::max<qreal>(width, 1.0);

    QPen pen(withAlpha(color, front ? 200 : 120), w, Qt::SolidLine, Qt::FlatCap);
    if (dashed) {
        // Dash lengths are in pen widths; about 7 px dashes with 5 px gaps whatever the band's width.
        const qreal unit = std::max<qreal>(w, 1.0);
        pen.setDashPattern({ 7.0 / unit, 5.0 / unit });
    }
    p.setPen(pen);
    p.drawArc(rect, start, span);
    if (quality >= 2 && w >= 3 && !dashed) {
        // A brighter inner lane and a dark division give the band some structure.
        QPen lane(withAlpha(color.lighter(135), front ? 170 : 95), w * 0.3, Qt::SolidLine, Qt::FlatCap);
        p.setPen(lane);
        p.drawArc(rect, start, span);
        const qreal gap = w * 0.25;
        QPen division(QColor(6, 4, 20, front ? 110 : 70), std::max<qreal>(1.0, w * 0.08), Qt::SolidLine, Qt::FlatCap);
        p.setPen(division);
        p.drawArc(QRectF(-rx - gap, -ry - gap * ry / rx, 2 * (rx + gap), 2 * (ry + gap * ry / rx)), start, span);
    }
    p.restore();
}

void drawBlackHole(QPainter& p, QPointF c, qreal r, qreal swirl, int quality, qreal opacity)
{
    if (r <= 0.5 || opacity <= 0.001) {
        return;
    }
    p.save();
    p.setOpacity(p.opacity() * opacity);
    p.setBrush(Qt::NoBrush);

    if (quality >= 1) {
        QRadialGradient halo(c, r * 4.2);
        halo.setColorAt(0, QColor(150, 90, 255, 70));
        halo.setColorAt(0.45, QColor(110, 60, 220, 28));
        halo.setColorAt(1, QColor(80, 40, 180, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(halo);
        p.drawEllipse(c, r * 4.2, r * 4.2);
        p.setBrush(Qt::NoBrush);
    }

    // The accretion disk lies in the galaxy plane, seen at the same 45 degree tilt as everything else.
    const qreal rx = r * 2.7;
    const qreal ry = rx * 0.7071 * 0.62;
    const QRectF disk(-rx, -ry, 2 * rx, 2 * ry);
    QConicalGradient hot(QPointF(0, 0), swirl);
    hot.setColorAt(0.0, QColor(255, 180, 100));
    hot.setColorAt(0.25, QColor(255, 110, 170));
    hot.setColorAt(0.5, QColor(160, 100, 255));
    hot.setColorAt(0.75, QColor(255, 140, 120));
    hot.setColorAt(1.0, QColor(255, 180, 100));

    auto diskHalf = [&](bool front) {
        p.save();
        p.translate(c);
        p.rotate(-8);
        const int start = front ? 180 * 16 : 0;
        if (quality >= 1) {
            p.setOpacity(p.opacity() * (front ? 0.9 : 0.55));
            p.setPen(QPen(QBrush(hot), r * 0.95, Qt::SolidLine, Qt::FlatCap));
            p.drawArc(disk, start, 180 * 16);
            if (quality >= 2) {
                p.setPen(QPen(QColor(255, 240, 220, front ? 150 : 80), std::max(1.0, r * 0.18), Qt::SolidLine, Qt::FlatCap));
                p.drawArc(disk.adjusted(r * 0.25, r * 0.25 * 0.44, -r * 0.25, -r * 0.25 * 0.44), start, 180 * 16);
            }
        } else {
            p.setPen(QPen(QColor(220, 150, 200), std::max(1.0, r * 0.4)));
            p.drawArc(disk, start, 180 * 16);
        }
        p.restore();
    };

    diskHalf(false);

    if (quality >= 1) {
        // Light from the far side of the disk, bent over the top of the hole.
        p.save();
        p.setPen(QPen(QBrush(hot), std::max(1.0, r * 0.22), Qt::SolidLine, Qt::RoundCap));
        p.setOpacity(p.opacity() * 0.75);
        p.drawArc(QRectF(c.x() - r * 1.35, c.y() - r * 1.3, r * 2.7, r * 2.6), 15 * 16, 150 * 16);
        p.restore();
    }

    // Event horizon and photon ring.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(2, 1, 6));
    p.drawEllipse(c, r, r);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 226, 200, 230), std::max(1.2, r * 0.07)));
    p.drawEllipse(c, r * 1.04, r * 1.04);

    diskHalf(true);
    p.restore();
}

void drawNebula(QPainter& p, QPointF c, qreal radius, const QColor& color, qreal time, std::uint32_t seed, int quality, qreal opacity)
{
    if (radius <= 1 || opacity <= 0.001) {
        return;
    }
    p.save();
    p.setOpacity(p.opacity() * opacity);
    p.setPen(Qt::NoPen);
    if (quality <= 0) {
        QRadialGradient cloud(c, radius);
        cloud.setColorAt(0, withAlpha(color, 110));
        cloud.setColorAt(1, withAlpha(color, 0));
        p.setBrush(cloud);
        p.drawEllipse(c, radius, radius * 0.75);
        p.restore();
        return;
    }
    p.setCompositionMode(QPainter::CompositionMode_Screen);
    const QColor purple(150, 90, 255);
    const QColor other = QColor::fromHsv((color.hsvHue() + 150) % 360, std::max(80, color.hsvSaturation()), 230);
    DetailRandom rnd(seed);
    const int blobs = quality >= 2 ? 9 : 5;
    for (int i = 0; i < blobs; i++) {
        const qreal distance = radius * (0.1 + 0.6 * rnd.next());
        const qreal direction = (i % 2 ? 1.0 : -1.0) * (0.025 + 0.02 * rnd.next());
        const qreal angle = rnd.next() * 2 * M_PI + time * direction;
        const qreal size = radius * (0.35 + 0.4 * rnd.next()) * (1 + 0.06 * std::sin(time * 0.5 + i));
        const QPointF at = c + QPointF(std::cos(angle) * distance, std::sin(angle) * distance * 0.72);
        const QColor tint = i % 3 == 0 ? color : (i % 3 == 1 ? purple : other);
        QRadialGradient cloud(at, size);
        cloud.setColorAt(0, withAlpha(tint, 70));
        cloud.setColorAt(0.5, withAlpha(tint, 30));
        cloud.setColorAt(1, withAlpha(tint, 0));
        p.setBrush(cloud);
        p.drawEllipse(at, size, size * 0.8);
    }
    // What is left of the sun: a faint, warm core.
    QRadialGradient core(c, radius * 0.35);
    core.setColorAt(0, withAlpha(mix(color, Qt::white, 0.5), 120));
    core.setColorAt(1, withAlpha(color, 0));
    p.setBrush(core);
    p.drawEllipse(c, radius * 0.35, radius * 0.35);
    p.restore();
}

void drawSparkle(QPainter& p, QPointF c, qreal size, const QColor& color, qreal twinkle, qreal opacity)
{
    if (opacity <= 0.001) {
        return;
    }
    p.save();
    p.setOpacity(p.opacity() * opacity);
    const QColor bright = mix(color, Qt::white, 0.55);
    QRadialGradient glow(c, size * 3.2);
    glow.setColorAt(0, withAlpha(bright, static_cast<int>(120 + 80 * twinkle)));
    glow.setColorAt(1, withAlpha(color, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(c, size * 3.2, size * 3.2);
    const qreal arm = size * (2.2 + 1.8 * twinkle);
    p.setPen(QPen(withAlpha(bright, static_cast<int>(150 + 100 * twinkle)), std::max(1.0, size * 0.35), Qt::SolidLine, Qt::RoundCap));
    p.drawLine(c - QPointF(arm, 0), c + QPointF(arm, 0));
    p.drawLine(c - QPointF(0, arm), c + QPointF(0, arm));
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawEllipse(c, size * 0.55, size * 0.55);
    p.restore();
}

void drawOrbit(QPainter& p, QPointF c, qreal rx, qreal ry, const QColor& color, qreal opacity)
{
    if (rx <= 1 || opacity <= 0.001) {
        return;
    }
    p.save();
    QPen pen(withAlpha(color, static_cast<int>(255 * opacity)), 1.0);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, rx, ry);
    p.restore();
}

QPixmap renderStarfield(const QSize& size, qreal dpr, int quality)
{
    QPixmap pixmap(size * dpr);
    pixmap.setDevicePixelRatio(dpr);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF rect(QPointF(0, 0), QSizeF(size));
    QLinearGradient sky(rect.topLeft(), rect.bottomRight());
    sky.setColorAt(0, QColor(9, 8, 26));
    sky.setColorAt(0.55, QColor(13, 9, 32));
    sky.setColorAt(1, QColor(20, 10, 40));
    p.fillRect(rect, sky);

    if (quality <= 0) {
        return pixmap;
    }

    // Fixed seed: the sky must not reshuffle on every resize.
    QRandomGenerator rng(0x6a1a5eedU);
    p.setPen(Qt::NoPen);

    // Faint nebulae.
    const QColor nebulae[] = { QColor(110, 60, 200), QColor(60, 80, 190), QColor(160, 70, 170) };
    const int nebulaCount = quality >= 2 ? 3 : 2;
    for (int i = 0; i < nebulaCount; i++) {
        const QPointF at(rng.bounded(rect.width()), rng.bounded(rect.height()));
        const qreal radius = std::max(rect.width(), rect.height()) * (0.25 + rng.bounded(0.25));
        QRadialGradient nebula(at, radius);
        nebula.setColorAt(0, withAlpha(nebulae[i % 3], quality >= 2 ? 34 : 24));
        nebula.setColorAt(1, withAlpha(nebulae[i % 3], 0));
        p.setBrush(nebula);
        p.drawEllipse(at, radius, radius);
    }

    // Stars: density scales with area, so large windows are not sparser.
    const int count = static_cast<int>(rect.width() * rect.height() / (quality >= 2 ? 2600.0 : 6500.0));
    for (int i = 0; i < count; i++) {
        const QPointF at(rng.bounded(rect.width()), rng.bounded(rect.height()));
        const qreal brightness = std::pow(rng.bounded(1.0), 2.2);
        const qreal radius = 0.35 + brightness * 1.1;
        const int tint = static_cast<int>(rng.bounded(3U));
        QColor star = tint == 0 ? QColor(255, 244, 228) : tint == 1 ? QColor(220, 228, 255) : QColor(255, 255, 255);
        star.setAlpha(static_cast<int>(60 + brightness * 170));
        p.setBrush(star);
        p.drawEllipse(at, radius, radius);
        if (quality >= 2 && brightness > 0.8) {
            QRadialGradient halo(at, radius * 4);
            halo.setColorAt(0, withAlpha(star, 50));
            halo.setColorAt(1, withAlpha(star, 0));
            p.setBrush(halo);
            p.drawEllipse(at, radius * 4, radius * 4);
        }
    }
    return pixmap;
}

QPixmap renderSkyBase(const QSize& size, int margin, qreal dpr, int quality)
{
    const QSize full = size + QSize(2 * margin, 2 * margin);
    QPixmap pixmap(full * dpr);
    pixmap.setDevicePixelRatio(dpr);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF rect(QPointF(0, 0), QSizeF(full));
    QLinearGradient sky(rect.topLeft(), rect.bottomRight());
    sky.setColorAt(0, QColor(9, 8, 26));
    sky.setColorAt(0.55, QColor(13, 9, 32));
    sky.setColorAt(1, QColor(20, 10, 40));
    p.fillRect(rect, sky);
    if (quality <= 0) {
        return pixmap;
    }

    QRandomGenerator rng(0x6a1a5eedU);
    p.setPen(Qt::NoPen);
    const QColor nebulae[] = { QColor(110, 60, 200), QColor(60, 80, 190), QColor(160, 70, 170) };
    const int nebulaCount = quality >= 2 ? 3 : 2;
    for (int i = 0; i < nebulaCount; i++) {
        const QPointF at(rng.bounded(rect.width()), rng.bounded(rect.height()));
        const qreal radius = std::max(rect.width(), rect.height()) * (0.25 + rng.bounded(0.25));
        QRadialGradient nebula(at, radius);
        nebula.setColorAt(0, withAlpha(nebulae[i % 3], quality >= 2 ? 34 : 24));
        nebula.setColorAt(1, withAlpha(nebulae[i % 3], 0));
        p.setBrush(nebula);
        p.drawEllipse(at, radius, radius);
    }
    return pixmap;
}

QPixmap renderStarTile(int tileSize, int layer, qreal dpr, int quality)
{
    QPixmap pixmap(QSize(tileSize, tileSize) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    if (quality <= 0) {
        return pixmap;
    }
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);

    QRandomGenerator rng(layer == 0 ? 0x51a7f1e1U : 0x7e4be7a2U);
    const qreal area = static_cast<qreal>(tileSize) * tileSize;
    const int count = layer == 0 ? static_cast<int>(area / (quality >= 2 ? 2400.0 : 5200.0)) : static_cast<int>(area / 14000.0);
    for (int i = 0; i < count; i++) {
        const QPointF at(rng.bounded(static_cast<qreal>(tileSize)), rng.bounded(static_cast<qreal>(tileSize)));
        qreal brightness = std::pow(rng.bounded(1.0), 2.2);
        if (layer == 0) {
            brightness *= 0.75;
        } else {
            brightness = 0.45 + 0.55 * brightness;
        }
        const qreal radius = (layer == 0 ? 0.3 : 0.6) + brightness * (layer == 0 ? 0.9 : 1.2);
        const int tint = static_cast<int>(rng.bounded(3U));
        QColor star = tint == 0 ? QColor(255, 244, 228) : tint == 1 ? QColor(220, 228, 255) : QColor(255, 255, 255);
        star.setAlpha(static_cast<int>(50 + brightness * 180));
        // Draw wrapped copies so the tile repeats without seams.
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                const QPointF c = at + QPointF(dx * tileSize, dy * tileSize);
                if (c.x() < -8 || c.y() < -8 || c.x() > tileSize + 8 || c.y() > tileSize + 8) {
                    continue;
                }
                if (layer == 1 && quality >= 2 && brightness > 0.7) {
                    QRadialGradient halo(c, radius * 4);
                    halo.setColorAt(0, withAlpha(star, 50));
                    halo.setColorAt(1, withAlpha(star, 0));
                    p.setBrush(halo);
                    p.drawEllipse(c, radius * 4, radius * 4);
                }
                p.setBrush(star);
                p.drawEllipse(c, radius, radius);
            }
        }
    }
    return pixmap;
}

}  // namespace CelestialPainter
