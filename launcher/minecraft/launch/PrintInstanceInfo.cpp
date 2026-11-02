/* Copyright 2013-2021 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fstream>
#include <string>

#include <launch/LaunchTask.h>
#include "PrintInstanceInfo.h"

#include <QFutureWatcher>
#include <QtConcurrent>

#include "HardwareInfo.h"

#if defined(Q_OS_FREEBSD)
namespace {
void runSysctlHwModel(QStringList& log)
{
    char buff[512];
    FILE* hwmodel = popen("sysctl hw.model", "r");
    while (fgets(buff, 512, hwmodel) != NULL) {
        log << QString::fromUtf8(buff);
        break;
    }
    pclose(hwmodel);
}

void runPciconf(QStringList& log)
{
    char buff[512];
    std::string strcard;
    FILE* pciconf = popen("pciconf -lv -a vgapci0", "r");
    while (fgets(buff, 512, pciconf) != NULL) {
        if (strncmp(buff, "    vendor", 10) == 0) {
            std::string str(buff);
            strcard.append(str.substr(str.find_first_of("'") + 1, str.find_last_not_of("'") - (str.find_first_of("'") + 2)));
            strcard.append(" ");
        } else if (strncmp(buff, "    device", 10) == 0) {
            std::string str2(buff);
            strcard.append(str2.substr(str2.find_first_of("'") + 1, str2.find_last_not_of("'") - (str2.find_first_of("'") + 2)));
        }
        log << QString::fromStdString(strcard);
        break;
    }
    pclose(pciconf);
}
}  // namespace
#endif

namespace {
/** The hardware part of the log: CPU, RAM and GPU. Probing the GPU runs `lspci`, which can take a second. */
QStringList hardwareLines()
{
    QStringList log;
    log << "";
    log << "OS: " + QString("%1 | %2 | %3").arg(QSysInfo::prettyProductName(), QSysInfo::kernelType(), QSysInfo::kernelVersion());
#ifdef Q_OS_FREEBSD
    ::runSysctlHwModel(log);
    ::runPciconf(log);
#else
    log << "CPU: " + HardwareInfo::cpuInfo();
#ifdef Q_OS_MACOS
    log << "Memory pressure level: " + MacOSHardwareInfo::memoryPressureLevelName();
#else
    log << QString("RAM: %1 MiB (available: %2 MiB)").arg(HardwareInfo::totalRamMiB()).arg(HardwareInfo::availableRamMiB());
#endif

#endif
    return log;
}
}  // namespace

void PrintInstanceInfo::executeTask()
{
    // Galaxy: the hardware probe runs on a worker thread (it froze the launcher for about a second at every launch),
    // and the GPU part, which does not change while the launcher runs, is probed once.
    static QStringList gpu;
    auto* watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher] {
        auto log = watcher->result();
        watcher->deleteLater();
        if (gpu.isEmpty()) {
            gpu = log.takeLast().split('\n');
        } else {
            log.removeLast();
        }
        log.append(gpu);
        log << "";
        logLines(log, MessageLevel::Launcher);
        auto instance = m_parent->instance();
        logLines(instance->verboseDescription(m_session, m_targetToJoin), MessageLevel::Launcher);
        emitSucceeded();
    });
    const bool probeGpu = gpu.isEmpty();
    watcher->setFuture(QtConcurrent::run([probeGpu] {
        auto log = hardwareLines();
        log << (probeGpu ? HardwareInfo::gpuInfo().join('\n') : QString());
        return log;
    }));
}
