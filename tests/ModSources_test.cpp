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
#include <modplatform/gravity/GravityAPI.h>

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

    void test_gravityVersion()
    {
        // A version as Gravity Studios' catalogue sends it (Modrinth's shape plus "provider" on dependencies).
        const auto version = GravityAPI::parseVersion(QJsonDocument::fromJson("{" "\"id\": \"orbit-1.3.2-26.3\", \"project_id\": \"orbit\", \"name\": \"Orbit 1.3.2 for 26.3\", \"version_number\": \"1.3.2+26.3\"," "\"changelog\": null, \"date_published\": \"2026-11-16T21:11:08Z\", \"version_type\": \"release\", \"status\": \"listed\"," "\"files\": [{\"hashes\": {\"sha1\": \"dc9f\", \"sha512\": \"8c73\", \"sha256\": \"46fd\"}," "\"url\": \"https://api.orbitcosmetics.dev/v1/catalog/download/orbit-1.3.2-26.3/orbit-mod-mc26.3-1.3.2.jar\"," "\"filename\": \"orbit-mod-mc26.3-1.3.2.jar\", \"primary\": true, \"size\": 1198454, \"file_type\": null}]," "\"dependencies\": [" "{\"version_id\": null, \"project_id\": \"P7dR8mSH\", \"file_name\": null, \"dependency_type\": \"required\", \"provider\": \"modrinth\"}," "{\"version_id\": null, \"project_id\": \"gs-lib\", \"file_name\": null, \"dependency_type\": \"optional\", \"provider\": \"gravity\"}," "{\"version_id\": null, \"project_id\": \"123\", \"file_name\": null, \"dependency_type\": \"required\", \"provider\": \"curseforge\"}]," "\"game_versions\": [\"26.3\", \"26.3.1\"], \"loaders\": [\"fabric\"]}")
                                                                .object());
        QVERIFY(version.has_value());
        QCOMPARE(version->fileName, QStringLiteral("orbit-mod-mc26.3-1.3.2.jar"));
        QCOMPARE(version->hashType, QStringLiteral("sha512"));
        QCOMPARE(version->hash, QStringLiteral("8c73"));
        QVERIFY(version->mcVersion.contains("26.3.1"));
        QCOMPARE(version->dependencies.size(), 3);
        QCOMPARE(version->dependencies[0].provider, std::optional(ModPlatform::ResourceProvider::MODRINTH));
        QCOMPARE(version->dependencies[1].provider, std::optional(ModPlatform::ResourceProvider::GRAVITY));
        QCOMPARE(version->dependencies[2].provider, std::optional(ModPlatform::ResourceProvider::FLAME));
    }

    void test_gravityProject()
    {
        const auto hit = GravityAPI::parseProject(QJsonDocument::fromJson("{" "\"project_id\": \"orbit\", \"project_type\": \"mod\", \"slug\": \"orbit\", \"author\": \"Gravity Studios\", \"title\": \"Orbit\"," "\"description\": \"Cosmetics.\", \"categories\": [\"cosmetic\", \"fabric\"], \"versions\": [\"26.3\"], \"downloads\": 1," "\"icon_url\": \"https://api.orbitcosmetics.dev/v1/catalog/icon/orbit.png\", \"client_side\": \"required\"," "\"server_side\": \"unsupported\", \"self_updating\": true}")
                                                           .object());
        QVERIFY(hit.has_value());
        QCOMPARE(hit->provider, ModPlatform::ResourceProvider::GRAVITY);
        QVERIFY(hit->selfUpdating);
        QCOMPARE(hit->name, QStringLiteral("Orbit"));
        QCOMPARE(QString(ModPlatform::ProviderCapabilities::name(ModPlatform::ResourceProvider::GRAVITY)), QStringLiteral("gravity"));
        QCOMPARE(GravityAPI::get().baseUrl(), QString::fromLatin1(GravityAPI::ProductionUrl));
    }
};

QTEST_GUILESS_MAIN(ModSourcesTest)

#include "ModSources_test.moc"
