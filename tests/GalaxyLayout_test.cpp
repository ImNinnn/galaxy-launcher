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

#include <QTest>

#include <QLineF>
#include <QMap>
#include <QSet>
#include <QtMath>

#include <ui/galaxy/GalaxyLayout.h>

using namespace GalaxyLayout;
using Kind = CelestialBody::Kind;
using Role = Node::Role;

namespace {

CelestialBody body(Kind kind, const QString& file, qint64 bytes, bool shaderLoader = false, bool enabled = true)
{
    CelestialBody b;
    b.kind = kind;
    b.fileName = file;
    b.internalId = file;
    b.key = QString::number(static_cast<int>(kind)) + ':' + file;
    b.bytes = bytes;
    b.isShaderLoader = shaderLoader;
    b.enabled = enabled;
    return b;
}

CelestialBody base()
{
    return body(Kind::BaseGame, QString(), -1);
}

int nodeOf(const SystemLayout& layout, int bodyIndex)
{
    for (int n = 0; n < layout.nodes.size(); n++) {
        if (layout.nodes[n].body == bodyIndex) {
            return n;
        }
    }
    return -1;
}

}  // namespace

class GalaxyLayoutTest : public QObject {
    Q_OBJECT

   private slots:
    void sizesFollowFileSizeWithinLimits()
    {
        qreal previous = 0;
        for (qint64 bytes : { 1LL, 16LL * 1024, 100LL * 1024, 1LL << 20, 10LL << 20, 64LL << 20, 1LL << 34 }) {
            const qreal r = planetRadius(bytes);
            QVERIFY2(r >= previous, "planet size must not shrink as the file grows");
            QVERIFY(r >= PlanetMinRadius - 1e-9);
            QVERIFY(r <= PlanetMaxRadius + 1e-9);
            previous = r;
        }
        QCOMPARE(planetRadius(1), PlanetMinRadius);
        QCOMPARE(planetRadius(1LL << 34), PlanetMaxRadius);
        // A noticeable difference between a small and a large mod.
        QVERIFY(planetRadius(5LL << 20) > planetRadius(50LL * 1024) * 1.5);
    }

    void unknownSizesUseTheDocumentedFallback()
    {
        QCOMPARE(sizeFraction(-1, PlanetMinBytes, PlanetMaxBytes), UnknownSizeFraction);
        QVERIFY(planetRadius(-1) > PlanetMinRadius);
        QVERIFY(planetRadius(-1) < PlanetMaxRadius);
    }

    void moonsStaySmallerThanTheirPlanet()
    {
        const qreal parent = BaseGameRadius;
        QVERIFY(moonRadius(1LL << 40, parent) <= parent * MoonMaxParentFraction + 1e-9);
        QVERIFY(moonRadius(1, parent) >= MoonMinRadius - 1e-9);
        QVERIFY(moonRadius(200LL << 20, parent) > moonRadius(20LL * 1024, parent));
    }

    void ringWidthFollowsShaderSize()
    {
        QVERIFY(ringWidth(30LL << 20) > ringWidth(100LL * 1024));
        QVERIFY(ringWidth(1) >= RingMinWidth - 1e-9);
        QVERIFY(ringWidth(1LL << 40) <= RingMaxWidth + 1e-9);
    }

    void objectScaleScalesBodies()
    {
        Params big;
        big.objectScale = 2.0;
        QCOMPARE(planetRadius(1 << 20, big), planetRadius(1 << 20) * 2.0);
    }

    void emptyProfileHasOnlyTheBaseGame()
    {
        const auto layout = layoutSystem({ base() });
        QCOMPARE(layout.nodes.size(), 1);
        QCOMPARE(layout.nodes[0].role, Role::Planet);
        QVERIFY(layout.extent > SunRadius);
    }

