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

#include "OrbitPainter.h"

#include <QHash>
#include <QImage>
#include <QPainter>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <functional>

#include "GalaxyLook.h"

namespace OrbitPainter {

namespace {

namespace O = GalaxyLook::Orbit;

/** 4x4 ordered dither threshold in [0, 1): softens tone boundaries the way pixel art does. */
qreal bayer(int x, int y)
{
    static constexpr int matrix[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    return matrix[y & 3][x & 3] / 16.0;
}

QColor mix(const QColor& a, const QColor& b, qreal t)
{
    return QColor::fromRgbF(static_cast<float>(a.redF() + (b.redF() - a.redF()) * t),
                            static_cast<float>(a.greenF() + (b.greenF() - a.greenF()) * t),
                            static_cast<float>(a.blueF() + (b.blueF() - a.blueF()) * t),
                            static_cast<float>(a.alphaF() + (b.alphaF() - a.alphaF()) * t));
}

QColor withAlpha(QColor color, int alpha)
{
    color.setAlpha(std::clamp(alpha, 0, 255));
    return color;
}

QRgb premultiplied(const QColor& color)
{
    return qPremultiply(color.rgba());
}

std::uint32_t hash3(int x, int y, std::uint32_t seed)
{
    auto h = static_cast<std::uint32_t>(x) * 374761393U + static_cast<std::uint32_t>(y) * 668265263U + seed * 2246822519U;
    h = (h ^ (h >> 13U)) * 1274126177U;
    return h ^ (h >> 16U);
}

/** Smooth value noise in [0, 1]. */
qreal valueNoise(qreal x, qreal y, std::uint32_t seed)
{
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const qreal fx = x - x0;
    const qreal fy = y - y0;
    auto v = [seed](int ix, int iy) { return (hash3(ix, iy, seed) % 10000U) / 10000.0; };
    const qreal sx = fx * fx * (3 - 2 * fx);
    const qreal sy = fy * fy * (3 - 2 * fy);
    const qreal a = v(x0, y0) + (v(x0 + 1, y0) - v(x0, y0)) * sx;
    const qreal b = v(x0, y0 + 1) + (v(x0 + 1, y0 + 1) - v(x0, y0 + 1)) * sx;
    return a + (b - a) * sy;
}

/** Draws a cell image scaled up by @p cell logical pixels, its top left on whole pixels. */
void blit(QPainter& p, const QImage& cells, QPointF topLeft, qreal cell, qreal opacity)
{
    if (cells.isNull() || opacity <= 0.001) {
        return;
    }
    p.save();
    p.setOpacity(p.opacity() * opacity);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.drawImage(QRectF(QPointF(std::round(topLeft.x()), std::round(topLeft.y())), QSizeF(cells.width() * cell, cells.height() * cell)), cells);
    p.restore();
}

/** True when the cell (i, j) of an n-cell grid is inside the blocky disc (the shape of Orbit's icon). */
bool inDisc(int i, int j, int n, qreal* u = nullptr, qreal* v = nullptr)
{
    const qreal half = n / 2.0;
    const qreal x = (i + 0.5 - half) / half;
    const qreal y = (j + 0.5 - half) / half;
    if (u) {
        *u = x;
    }
    if (v) {
        *v = y;
    }
    return x * x + y * y <= 1.0 + 0.6 / std::max(1, n);
}

/**
 * Rasterizes a band along an ellipse (rx, ry, rotated by @p rotationDeg) into cells: a ring, an accretion disk, an
 * arc. @p half: 1 only the near (lower) half, -1 only the far half, 0 both. @p color gets the angle on the ellipse
 * (radians), where the cell is across the band (-1 inner edge to 1 outer edge) and the cell; an invalid color
 * leaves the cell empty.
 */
struct Band {
    QImage image;
    QPointF topLeft;  ///< Relative to the centre.
};
Band rasterBand(qreal rx,
                qreal ry,
                qreal width,
                qreal rotationDeg,
                int half,
                int cell,
                const std::function<QColor(qreal angle, qreal across, int i, int j)>& color)
{
    const qreal theta = qDegreesToRadians(rotationDeg);
    const qreal cs = std::cos(theta);
    const qreal sn = std::sin(theta);
    const qreal halfWidth = std::max(width / 2.0, cell * 0.55);
    const qreal ex = std::hypot(rx * cs, ry * sn) + halfWidth + cell;
    const qreal ey = std::hypot(rx * sn, ry * cs) + halfWidth + cell;
    const int w = static_cast<int>(std::ceil(2 * ex / cell));
    const int h = static_cast<int>(std::ceil(2 * ey / cell));
    Band band;
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) {
        return band;
    }
    band.image = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    band.image.fill(Qt::transparent);
    band.topLeft = QPointF(-w * cell / 2.0, -h * cell / 2.0);
    for (int j = 0; j < h; j++) {
        auto* line = reinterpret_cast<QRgb*>(band.image.scanLine(j));
        const qreal y = (j + 0.5) * cell + band.topLeft.y();
        for (int i = 0; i < w; i++) {
            const qreal x = (i + 0.5) * cell + band.topLeft.x();
            // Into the ellipse's own frame.
            const qreal xr = x * cs + y * sn;
            const qreal yr = -x * sn + y * cs;
            if ((half > 0 && yr < 0) || (half < 0 && yr >= 0)) {
                continue;
            }
            const qreal d = std::hypot(xr / rx, yr / ry);
            if (d < 1e-6) {
                continue;
            }
            // Approximate distance to the ellipse along the ray from the centre.
            const qreal distance = (d - 1) * std::hypot(xr, yr) / d;
            if (std::abs(distance) > halfWidth) {
                continue;
            }
            const QColor c = color(std::atan2(yr / ry, xr / rx), distance / halfWidth, i, j);
            if (c.isValid() && c.alpha() > 0) {
                line[i] = premultiplied(c);
            }
        }
    }
    return band;
}

/** A small cache for rasterized rings: their shape only changes with the zoom and the hover. */
QHash<quint64, Band>& ringCache()
{
    static QHash<quint64, Band> cache;
    return cache;
}

quint64 mixKey(quint64 key, quint64 value)
{
    key ^= value + 0x9e3779b97f4a7c15ULL + (key << 6U) + (key >> 2U);
    return key;
}

}  // namespace

int cellFor(qreal r)
{
    return std::clamp(static_cast<int>(std::lround(2 * r / 16.0)), 1, 12);
}

void drawSphere(QPainter& p, QPointF c, qreal r, const SurfaceStyle& style, QPointF light, qreal lightFacing, int quality, qreal opacity)
{
    if (r <= 0.2 || opacity <= 0.001) {
        return;
    }
    const int cell = cellFor(r);
    const int n = std::max(2, static_cast<int>(std::lround(2 * r / cell)));

    // Light in 3D: towards the light on screen, and towards the viewer as much as the lit side faces them.
    qreal lx = light.x();
    qreal ly = light.y();
    if (const qreal len = std::hypot(lx, ly); len > 1e-6) {
        lx /= len;
        ly /= len;
    } else {
        lx = -0.6;
        ly = -0.8;
    }
    const qreal lz = std::clamp(lightFacing, -1.0, 1.0);
    const qreal k = std::sqrt(std::max(0.0, 1 - lz * lz));
    lx *= k;
    ly *= k;

    const QColor base = style.base;
    const QColor accent = style.accent.isValid() ? style.accent : base;
    const std::uint32_t seed = style.seed;

    // Craters of a rocky body, in the unit disc.
    struct Crater {
        qreal x, y, r;
    };
    QList<Crater> craters;
    if (style.surface == CelestialPainter::Surface::Rocky && quality >= 2 && n >= 7) {
        const int count = 3 + static_cast<int>(seed % 3U);
        for (int i = 0; i < count; i++) {
            const auto h = hash3(i, 7, seed);
            craters.append({ ((h % 1000U) / 1000.0 - 0.5) * 1.3, (((h >> 10U) % 1000U) / 1000.0 - 0.5) * 1.3,
                             0.13 + ((h >> 20U) % 100U) / 100.0 * 0.14 });
        }
    }
    const int bands = 4 + static_cast<int>(seed % 3U);
    const qreal bandShift = (seed % 97U) / 97.0;

    QImage image(n, n, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    for (int j = 0; j < n; j++) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(j));
        for (int i = 0; i < n; i++) {
            qreal u = 0;
            qreal v = 0;
            if (!inDisc(i, j, n, &u, &v)) {
                continue;
            }
            if (quality <= 0 || n <= 3) {
                line[i] = premultiplied(base);
                continue;
            }
            // Surface color first, independent of the light.
            QColor surface = base;
            if (quality >= 2) {
                switch (style.surface) {
                    case CelestialPainter::Surface::Banded: {
                        const qreal wobble = 0.08 * std::sin(u * 3.1 + seed % 13U);
                        const int b = static_cast<int>(std::floor((v * 0.5 + 0.5 + wobble) * bands + bandShift));
                        surface = b % 3 == 0 ? mix(base, accent, 0.65) : (b % 3 == 1 ? base : base.lighter(112));
                        break;
                    }
                    case CelestialPainter::Surface::Rocky:
                        for (const auto& crater : craters) {
                            const qreal d = std::hypot(u - crater.x, v - crater.y);
                            if (d < crater.r) {
                                surface = base.darker(135);
                            } else if (d < crater.r + 0.12 && (u - crater.x) + (v - crater.y) > 0) {
                                surface = base.lighter(112);
                            }
                        }
                        break;
                    case CelestialPainter::Surface::Terran:
                        if (std::abs(v) > 0.78) {
                            surface = QColor(236, 240, 246);  // ice caps
                        } else if (valueNoise(u * 2.6 + 5, v * 2.6 + 3, seed) > 0.56) {
                            surface = accent;
                        }
                        break;
                    case CelestialPainter::Surface::Smooth:
                        if (n >= 10 && std::abs(v - 0.1) < 0.09) {
                            surface = mix(base, accent, 0.35);
                        }
                        break;
                }
            }
            // Lit sphere, quantized to four tones with ordered dithering at the boundaries.
            const qreal w = std::sqrt(std::max(0.0, 1 - u * u - v * v));
            const qreal lambert = u * lx + v * ly + w * lz + (bayer(i, j) - 0.5) * 0.22;
            QColor tone;
            if (lambert > 0.62) {
                tone = mix(surface.lighter(128), Qt::white, 0.12);
            } else if (lambert > 0.12) {
                tone = surface;
            } else if (lambert > -0.25) {
                tone = surface.darker(165);
            } else {
                tone = mix(surface.darker(330), O::Background, 0.35);
            }
            line[i] = premultiplied(tone);
        }
    }
    const qreal side = n * cell;
    blit(p, image, c - QPointF(side / 2.0, side / 2.0), cell, opacity);
}

