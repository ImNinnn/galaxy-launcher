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

#include "GalaxyLayout.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace GalaxyLayout {

namespace {

constexpr qreal FirstOrbitGap = 1.15;     ///< Space between the sun's surface and the first orbit's bodies.
constexpr qreal OrbitGap = 0.32;          ///< Space between neighbouring orbits' bodies.
constexpr qreal BodyGap = 0.22;           ///< Space between neighbouring bodies on one orbit.
constexpr qreal OrbitFill = 0.92;         ///< Fraction of an orbit's circumference bodies may occupy.
constexpr qreal BaseAngularSpeed = 0.11;  ///< Radians per second on the innermost orbit.
constexpr qreal RingGap = 0.03;
constexpr qreal MoonGap = 0.08;

qreal lerp(qreal a, qreal b, qreal t)
{
    return a + (b - a) * t;
}

qreal phaseFor(int orbit)
{
    // Golden angle offsets keep neighbouring orbits from lining up.
    return std::fmod(orbit * 2.399963229728653, 2 * M_PI);
}

/** Lays out moons and rings around a host planet; returns the host's extent. */
qreal layoutSatellites(SystemLayout& layout, int hostNode, const QList<int>& rings, const QList<int>& moons, const QList<CelestialBody>& bodies,
                       const Params& params)
{
    const qreal hostRadius = layout.nodes[hostNode].radius;
    qreal extent = hostRadius;

    // Rings: concentric bands, innermost first.
    qreal inner = hostRadius * 1.3;
    for (int bodyIndex : rings) {
        Node ring;
        ring.body = bodyIndex;
        ring.parent = hostNode;
        ring.role = Node::Role::Ring;
        ring.radius = ringWidth(bodies[bodyIndex].bytes, params);
        ring.orbitRadius = inner + ring.radius / 2;
        inner += ring.radius + RingGap;
        layout.nodes.append(ring);
    }
    if (!rings.isEmpty()) {
        extent = std::max(extent, inner);
    }

    if (moons.isEmpty()) {
        return extent;
    }

    // Moons: evenly spaced on as many small orbits as needed.
    qreal maxMoon = 0;
    QList<qreal> radii;
    for (int bodyIndex : moons) {
        radii.append(moonRadius(bodies[bodyIndex].bytes, hostRadius, params));
        maxMoon = std::max(maxMoon, radii.last());
    }
    qreal orbitRadius = std::max(hostRadius * 1.6, extent + 0.12) + maxMoon;
    int placed = 0;
    int moonOrbit = 0;
    while (placed < moons.size()) {
        const int capacity = std::max(3, static_cast<int>(std::floor(2 * M_PI * orbitRadius / (2 * maxMoon + MoonGap))));
        const int count = std::min(capacity, static_cast<int>(moons.size()) - placed);
        const qreal speed = 0.35 * std::pow(hostRadius * 1.6 / orbitRadius, 1.5) * (moonOrbit % 2 ? -1 : 1);
        for (int i = 0; i < count; i++) {
            Node moon;
            moon.body = moons[placed + i];
            moon.parent = hostNode;
            moon.role = Node::Role::Moon;
            moon.radius = radii[placed + i];
            moon.orbitRadius = orbitRadius;
            moon.orbit = moonOrbit;
            moon.phase = phaseFor(moonOrbit + 3) + 2 * M_PI * i / count;
            moon.speed = speed;
            layout.nodes.append(moon);
        }
        placed += count;
        extent = std::max(extent, orbitRadius + maxMoon);
        orbitRadius += 2 * maxMoon + 0.1;
        moonOrbit++;
    }
    return extent;
}

}  // namespace

qreal sizeFraction(qint64 bytes, qint64 lo, qint64 hi)
{
    if (bytes < 0) {
        return UnknownSizeFraction;
    }
    const qreal value = std::log(static_cast<qreal>(std::max<qint64>(bytes, 1)));
    const qreal low = std::log(static_cast<qreal>(lo));
    const qreal high = std::log(static_cast<qreal>(hi));
    return std::clamp((value - low) / (high - low), 0.0, 1.0);
}

qreal planetRadius(qint64 bytes, const Params& params)
{
    return lerp(PlanetMinRadius, PlanetMaxRadius, sizeFraction(bytes, PlanetMinBytes, PlanetMaxBytes)) * params.objectScale;
}

qreal moonRadius(qint64 bytes, qreal parentRadius, const Params& params)
{
    const qreal radius = lerp(MoonMinRadius, MoonMaxRadius, sizeFraction(bytes, MoonMinBytes, MoonMaxBytes)) * params.objectScale;
    return std::min(radius, parentRadius * MoonMaxParentFraction);
}

qreal ringWidth(qint64 bytes, const Params& params)
{
    return lerp(RingMinWidth, RingMaxWidth, sizeFraction(bytes, RingMinBytes, RingMaxBytes)) * params.objectScale;
}

