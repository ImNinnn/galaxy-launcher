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

#include <QHash>
#include <QLineF>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <numeric>

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
        const qreal speed = 0.35 * std::pow(hostRadius * 1.6 / orbitRadius, 1.5) * (moonOrbit % 2 ? -1 : 1) *
                            orbitSpeedFactor(params.seed ^ 0x5bd1e995U, moonOrbit);
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

qreal orbitSpeedFactor(std::uint32_t seed, int orbit)
{
    std::uint32_t x = seed ^ (static_cast<std::uint32_t>(orbit + 1) * 0x9e3779b9U);
    x ^= x >> 16U;
    x *= 0x7feb352dU;
    x ^= x >> 15U;
    x *= 0x846ca68bU;
    x ^= x >> 16U;
    return 0.55 + (x % 10000U) / 10000.0;
}

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
        const qreal speed = BaseAngularSpeed * std::pow(layout.orbitRadii.first() / radius, 1.5) * orbitSpeedFactor(params.seed, k);
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

QList<QPointF> layoutGalaxyClusters(const QStringList& groups, qreal spacingFactor, QList<Cluster>* clusters)
{
    const int count = static_cast<int>(groups.size());
    if (std::all_of(groups.begin(), groups.end(), [](const QString& group) { return group.isEmpty(); })) {
        return layoutGalaxy(count, spacingFactor);
    }
    const qreal spacing = systemSpacing(spacingFactor);
    const qreal c = spacing / 1.75;
    constexpr qreal goldenAngle = 2.399963229728653;
    const qreal categoryGap = spacing * 0.45;  // extra room around a category, so it reads as a group

    struct Unit {
        QString name;
        QList<int> members;
        qreal radius = 0;
        QPointF center;
    };
    QList<Unit> units;
    QHash<QString, int> unitOf;
    for (int i = 0; i < count; i++) {
        if (groups[i].isEmpty()) {
            units.append(Unit{ {}, { i } });
            continue;
        }
        if (!unitOf.contains(groups[i])) {
            unitOf.insert(groups[i], static_cast<int>(units.size()));
            units.append(Unit{ groups[i], {} });
        }
        units[unitOf[groups[i]]].members.append(i);
    }
    // Inside a category: a sunflower around its centre (one system sits at the centre).
    auto localPosition = [c](int k, qsizetype members) {
        if (members == 1) {
            return QPointF();
        }
        const qreal radius = c * std::sqrt(k + 0.5);
        return QPointF(radius * std::cos(k * goldenAngle), radius * std::sin(k * goldenAngle));
    };
    for (auto& unit : units) {
        qreal reach = 0;
        for (int k = 0; k < unit.members.size(); k++) {
            const QPointF at = localPosition(k, unit.members.size());
            reach = std::max(reach, std::hypot(at.x(), at.y()));
        }
        unit.radius = reach + spacing / 2;
    }

    // Categories first, larger ones first, then the systems without a category in their order.
    QList<int> order(units.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&units](int a, int b) {
        const bool namedA = !units[a].name.isEmpty();
        const bool namedB = !units[b].name.isEmpty();
        if (namedA != namedB) {
            return namedA;
        }
        return units[a].members.size() > units[b].members.size();
    });

    // Each unit takes the first point of the galaxy's spiral where it overlaps neither the black hole nor another unit.
    QList<int> placed;
    for (int u : order) {
        auto& unit = units[u];
        for (int j = 0;; j++) {
            const qreal radius = BlackHoleClearance + c * std::sqrt(j + 0.5);
            const QPointF at(radius * std::cos(j * goldenAngle), radius * std::sin(j * goldenAngle));
            // A single system keeps the usual clearance; a category's area keeps a little more.
            const qreal clearance = BlackHoleClearance - spacing / 2 + (unit.name.isEmpty() ? 0.0 : 0.6);
            if (radius - unit.radius < clearance - 1e-9) {
                continue;
            }
            const bool free = std::none_of(placed.begin(), placed.end(), [&](int other) {
                const qreal gap = !unit.name.isEmpty() || !units[other].name.isEmpty() ? categoryGap : 0.0;
                return QLineF(at, units[other].center).length() < unit.radius + units[other].radius + gap - 1e-9;
            });
            if (free) {
                unit.center = at;
                break;
            }
        }
        placed.append(u);
    }

    QList<QPointF> positions(count);
    for (const auto& unit : units) {
        for (int k = 0; k < unit.members.size(); k++) {
            positions[unit.members[k]] = unit.center + localPosition(k, unit.members.size());
        }
        if (clusters && !unit.name.isEmpty()) {
            clusters->append(Cluster{ unit.name, unit.center, unit.radius - spacing / 2 + OverviewRadius + 0.3 });
        }
    }
    return positions;
}

}  // namespace GalaxyLayout