void drawOutline(QPainter& p, QPointF c, qreal r, const QColor& color, int gap, int thickness, bool dashed)
{
    if (r <= 0.2 || color.alpha() == 0) {
        return;
    }
    const int cell = cellFor(r);
    const int n = std::max(2, static_cast<int>(std::lround(2 * r / cell)));
    const int border = gap + thickness + 1;
    const int m = n + 2 * border;  // same centre and grid as the sphere's image
    const qreal inner = n / 2.0 + gap;
    const qreal outer = inner + thickness;
    QImage image(m, m, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const QRgb pixel = premultiplied(color);
    for (int j = 0; j < m; j++) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(j));
        for (int i = 0; i < m; i++) {
            const qreal x = i + 0.5 - m / 2.0;
            const qreal y = j + 0.5 - m / 2.0;
            const qreal d = std::hypot(x, y);
            if (d < inner || d >= outer) {
                continue;
            }
            if (dashed) {
                // Two cells on, two off, along the ring.
                const qreal along = (std::atan2(y, x) + M_PI) * (inner + thickness / 2.0);
                if (static_cast<int>(along / 2.0) % 2 == 1) {
                    continue;
                }
            }
            line[i] = pixel;
        }
    }
    const qreal side = m * cell;
    blit(p, image, c - QPointF(side / 2.0, side / 2.0), cell, 1.0);
}