SystemLayout layoutSystem(const QList<CelestialBody>& bodies, const Params& params)
{
    SystemLayout layout;
    if (bodies.isEmpty()) {
        return layout;
    }

    // Planets: the base game first, then mods in their (stable) incoming order.
    QList<int> planetBodies;
    QList<int> ringBodies;
    QList<int> moonBodies;
    int shaderLoader = -1;
    for (int i = 0; i < bodies.size(); i++) {
        const auto& body = bodies[i];
        if (body.isPlanet()) {
            planetBodies.append(i);
            if (shaderLoader < 0 && body.isShaderLoader && body.enabled) {
                shaderLoader = i;
            }
        } else if (body.isRing()) {
            ringBodies.append(i);
        } else {
            moonBodies.append(i);
        }
    }

    // Create planet nodes and their satellites (satellite nodes follow all planet nodes).
    for (int bodyIndex : planetBodies) {
        Node planet;
        planet.body = bodyIndex;
        planet.role = Node::Role::Planet;
        planet.radius = bodies[bodyIndex].kind == CelestialBody::Kind::BaseGame ? BaseGameRadius * params.objectScale
                                                                                 : planetRadius(bodies[bodyIndex].bytes, params);
        planet.extent = planet.radius;
        layout.nodes.append(planet);
    }
    const int planetCount = static_cast<int>(layout.nodes.size());
    auto nodeOfBody = [&layout, planetCount](int bodyIndex) {
        for (int n = 0; n < planetCount; n++) {
            if (layout.nodes[n].body == bodyIndex) {
                return n;
            }
        }
        return 0;
    };
    layout.moonHost = 0;  // base game
    layout.ringHost = shaderLoader >= 0 ? nodeOfBody(shaderLoader) : 0;

    if (layout.ringHost == layout.moonHost) {
        layout.nodes[0].extent = layoutSatellites(layout, 0, ringBodies, moonBodies, bodies, params);
    } else {
        layout.nodes[layout.moonHost].extent = layoutSatellites(layout, layout.moonHost, {}, moonBodies, bodies, params);
        layout.nodes[layout.ringHost].extent = layoutSatellites(layout, layout.ringHost, ringBodies, {}, bodies, params);
    }

    // Group planets into orbits by capacity, then compute the actual orbit radii.
    QList<QList<int>> orbits;
    {
        qreal provisional = SunRadius + FirstOrbitGap + layout.nodes[0].extent;
        qreal used = 0;
        qreal maxExtent = 0;
        orbits.append(QList<int>());
        for (int n = 0; n < planetCount; n++) {
            const qreal need = 2 * layout.nodes[n].extent + BodyGap;
            const qreal capacity = 2 * M_PI * provisional * OrbitFill;
            const bool full = orbits.last().size() >= MaxPlanetsPerOrbit || used + need > capacity;
            if (!orbits.last().isEmpty() && full) {
                provisional += maxExtent + OrbitGap + layout.nodes[n].extent;
                orbits.append(QList<int>());
                used = 0;
                maxExtent = 0;
            }
            orbits.last().append(n);
            used += need;
            maxExtent = std::max(maxExtent, layout.nodes[n].extent);
        }
    }

    qreal previousRadius = SunRadius;
    qreal previousExtent = 0;
    for (int k = 0; k < orbits.size(); k++) {
        qreal maxExtent = 0;
        qreal total = 0;
        for (int n : orbits[k]) {
            maxExtent = std::max(maxExtent, layout.nodes[n].extent);
            total += 2 * layout.nodes[n].extent + BodyGap;
        }
        const qreal radius = k == 0 ? SunRadius + FirstOrbitGap + maxExtent : previousRadius + previousExtent + OrbitGap + maxExtent;
        layout.orbitRadii.append(radius);

        // Spread bodies along the orbit in proportion to their size, starting at a per-orbit offset.
        const qreal speed = BaseAngularSpeed * std::pow(layout.orbitRadii.first() / radius, 1.5);
        const qreal circumference = 2 * M_PI * radius;
        const qreal slack = std::max(0.0, circumference - total) / std::max<qsizetype>(1, orbits[k].size());
        qreal along = 0;
        for (int n : orbits[k]) {
            auto& node = layout.nodes[n];
            const qreal span = 2 * node.extent + BodyGap + slack;
            node.orbit = k;
            node.orbitRadius = radius;
            node.phase = phaseFor(k) + (along + span / 2) / radius;
            node.speed = speed;
            along += span;
        }
        previousRadius = radius;
        previousExtent = maxExtent;
    }
    layout.extent = previousRadius + previousExtent;
    layout.viewExtent = layout.extent;
    if (layout.orbitRadii.size() > InitialViewOrbits) {
        qreal maxExtent = 0;
        for (int n : orbits[InitialViewOrbits - 1]) {
            maxExtent = std::max(maxExtent, layout.nodes[n].extent);
        }
        layout.viewExtent = layout.orbitRadii[InitialViewOrbits - 1] + maxExtent;
    }
    return layout;
}

qreal galacticSpeed(qreal /*radius*/)
{
    // The whole galaxy turns together (about one turn every five minutes). Differential rotation would look more
    // "physical", but systems at similar distances would then drift into each other.
    return 2 * M_PI / 300.0;
}

qreal systemSpacing(qreal spacingFactor)
{
    return 3.6 * std::clamp(spacingFactor, 0.5, 3.0);
}

QList<QPointF> layoutGalaxy(int count, qreal spacingFactor)
{
    QList<QPointF> positions;
    positions.reserve(count);
    // Vogel's sunflower spiral: even density and no overlaps at any count. The half-step offset keeps the
    // first systems as far apart as the rest.
    const qreal c = systemSpacing(spacingFactor) / 1.75;
    constexpr qreal goldenAngle = 2.399963229728653;
    for (int i = 0; i < count; i++) {
        // Pushed outward so the black hole has room; spacing between systems only grows.
        const qreal radius = BlackHoleClearance + c * std::sqrt(i + 0.5);
        const qreal angle = i * goldenAngle;
        positions.append(QPointF(radius * std::cos(angle), radius * std::sin(angle)));
    }
    return positions;
}

}  // namespace GalaxyLayout