    void packsOrbitTheBaseGameAndShadersRingIt()
    {
        const QList<CelestialBody> bodies{ base(), body(Kind::Mod, "sodium.jar", 1 << 20), body(Kind::ResourcePack, "faithful.zip", 5 << 20),
                                           body(Kind::DataPack, "dp.zip", 30000), body(Kind::ShaderPack, "bsl.zip", 2 << 20) };
        const auto layout = layoutSystem(bodies);
        QCOMPARE(layout.nodes.size(), bodies.size());
        QCOMPARE(layout.moonHost, 0);
        QCOMPARE(layout.ringHost, 0);
        const auto& moon = layout.nodes[nodeOf(layout, 2)];
        QCOMPARE(moon.role, Role::Moon);
        QCOMPARE(moon.parent, 0);
        QCOMPARE(layout.nodes[nodeOf(layout, 3)].role, Role::Moon);
        const auto& ring = layout.nodes[nodeOf(layout, 4)];
        QCOMPARE(ring.role, Role::Ring);
        QCOMPARE(ring.parent, 0);
        // Rings sit outside the planet; moons outside the rings.
        QVERIFY(ring.orbitRadius - ring.radius / 2 > layout.nodes[0].radius);
        QVERIFY(moon.orbitRadius - moon.radius > ring.orbitRadius + ring.radius / 2);
    }

    void shadersRingAnEnabledShaderLoader()
    {
        const QList<CelestialBody> bodies{ base(), body(Kind::Mod, "iris.jar", 2 << 20, true), body(Kind::ShaderPack, "complementary.zip", 4 << 20) };
        const auto layout = layoutSystem(bodies);
        QCOMPARE(layout.ringHost, nodeOf(layout, 1));
        QCOMPARE(layout.nodes[nodeOf(layout, 2)].parent, nodeOf(layout, 1));

        // A disabled loader does not load shaders, so the rings fall back to the base game.
        const QList<CelestialBody> disabled{ base(), body(Kind::Mod, "iris.jar.disabled", 2 << 20, true, false),
                                             body(Kind::ShaderPack, "complementary.zip", 4 << 20) };
        QCOMPARE(layoutSystem(disabled).ringHost, 0);
    }

    void planetsOnAnOrbitNeverOverlap_data()
    {
        QTest::addColumn<int>("mods");
        QTest::newRow("few") << 5;
        QTest::newRow("modpack") << 120;
        QTest::newRow("huge") << 600;
    }

    void planetsOnAnOrbitNeverOverlap()
    {
        QFETCH(int, mods);
        QList<CelestialBody> bodies{ base() };
        for (int i = 0; i < 12; i++) {
            bodies.append(body(Kind::ResourcePack, QString("pack%1.zip").arg(i), (i + 1) * 300000));
        }
        for (int i = 0; i < mods; i++) {
            bodies.append(body(Kind::Mod, QString("mod%1.jar").arg(i, 4, 10, QChar('0')), (i * 7919 % 5000 + 1) * 4096));
        }
        const auto layout = layoutSystem(bodies);
        QCOMPARE(layout.nodes.size(), bodies.size());

        // Every planet orbit is outside the sun, and orbits do not cross each other's bodies.
        for (const auto& node : layout.nodes) {
            if (node.role != Role::Planet) {
                continue;
            }
            QVERIFY(node.orbitRadius - node.extent > SunRadius);
            QVERIFY(node.orbitRadius + node.extent <= layout.extent + 1e-6);
        }
        for (int a = 0; a < layout.nodes.size(); a++) {
            for (int b = a + 1; b < layout.nodes.size(); b++) {
                const auto& na = layout.nodes[a];
                const auto& nb = layout.nodes[b];
                if (na.role != Role::Planet || nb.role != Role::Planet) {
                    continue;
                }
                const QPointF pa(na.orbitRadius * std::cos(na.phase), na.orbitRadius * std::sin(na.phase));
                const QPointF pb(nb.orbitRadius * std::cos(nb.phase), nb.orbitRadius * std::sin(nb.phase));
                const qreal distance = std::hypot(pa.x() - pb.x(), pa.y() - pb.y());
                if (na.orbit == nb.orbit) {
                    // Same orbit, same angular speed: they keep this distance forever.
                    QVERIFY2(distance >= na.extent + nb.extent, "planets on one orbit overlap");
                } else {
                    // Different orbits: radial separation guarantees no overlap at any time.
                    QVERIFY2(std::abs(na.orbitRadius - nb.orbitRadius) >= na.extent + nb.extent - 1e-9, "orbits overlap");
                }
            }
        }
        // At most three planets per orbit: large collections get more orbits instead of crowding.
        QMap<int, int> perOrbit;
        for (const auto& node : layout.nodes) {
            if (node.role == Role::Planet) {
                perOrbit[node.orbit]++;
            }
        }
        for (int count : perOrbit) {
            QVERIFY2(count <= MaxPlanetsPerOrbit, "more than three planets share an orbit");
        }
        QVERIFY(layout.orbitRadii.size() >= (mods + 1 + MaxPlanetsPerOrbit - 1) / MaxPlanetsPerOrbit);

        // Large systems open on their inner orbits; small ones show everything.
        QVERIFY(layout.viewExtent <= layout.extent + 1e-9);
        QVERIFY(layout.viewExtent > SunRadius);
        if (layout.orbitRadii.size() <= InitialViewOrbits) {
            QCOMPARE(layout.viewExtent, layout.extent);
        } else {
            QVERIFY(layout.viewExtent < layout.extent);
        }
    }

