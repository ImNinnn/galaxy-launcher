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

#include <QPointer>
#include <QWidget>

#include <memory>

class MinecraftInstance;
class PageContainer;
class QLabel;
class QVBoxLayout;
class BasePageProvider;

/**
 * List view companion: the selected profile's installed content, using Prism's own instance pages (Mods, Resource
 * Packs, Shader Packs, Data Packs, Texture Packs). They operate on the same models as the galaxy, so a change in
 * either view shows up in the other.
 */
class ProfileContentsPanel : public QWidget {
    Q_OBJECT
   public:
    explicit ProfileContentsPanel(QWidget* parent = nullptr);
    ~ProfileContentsPanel() override;

    void setInstance(MinecraftInstance* instance);
    MinecraftInstance* instance() const { return m_instance; }

    /** Saves page state and closes the pages; call before hiding the panel. */
    void release();

   private:
    QPointer<MinecraftInstance> m_instance;
    std::unique_ptr<BasePageProvider> m_provider;
    PageContainer* m_container = nullptr;
    QLabel* m_header = nullptr;
    QLabel* m_placeholder = nullptr;
    QVBoxLayout* m_layout = nullptr;
};