void drawSun(QPainter& p, QPointF c, qreal r, const QColor& color, qreal intensity, int quality, qreal opacity)
{
    if (r <= 0.2 || opacity <= 0.001) {
        return;
    }
    const int cell = cellFor(r);
    const int n = std::max(2, static_cast<int>(std::lround(2 * r / cell)));
    const qreal boost = std::clamp(intensity - 1.0, 0.0, 2.0);
    const QColor hot = mix(color, Qt::white, std::min(1.0, 0.25 + boost * 0.35));
    const qreal reach = quality >= 1 ? 1.75 + boost * 0.45 : 1.0;  // the corona, in sun radii
    const int extra = static_cast<int>(std::ceil(n / 2.0 * (reach - 1))) + 1;
    const int m = n + 2 * extra;
    QImage image(m, m, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const qreal half = n / 2.0;
    for (int j = 0; j < m; j++) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(j));
        for (int i = 0; i < m; i++) {
            const int di = i - extra;
            const int dj = j - extra;
            qreal u = 0;
            qreal v = 0;
            if (di >= 0 && dj >= 0 && di < n && dj < n && inDisc(di, dj, n, &u, &v)) {
                const qreal d = std::hypot(u, v);
                // Brightest towards the upper left, a darker rim at the lower right.
                const qreal shade = d + 0.25 * (u + v) + (bayer(i, j) - 0.5) * 0.18;
                QColor tone = shade < 0.35 ? mix(hot, Qt::white, 0.6) : shade < 0.8 ? hot : shade < 1.12 ? color : color.darker(125);
                line[i] = premultiplied(quality >= 1 ? tone : color);
                continue;
            }
            if (quality < 1) {
                continue;
            }
            // Corona: a dense ring of pixels, then a sparse one, and four rays.
            const qreal x = (di + 0.5 - half) / half;
            const qreal y = (dj + 0.5 - half) / half;
            const qreal d = std::hypot(x, y);
            const bool ray = (std::abs(x) < 1.0 / half || std::abs(y) < 1.0 / half) && d < reach + 0.2;
            if (d < 1.0 + (reach - 1) * 0.45 && (i + j) % 2 == 0) {
                line[i] = premultiplied(withAlpha(hot, static_cast<int>(150 + boost * 50)));
            } else if (d < reach && i % 2 == 0 && j % 2 == 0) {
                line[i] = premultiplied(withAlpha(color, static_cast<int>(85 + boost * 50)));
            } else if (ray) {
                line[i] = premultiplied(withAlpha(hot, static_cast<int>(110 + boost * 60)));
            }
        }
    }
    const qreal side = m * cell;
    blit(p, image, c - QPointF(side / 2.0, side / 2.0), cell, opacity);
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
    // The planet inside is about rx / 1.9: the same cell size keeps the ring and its planet one piece of pixel art.
    const int cell = std::clamp(static_cast<int>(std::lround(rx / 15.0)), 1, 12);
    quint64 key = 1469598103934665603ULL;
    for (const quint64 value : { static_cast<quint64>(std::lround(rx * 2)), static_cast<quint64>(std::lround(ry * 2)),
                                 static_cast<quint64>(std::lround(width * 2)), static_cast<quint64>(std::lround(rotationDeg * 4) + 4000),
                                 static_cast<quint64>(color.rgba()), static_cast<quint64>(front), static_cast<quint64>(quality),
                                 static_cast<quint64>(dashed) }) {
        key = mixKey(key, value);
    }
    auto& cache = ringCache();
    auto it = cache.constFind(key);
    if (it == cache.constEnd()) {
        if (cache.size() > 400) {
            cache.clear();
        }
        const int cellsAcross = std::max(1, static_cast<int>(std::lround(std::max(width, 1.0) / cell)));
        const QColor main = withAlpha(color, front ? 235 : 150);
        const QColor edge = withAlpha(color.darker(135), front ? 235 : 150);
        const QColor lane = withAlpha(mix(color, Qt::white, 0.5), front ? 245 : 165);
        const qreal mean = (rx + ry) / 2;
        auto shade = [&](qreal angle, qreal across, int i, int j) -> QColor {
            if (dashed) {
                // About 7 px on and 5 px off along the ring, whatever its size.
                const qreal along = std::fmod(angle * mean + 1000 * 12, 12.0);
                if (along > 7) {
                    return {};
                }
                return withAlpha(color, front ? 190 : 120);
            }
            if (quality >= 2 && cellsAcross >= 3) {
                if (across > 0.45) {
                    return edge;  // the outer edge is a shade darker, like the logo's ring
                }
                if (std::abs(across + 0.1) < 0.25) {
                    return lane;
                }
            }
            // A few darker pixels along the ring break up the flat band.
            if (quality >= 2 && hash3(i, j, 99U) % 9U == 0) {
                return edge;
            }
            return main;
        };
        it = cache.insert(key, rasterBand(rx, ry, width, rotationDeg, front ? 1 : -1, cell, shade));
    }
    blit(p, it->image, c + it->topLeft, cell, opacity);
}

