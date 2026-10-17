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

#pragma once

#include <QColor>
#include <QPixmap>
#include <QPointF>

#include <cstdint>

class QPainter;

/**
 * Procedural drawing of celestial objects with QPainter.
 *
 * Everything is drawn in screen space; the 2.5D projection happens in GalaxyView. No assets or network access are
 * needed. Quality levels: 0 = flat shapes (fallback), 1 = shaded, 2 = shaded with surface detail and soft shadows.
 */
namespace CelestialPainter {

enum class Surface : std::uint8_t {
    Banded,  ///< Gas giant style stripes.
    Rocky,   ///< Craters.
    Smooth,  ///< Plain, icy.
    Terran,  ///< Oceans and continents (used for the base game planet).
};

struct SurfaceStyle {
    QColor base;
    QColor accent;
    Surface surface = Surface::Smooth;
    std::uint32_t seed = 0;
};

/** A deterministic, pleasant style for a body derived from a stable key (e.g. a file name). */
SurfaceStyle styleForKey(const QString& key, bool moon);

/** The base game planet: blue-green with blocky continents. */
SurfaceStyle baseGameStyle();

/**
 * Draws a lit sphere.
 * @param light screen-space direction from the body towards its light source (need not be normalized).
 * @param lightFacing how much of the lit hemisphere faces the viewer, in [-1, 1].
 */
void drawSphere(QPainter& p,
                QPointF center,
                qreal radius,
                const SurfaceStyle& style,
                QPointF light,
                qreal lightFacing,
                int quality,
                qreal opacity = 1.0);

/** Draws a sun with corona. @param intensity 1 is normal; higher values brighten (launch flash). */
void drawSun(QPainter& p, QPointF center, qreal radius, const QColor& color, qreal intensity, int quality, qreal opacity = 1.0);

/** Soft contact shadow on the "table" plane below a body. */
void drawShadow(QPainter& p, QPointF center, qreal rx, qreal ry, qreal opacity);

/**
 * Draws half of an elliptical band (a shader ring).
 * @param front true for the half nearer to the viewer (drawn after the planet), false for the far half.
 */
void drawRingHalf(QPainter& p,
                  QPointF center,
                  qreal rx,
                  qreal ry,
                  qreal width,
                  qreal rotationDeg,
                  const QColor& color,
                  bool front,
                  int quality,
                  qreal opacity = 1.0);

/**
 * The black hole at the centre of the galaxy: halo, tilted accretion disk (far half behind, near half in front),
 * a lensed arc of the disk's far side above it, the event horizon and its photon ring.
 * @param swirl angle in degrees that rotates the disk's colors, for a slow swirling motion.
 */
void drawBlackHole(QPainter& p, QPointF center, qreal radius, qreal swirl, int quality, qreal opacity = 1.0);

/**
 * A nebula: swirling gas clouds in the sun's color and space purple, left behind when a profile's sun explodes at
 * launch. @param time drives the slow swirl; @param seed varies the cloud shapes per profile.
 */
void drawNebula(QPainter& p, QPointF center, qreal radius, const QColor& color, qreal time, std::uint32_t seed, int quality, qreal opacity = 1.0);

/** A twinkling star-like sparkle (what planets, moons and rings become inside a nebula). */
void drawSparkle(QPainter& p, QPointF center, qreal size, const QColor& color, qreal twinkle, qreal opacity = 1.0);

/** A subtle orbit path. */
void drawOrbit(QPainter& p, QPointF center, qreal rx, qreal ry, const QColor& color, qreal opacity);

/** Renders a static star field with faint nebulae. Cached by the caller; only regenerated on resize. */
QPixmap renderStarfield(const QSize& size, qreal devicePixelRatio, int quality);

/**
 * Parallax background, part 1: sky gradient and nebulae, without stars. Rendered larger than the view by `margin`
 * pixels on every side so it can shift a little.
 */
QPixmap renderSkyBase(const QSize& size, int margin, qreal devicePixelRatio, int quality);

/**
 * Parallax background, part 2: a seamlessly tileable square of stars. Layer 0 is distant (many faint stars),
 * layer 1 is near (fewer, brighter stars with halos).
 */
QPixmap renderStarTile(int tileSize, int layer, qreal devicePixelRatio, int quality);

}  // namespace CelestialPainter
