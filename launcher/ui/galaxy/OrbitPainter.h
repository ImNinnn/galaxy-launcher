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

#include "CelestialPainter.h"

/**
 * CelestialPainter's objects as pixel art, for the Orbit look: blocky lit spheres like Orbit's icon, suns with a
 * pixel corona, rings of square pixels (after the Gravity Studios logo), a pixel black hole, dithered nebulae, plus
 * shaped sparkles and a grid instead of stars. Every object is rasterized on a coarse grid of square cells and drawn
 * scaled up without smoothing. Same parameters as CelestialPainter, which forwards to these with the Orbit look.
 */
namespace OrbitPainter {

using CelestialPainter::SurfaceStyle;

/** Side of one pixel-art cell, in logical pixels, for a sphere of radius @p r: about 16 cells across. */
int cellFor(qreal r);

void drawSphere(QPainter& p, QPointF c, qreal r, const SurfaceStyle& style, QPointF light, qreal lightFacing, int quality, qreal opacity);
void drawSun(QPainter& p, QPointF c, qreal r, const QColor& color, qreal intensity, int quality, qreal opacity);
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
                  bool dashed);
void drawBlackHole(QPainter& p, QPointF c, qreal r, qreal swirl, int quality, qreal opacity);
void drawNebula(QPainter& p, QPointF c, qreal radius, const QColor& color, qreal time, std::uint32_t seed, int quality, qreal opacity);
void drawSparkle(QPainter& p, QPointF c, qreal size, const QColor& color, qreal twinkle, qreal opacity);
void drawOrbit(QPainter& p, QPointF c, qreal rx, qreal ry, const QColor& color, qreal opacity);

/**
 * A pixel outline around a sphere of radius @p r drawn by drawSphere(): a ring of cells on the sphere's own pixel
 * grid, @p gap cells away from it, @p thickness cells wide; @p dashed leaves out every other pair of cells.
 */
void drawOutline(QPainter& p, QPointF c, qreal r, const QColor& color, int gap, int thickness = 1, bool dashed = false);

/** Orbit's flat background. */
QPixmap renderSkyBase(const QSize& size, int margin, qreal devicePixelRatio);
/** Layer 0: the 20 px grid; layer 1: a coarser grid of small crosses that moves more (depth). */
QPixmap renderGridTile(int tileSize, int layer, qreal devicePixelRatio, int quality);

/** Logical size of a grid tile: a multiple of both grids. */
constexpr int GridTileSize = 480;

}  // namespace OrbitPainter
