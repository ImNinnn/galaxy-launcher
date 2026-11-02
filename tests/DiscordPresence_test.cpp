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

#include <ui/galaxy/DiscordPresence.h>

/** A mod with its own Discord Rich Presence takes priority over Galaxy's status while its game runs. */
class DiscordPresenceTest : public QObject {
    Q_OBJECT

   private slots:
    void recognisesRichPresenceMods_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<bool>("rpc");
        QTest::newRow("CraftPresence") << "CraftPresence-2.5.3+1.21.1.jar" << true;
        QTest::newRow("Simple RPC") << "simple-rpc-fabric-1.21-4.0.0.jar" << true;
        QTest::newRow("Discord RPC") << "DiscordRPC-1.2.jar" << true;
        QTest::newRow("discord-rpc") << "discord-rpc-forge-1.20.1.jar" << true;
        QTest::newRow("RPC word") << "hypixel-rpc-0.3.jar" << true;
        QTest::newRow("Rich Presence") << "Minecraft-Rich-Presence-1.0.jar" << true;
        QTest::newRow("Sodium") << "sodium-fabric-0.6.0+mc1.21.1.jar" << false;
        QTest::newRow("rpc inside a word") << "hyperpc-utils-1.0.jar" << false;
        QTest::newRow("chat bridge") << "dcintegration-fabric-3.0.jar" << false;
    }

    void recognisesRichPresenceMods()
    {
        QFETCH(QString, file);
        QFETCH(bool, rpc);
        QCOMPARE(DiscordPresence::isRichPresenceModFile(file), rpc);
    }
};

QTEST_GUILESS_MAIN(DiscordPresenceTest)

#include "DiscordPresence_test.moc"