void drawBlackHole(QPainter& p, QPointF c, qreal r, qreal swirl, int quality, qreal opacity)
{
    if (r <= 0.5 || opacity <= 0.001) {
        return;
    }
    const int cell = std::clamp(static_cast<int>(std::lround(r / 4.0)), 1, 10);
    const qreal rx = r * 2.7;
    const qreal ry = rx * 0.7071 * 0.62;
    // The disk's colors turn in steps: red, light red, white hot, red, dark red.
    const QColor palette[8] = { O::Red, O::Red, O::ErrorText, O::EdgeHot, O::ErrorText, O::Red, O::RedDark, O::RedDark };
    const qreal step = std::floor(swirl / 22.5) * 22.5;  // whole steps: the pattern moves pixel by pixel
    auto diskColor = [&](bool front) {
        return [&, front](qreal angle, qreal across, int, int) -> QColor {
            if (quality >= 2 && across < -0.45) {
                return withAlpha(O::EdgeHot, front ? 210 : 120);
            }
            const qreal deg = std::fmod(qRadiansToDegrees(angle) + step + 3600.0, 360.0);
            const int index = std::clamp(static_cast<int>(deg / 45.0), 0, 7);
            return withAlpha(palette[quality >= 1 ? index : 0], front ? 235 : 140);
        };
    };

    p.save();
    // A faint, sparse halo of dark red pixels instead of a glow.
    if (quality >= 1) {
        const int hn = static_cast<int>(std::ceil(r * 4.2 * 2 / cell));
        QImage halo(hn, hn, QImage::Format_ARGB32_Premultiplied);
        halo.fill(Qt::transparent);
        for (int j = 0; j < hn; j++) {
            auto* line = reinterpret_cast<QRgb*>(halo.scanLine(j));
            for (int i = 0; i < hn; i++) {
                const qreal x = (i + 0.5 - hn / 2.0) * cell / r;
                const qreal y = (j + 0.5 - hn / 2.0) * cell / r;
                const qreal d = std::hypot(x, y);
                if (d > 1.2 && d < 4.2 && bayer(i, j) < 0.4 * (1 - (d - 1.2) / 3.0)) {
                    line[i] = premultiplied(withAlpha(O::RedDark, 70));
                }
            }
        }
        blit(p, halo, c - QPointF(hn * cell / 2.0, hn * cell / 2.0), cell, opacity);
    }

    const Band back = rasterBand(rx, ry, r * 0.95, -8, -1, cell, diskColor(false));
    blit(p, back.image, c + back.topLeft, cell, opacity);

    if (quality >= 1) {
        // Light from the far side of the disk, bent over the top of the hole.
        const Band arc = rasterBand(r * 1.35, r * 1.3, std::max<qreal>(cell, r * 0.22), 0, -1, cell, [&](qreal angle, qreal, int, int) -> QColor {
            const qreal deg = -qRadiansToDegrees(angle);  // the upper half has negative angles
            if (deg < 15 || deg > 165) {
                return {};
            }
            return withAlpha(O::ErrorText, 190);
        });
        blit(p, arc.image, c + arc.topLeft, cell, opacity);
    }

    // Event horizon and its photon ring.
    const int n = std::max(3, static_cast<int>(std::lround(2 * r / cell)));
    const int m = n + 2;
    QImage hole(m, m, QImage::Format_ARGB32_Premultiplied);
    hole.fill(Qt::transparent);
    for (int j = 0; j < m; j++) {
        auto* line = reinterpret_cast<QRgb*>(hole.scanLine(j));
        for (int i = 0; i < m; i++) {
            if (i >= 1 && j >= 1 && i <= n && j <= n && inDisc(i - 1, j - 1, n)) {
                line[i] = qRgba(0, 0, 0, 255);
            } else if (inDisc(i, j, m)) {
                line[i] = premultiplied(O::EdgeHot);
            }
        }
    }
    blit(p, hole, c - QPointF(m * cell / 2.0, m * cell / 2.0), cell, opacity);

    const Band front = rasterBand(rx, ry, r * 0.95, -8, 1, cell, diskColor(true));
    blit(p, front.image, c + front.topLeft, cell, opacity);
    p.restore();
}

