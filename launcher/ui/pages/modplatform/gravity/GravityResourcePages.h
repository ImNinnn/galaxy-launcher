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

#include "ui/pages/modplatform/DataPackPage.h"
#include "ui/pages/modplatform/ModPage.h"
#include "ui/pages/modplatform/ResourcePackPage.h"
#include "ui/pages/modplatform/ShaderPackPage.h"

namespace ResourceDownload {

/** Discover's Gravity Studios source (see GravityAPI), on the mod, resource pack, shader pack and data pack pages. */
namespace Gravity {
inline QString id()
{
    return "gravity";
}
ModPage* createModPage(ResourceDownloadDialog* dialog, BaseInstance& instance);
ResourcePackResourcePage* createResourcePackResourcePage(ResourceDownloadDialog* dialog, BaseInstance& instance);
ShaderPackResourcePage* createShaderPackResourcePage(ResourceDownloadDialog* dialog, BaseInstance& instance);
DataPackResourcePage* createDataPackResourcePage(ResourceDownloadDialog* dialog, BaseInstance& instance);
}  // namespace Gravity

}  // namespace ResourceDownload
