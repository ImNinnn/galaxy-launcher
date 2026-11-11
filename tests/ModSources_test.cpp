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

#include <QJsonDocument>
#include <QTest>
#include <QUrlQuery>

#include <modplatform/flame/CurseForgeAPI.h>
#include <modplatform/orbit/OrbitAPI.h>

/** Galaxy Launcher's CurseForge search (encoding, kinds, ranking) and the Orbit source (targets, releases). */
class ModSourcesTest : public QObject {
    Q_OBJECT

   private slots:
    void test_searchUrlEncodesText()
    {
        ResourceAPI::SearchArgs args;
        args.type = ModPlatform::ResourceType::Mod;
        args.search = QStringLiteral("xaero's minimap & co+");
        const QUrlQuery query(CurseForgeAPI::searchUrl(args));
        QCOMPARE(query.queryItemValue("searchFilter", QUrl::FullyDecoded), QStringLiteral("xaero's minimap & co+"));
        QCOMPARE(query.queryItemValue("classId"), QStringLiteral("6"));  // mods, never modpacks
        QCOMPARE(query.queryItemValue("sortField"), QStringLiteral("2"));  // relevance starts from popularity
        QCOMPARE(query.queryItemValue("pageSize"), QString::number(CurseForgeAPI::PageSize));
        QVERIFY(!CurseForgeAPI::searchUrl(args).toString(QUrl::FullyEncoded).contains(' '));
    }

    void test_searchUrlKinds()
    {
        ResourceAPI::SearchArgs args;
        args.type = ModPlatform::ResourceType::ShaderPack;
        QCOMPARE(QUrlQuery(CurseForgeAPI::searchUrl(args)).queryItemValue("classId"), QStringLiteral("6552"));
        args.type = ModPlatform::ResourceType::ResourcePack;
        QCOMPARE(QUrlQuery(CurseForgeAPI::searchUrl(args)).queryItemValue("classId"), QStringLiteral("12"));
    }

    void test_relevanceOrder()
    {
        // The project the user means comes before projects that only mention it.
        const auto exact = CurseForgeAPI::relevance("sodium", "Sodium", "sodium", "");
        const auto addon = CurseForgeAPI::relevance("sodium", "Sodium Extra", "sodium-extra", "");
        const auto mention = CurseForgeAPI::relevance("sodium", "Embeddium", "embeddium", "A fork of Sodium");
        const auto unrelated = CurseForgeAPI::relevance("sodium", "Lithium", "lithium", "Optimizes things");
        QVERIFY(exact > addon);
        QVERIFY(addon > mention);
        QVERIFY(mention > unrelated);
        // Abbreviations and slugs.
        QVERIFY(CurseForgeAPI::relevance("jei", "Just Enough Items (JEI)", "jei", "") >= 800);
        QVERIFY(CurseForgeAPI::relevance("xaeros minimap", "Xaero's Minimap", "xaeros-minimap", "") == 1000);
        QVERIFY(CurseForgeAPI::relevance("fabric api", "Fabric API", "fabric-api", "") >
                CurseForgeAPI::relevance("fabric api", "Cloth Config API (Fabric/Forge)", "cloth-config", ""));
    }

    void test_orbitTarget()
    {
        QCOMPARE(OrbitAPI::targetFor("26.3"), QStringLiteral("26.3"));
        QCOMPARE(OrbitAPI::targetFor("26.3.1"), QStringLiteral("26.3"));
        QCOMPARE(QUrlQuery(OrbitAPI::releaseUrl("26.1")).queryItemValue("target"), QStringLiteral("26.1"));
    }

    void test_orbitRelease()
    {
        const auto answer = QJsonDocument::fromJson(R"({"latest":"1.0.3","minimum":"1.0.0","required":true,
            "update":{"version":"1.0.3","url":"/v1/mod/releases/1.0.3/26.3","sha256":"c561c8","size":980534,"signature":"x"}})")
                                .object();
        const auto pack = OrbitAPI::project(answer, "26.3", "26.3.1");
        QCOMPARE(pack.provider, ModPlatform::ResourceProvider::ORBIT);
        QVERIFY(pack.versionsLoaded);
        QCOMPARE(pack.versions.size(), 1);
        const auto& version = pack.versions.first();
        QCOMPARE(version.downloadUrl, QStringLiteral("https://api.orbitcosmetics.dev/v1/mod/releases/1.0.3/26.3"));
        QCOMPARE(version.hashType, QStringLiteral("sha256"));
        QCOMPARE(version.hash, QStringLiteral("c561c8"));
        QVERIFY(version.mcVersion.contains("26.3.1"));
        QVERIFY(version.loaders.testFlag(ModPlatform::Fabric));
        QCOMPARE(version.dependencies.size(), 1);
        QCOMPARE(version.dependencies.first().addonId.toString(), QString::fromLatin1(OrbitAPI::FabricApiModrinthId));

        // No release for this Minecraft version: the project, without versions.
        const auto none = OrbitAPI::project(QJsonDocument::fromJson(R"({"latest":null,"update":null})").object(), "1.21", "1.21.1");
        QVERIFY(none.versions.isEmpty());
    }
};

QTEST_GUILESS_MAIN(ModSourcesTest)

#include "ModSources_test.moc"