void drawNebula(QPainter& p, QPointF c, qreal radius, const QColor& color, qreal time, std::uint32_t seed, int quality, qreal opacity)
{
    if (radius <= 1 || opacity <= 0.001) {
        return;
    }
    const int cell = std::clamp(static_cast<int>(std::lround(radius / 36.0)), 2, 8);
    const int w = static_cast<int>(std::ceil(radius * 2.4 / cell));
    const int h = static_cast<int>(std::ceil(radius * 2.0 / cell));
    if (w <= 0 || h <= 0 || w > 2048 || h > 2048) {
        return;
    }
    struct Blob {
        QPointF at;
        qreal size;
        QColor tint;
    };
    // Gas in the sun's color, Orbit's dark red and grey.
    const QColor tints[3] = { color, O::RedDark, O::Faint };
    QList<Blob> blobs;
    std::uint32_t state = seed ? seed : 0x9e3779b9U;
    auto next = [&state] {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        return static_cast<qreal>(state % 100000U) / 100000.0;
    };
    const int count = quality >= 2 ? 9 : 5;
    for (int i = 0; i < count; i++) {
        const qreal distance = radius * (0.1 + 0.6 * next());
        const qreal direction = (i % 2 ? 1.0 : -1.0) * (0.025 + 0.02 * next());
        const qreal angle = next() * 2 * M_PI + time * direction;
        const qreal size = radius * (0.35 + 0.4 * next()) * (1 + 0.06 * std::sin(time * 0.5 + i));
        blobs.append({ QPointF(std::cos(angle) * distance, std::sin(angle) * distance * 0.72), size, tints[i % 3] });
    }
    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const QPointF origin(-w * cell / 2.0, -h * cell / 2.0);
    for (int j = 0; j < h; j++) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(j));
        const qreal y = origin.y() + (j + 0.5) * cell;
        for (int i = 0; i < w; i++) {
            const qreal x = origin.x() + (i + 0.5) * cell;
            qreal density = 0;
            qreal strongest = 0;
            QColor tint;
            for (const auto& blob : blobs) {
                const qreal d = std::hypot(x - blob.at.x(), (y - blob.at.y()) / 0.8) / blob.size;
                if (d >= 1) {
                    continue;
                }
                const qreal s = (1 - d) * (1 - d);
                density += s;
                if (s > strongest) {
                    strongest = s;
                    tint = blob.tint;
                }
            }
            // What is left of the sun: a warm core.
            const qreal core = std::hypot(x, y) / (radius * 0.3);
            if (core < 1) {
                density += 1.2 * (1 - core);
                if (core < 0.5) {
                    tint = mix(color, Qt::white, 0.5);
                }
            }
            // Three dithered density levels.
            const int level = std::min(3, static_cast<int>(density * 2.2 + bayer(i, j)));
            if (level <= 0 || !tint.isValid()) {
                continue;
            }
            line[i] = premultiplied(withAlpha(tint, level * 34));
        }
    }
    blit(p, image, c + origin, cell, opacity);
}

