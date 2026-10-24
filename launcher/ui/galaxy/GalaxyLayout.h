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

#include <QList>
#include <QPointF>

#include <cstdint>

#include "CelestialBody.h"

/**
 * Pure geometry for the galaxy: how big bodies are and where they orbit. No Qt widgets, no painting.
 *
 * Units are "local units" in which a profile's sun has radius 1. Sizes follow the installed file size on a
 * logarithmic scale, clamped to a minimum and maximum so that tiny projects stay visible and huge ones do not
 * dominate. Content whose size is unknown uses a fixed fallback (see unknownSizeFraction).
 */
namespace GalaxyLayout {

constexpr qreal SunRadius = 1.0;
constexpr qreal BaseGameRadius = 0.5;  ///< Not size based: it stands for the game itself.

constexpr qreal PlanetMinRadius = 0.17;
constexpr qreal PlanetMaxRadius = 0.6;
constexpr qint64 PlanetMinBytes = 16LL * 1024;         ///< At or below: minimum radius.
constexpr qint64 PlanetMaxBytes = 64LL * 1024 * 1024;  ///< At or above: maximum radius.

constexpr qreal MoonMinRadius = 0.06;
constexpr qreal MoonMaxRadius = 0.2;
constexpr qreal MoonMaxParentFraction = 0.55;  ///< Moons always stay visibly smaller than their planet.
constexpr qint64 MoonMinBytes = 16LL * 1024;
constexpr qint64 MoonMaxBytes = 256LL * 1024 * 1024;  ///< High resolution resource packs can be very large.

constexpr qreal RingMinWidth = 0.035;
constexpr qreal RingMaxWidth = 0.14;
constexpr qint64 RingMinBytes = 16LL * 1024;
constexpr qint64 RingMaxBytes = 64LL * 1024 * 1024;

/** At most this many planets share an orbit, so planets stay well apart; large collections get more orbits. */
constexpr int MaxPlanetsPerOrbit = 3;

/** Large systems first open framed on this many inner orbits; zooming out shows the rest. */
constexpr int InitialViewOrbits = 8;

/** Position on the size scale used when a size is unknown (unreadable file, folder still being measured). */
constexpr qreal UnknownSizeFraction = 0.35;

/** The black hole at the centre of the galaxy, in galaxy units. Systems keep at least BlackHoleClearance away. */
constexpr qreal BlackHoleRadius = 0.55;
constexpr qreal BlackHoleClearance = 3.0;

/** Galactic rotation: angular speed (radians per second) of a system at distance `radius` from the black hole.
 *  The same for every radius, so the spacing between systems never changes. */
qreal galacticSpeed(qreal radius);

/** Radius of a profile's mini system in the galaxy overview, in galaxy units. */
constexpr qreal OverviewRadius = 1.25;
constexpr qreal OverviewSunRadius = 0.3;

struct Params {
    qreal objectScale = 1.0;  ///< User scaling for body sizes (Settings > Galaxy).
    std::uint32_t seed = 0;   ///< Varies each orbit's speed per profile (see orbitSpeedFactor).
};

/** A stable factor between 0.55 and 1.55 for an orbit's speed, so each orbit turns at its own pace. */
qreal orbitSpeedFactor(std::uint32_t seed, int orbit);

/** Maps a byte size to [0, 1] on a log scale between lo and hi. Unknown sizes (< 0) map to UnknownSizeFraction. */
qreal sizeFraction(qint64 bytes, qint64 lo, qint64 hi);

qreal planetRadius(qint64 bytes, const Params& params = {});
qreal moonRadius(qint64 bytes, qreal parentRadius, const Params& params = {});
qreal ringWidth(qint64 bytes, const Params& params = {});

struct Node {
    enum class Role : std::uint8_t { Planet, Moon, Ring };

    int body = -1;    ///< Index into the bodies list the layout was computed from.
    int parent = -1;  ///< Node index of the host planet (moons and rings).
    Role role = Role::Planet;
    qreal radius = 0;       ///< Sphere radius (planets, moons) or band width (rings).
    qreal orbitRadius = 0;  ///< Around the sun (planets), around the parent (moons, rings).
    qreal phase = 0;        ///< Angle at orbit time 0, radians.
    qreal speed = 0;        ///< Angular speed, radians per second.
    int orbit = 0;          ///< Index of the orbit (planets: around the sun; moons: around the parent).
    qreal extent = 0;       ///< Planets: radius including their moons and rings.
};

struct SystemLayout {
    QList<Node> nodes;
    QList<qreal> orbitRadii;     ///< Planet orbits around the sun.
    qreal extent = SunRadius;    ///< Radius of the whole system.
    qreal viewExtent = SunRadius;  ///< Radius framed when the system opens (the inner orbits of large systems).
    int ringHost = -1;           ///< Node index of the planet carrying shader rings.
    int moonHost = -1;           ///< Node index of the planet carrying pack moons.
};

/**
 * Lays out a profile. bodies[0] must be the base game.
 * Shader rings attach to the first enabled shader-loader mod (Iris, Oculus, OptiFine), else to the base game.
 * Packs have no relationship to individual mods, so their moons orbit the base game planet.
 */
SystemLayout layoutSystem(const QList<CelestialBody>& bodies, const Params& params = {});

/** Positions of `count` systems on the galaxy plane (x, z), as a sunflower spiral around the central black hole. */
QList<QPointF> layoutGalaxy(int count, qreal spacingFactor = 1.0);

/** Distance between neighbouring systems for a spacing factor, in galaxy units. */
qreal systemSpacing(qreal spacingFactor);

/** The systems of one profile category, gathered in one part of the galaxy. */
struct Cluster {
    QString name;
    QPointF center;
    qreal radius = 0;  ///< Reaches just past the overview discs of its outer systems.
};

/**
 * Like layoutGalaxy, but the systems of one category (`groups[i]`, empty for none) stay together: each category is a
 * small sunflower of its own, and the categories and the systems without one share the galaxy's spiral (larger
 * categories nearer the black hole). Without any category this is exactly layoutGalaxy.
 */
QList<QPointF> layoutGalaxyClusters(const QStringList& groups, qreal spacingFactor = 1.0, QList<Cluster>* clusters = nullptr);

}  // namespace GalaxyLayout