    void galaxySystemsDoNotOverlap()
    {
        for (int count : { 1, 2, 7, 40, 150 }) {
            const auto positions = layoutGalaxy(count);
            QCOMPARE(positions.size(), count);
            for (int a = 0; a < count; a++) {
                for (int b = a + 1; b < count; b++) {
                    const qreal distance = std::hypot(positions[a].x() - positions[b].x(), positions[a].y() - positions[b].y());
                    QVERIFY2(distance >= 2 * OverviewRadius, "solar systems overlap in the galaxy");
                }
                const qreal fromCenter = std::hypot(positions[a].x(), positions[a].y());
                QVERIFY2(fromCenter >= BlackHoleRadius + OverviewRadius + 0.5, "a solar system overlaps the black hole");
            }
        }
    }

    void categoriesFormClustersWithoutOverlap()
    {
        // Without categories the galaxy is laid out as before.
        QCOMPARE(layoutGalaxyClusters(QStringList(7, QString())), layoutGalaxy(7));

        QStringList groups;
        for (int i = 0; i < 60; i++) {
            groups << (i % 4 == 0 ? QString() : QStringLiteral("Category %1").arg(i % 3));
        }
        QList<Cluster> clusters;
        const auto positions = layoutGalaxyClusters(groups, 1.0, &clusters);
        QCOMPARE(positions.size(), groups.size());
        QCOMPARE(clusters.size(), 3);
        for (int a = 0; a < positions.size(); a++) {
            for (int b = a + 1; b < positions.size(); b++) {
                QVERIFY2(QLineF(positions[a], positions[b]).length() >= 2 * OverviewRadius, "solar systems overlap in the galaxy");
            }
            QVERIFY2(std::hypot(positions[a].x(), positions[a].y()) >= BlackHoleRadius + OverviewRadius + 0.5,
                     "a solar system overlaps the black hole");
        }
        // Every system of a category lies inside its cluster, and no other system does.
        for (const auto& cluster : clusters) {
            for (int i = 0; i < positions.size(); i++) {
                const qreal distance = QLineF(positions[i], cluster.center).length();
                if (groups[i] == cluster.name) {
                    QVERIFY2(distance + OverviewRadius <= cluster.radius + 1e-6, "a system sticks out of its category");
                } else {
                    QVERIFY2(distance - OverviewRadius >= cluster.radius - 1e-6, "a system lies inside another category");
                }
            }
        }
    }

    void orbitSpeedsVaryWithinLimits()
    {
        QSet<int> seen;
        for (int orbit = 0; orbit < 40; orbit++) {
            const qreal factor = orbitSpeedFactor(0x1234U, orbit);
            QVERIFY(factor >= 0.55 && factor <= 1.55);
            QCOMPARE(orbitSpeedFactor(0x1234U, orbit), factor);  // stable
            seen.insert(static_cast<int>(factor * 20));
        }
        QVERIFY2(seen.size() > 8, "orbit speeds barely vary");
    }

    void layoutIsDeterministic()
    {
        const QList<CelestialBody> bodies{ base(), body(Kind::Mod, "a.jar", 1 << 20), body(Kind::Mod, "b.jar", 3 << 20),
                                           body(Kind::ShaderPack, "s.zip", 1 << 20) };
        const auto first = layoutSystem(bodies);
        const auto second = layoutSystem(bodies);
        QCOMPARE(first.nodes.size(), second.nodes.size());
        for (int n = 0; n < first.nodes.size(); n++) {
            QCOMPARE(first.nodes[n].orbitRadius, second.nodes[n].orbitRadius);
            QCOMPARE(first.nodes[n].phase, second.nodes[n].phase);
        }
    }
};

QTEST_GUILESS_MAIN(GalaxyLayoutTest)

#include "GalaxyLayout_test.moc"