void drawSparkle(QPainter& p, QPointF c, qreal size, const QColor& color, qreal twinkle, qreal opacity)
{
    if (opacity <= 0.001) {
        return;
    }
    const int cell = std::max(1, static_cast<int>(std::lround(size * 0.6)));
    const int arm = 1 + static_cast<int>(std::lround(std::clamp(twinkle, 0.0, 1.0) * 2));
    const QColor bright = mix(color, Qt::white, 0.55);
    p.save();
    p.setOpacity(p.opacity() * opacity);
    p.setRenderHint(QPainter::Antialiasing, false);
    const QPointF at(std::round(c.x() - cell / 2.0), std::round(c.y() - cell / 2.0));
    for (int k = 1; k <= arm; k++) {
        const QColor tone = withAlpha(bright, static_cast<int>((150 + 100 * twinkle) * (k == arm && arm > 1 ? 0.55 : 1.0)));
        for (const QPoint d : { QPoint(k, 0), QPoint(-k, 0), QPoint(0, k), QPoint(0, -k) }) {
            p.fillRect(QRectF(at + QPointF(d.x() * cell, d.y() * cell), QSizeF(cell, cell)), tone);
        }
    }
    p.fillRect(QRectF(at, QSizeF(cell, cell)), Qt::white);
    p.restore();
}

void drawOrbit(QPainter& p, QPointF c, qreal rx, qreal ry, const QColor& color, qreal opacity)
{
    if (rx <= 1 || opacity <= 0.001) {
        return;
    }
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    // Dotted: two pixels on, three off.
    QPen pen(withAlpha(color, static_cast<int>(255 * std::min(1.0, opacity * 2.4))), 1.0, Qt::CustomDashLine, Qt::FlatCap);
    pen.setDashPattern({ 2, 3 });
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(std::round(c.x()), std::round(c.y())), std::round(rx), std::round(ry));
    p.restore();
}

QPixmap renderSkyBase(const QSize& size, int margin, qreal dpr)
{
    const QSize full = size + QSize(2 * margin, 2 * margin);
    QPixmap pixmap(full * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(O::Background);
    return pixmap;
}

QPixmap renderGridTile(int tileSize, int layer, qreal dpr, int quality)
{
    QPixmap pixmap(QSize(tileSize, tileSize) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    if (quality <= 0 && layer > 0) {
        return pixmap;
    }
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, false);
    if (layer == 0) {
        // Orbit's grid: white at 5 %, a 1 px line every 20 px.
        const QColor line(255, 255, 255, 0x0D);
        for (int at = 0; at < tileSize; at += 20) {
            p.fillRect(QRectF(at, 0, 1, tileSize), line);
            p.fillRect(QRectF(0, at, tileSize, 1), line);
        }
    } else {
        // Nearer: small crosses on a coarse grid, moving more than the grid behind them.
        const QColor mark(255, 255, 255, 0x16);
        for (int x = 0; x < tileSize; x += 120) {
            for (int y = 0; y < tileSize; y += 120) {
                const int cx = x + 60;
                const int cy = y + 60;
                p.fillRect(QRectF(cx - 3, cy, 7, 1), mark);
                p.fillRect(QRectF(cx, cy - 3, 1, 7), mark);
            }
        }
    }
    return pixmap;
}

}  // namespace OrbitPainter
