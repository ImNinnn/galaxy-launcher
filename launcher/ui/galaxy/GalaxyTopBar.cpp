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

#include "GalaxyTopBar.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QToolButton>
#include <QWindow>

#include "Application.h"
#include "BuildConfig.h"

namespace {

/** Window button glyphs, drawn so they look the same with every icon theme. */
QIcon windowGlyph(const QString& kind)
{
    QPixmap pixmap(QSize(32, 32));
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(236, 233, 255), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    if (kind == "minimize") {
        p.drawLine(QPointF(9, 17), QPointF(23, 17));
    } else if (kind == "maximize") {
        p.drawRoundedRect(QRectF(9, 9, 14, 14), 2, 2);
    } else if (kind == "restore") {
        p.drawRoundedRect(QRectF(8, 12, 12, 12), 2, 2);
        p.drawPolyline(QPolygonF({ QPointF(12, 12), QPointF(12, 8), QPointF(24, 8), QPointF(24, 20), QPointF(20, 20) }));
    } else {
        p.drawLine(QPointF(10, 10), QPointF(22, 22));
        p.drawLine(QPointF(22, 10), QPointF(10, 22));
    }
    return QIcon(pixmap);
}

}  // namespace

GalaxyTopBar::GalaxyTopBar(QWidget* parent) : QWidget(parent)
{
    setObjectName("galaxyTopBar");
    setAttribute(Qt::WA_StyledBackground);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 5, 10, 5);
    layout->setSpacing(6);

    auto* logo = new QLabel(this);
    logo->setPixmap(APPLICATION->logo().pixmap(QSize(26, 26)));
    logo->setAccessibleName(BuildConfig.LAUNCHER_DISPLAYNAME);
    m_brand = new QLabel(BuildConfig.LAUNCHER_DISPLAYNAME, this);
    m_brand->setObjectName("galaxyBrand");
    layout->addWidget(logo);
    layout->addWidget(m_brand);
    layout->addSpacing(12);

    m_back = new QToolButton(this);
    m_back->setObjectName("galaxyBackButton");
    m_back->setText(tr("Back"));
    m_back->setIcon(QIcon::fromTheme("arrow-left", QIcon::fromTheme("go-previous")));
    m_back->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_back->setToolTip(tr("Back to the galaxy (Esc)"));
    m_back->setAutoRaise(true);
    layout->addWidget(m_back);
    connect(m_back, &QToolButton::clicked, this, &GalaxyTopBar::backRequested);

    m_location = new QLabel(this);
    m_location->setObjectName("galaxyLocation");
    m_location->setTextFormat(Qt::RichText);
    m_location->setMinimumWidth(80);
    m_location->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(m_location, 1);

    m_context = new QWidget(this);
    m_contextLayout = new QHBoxLayout(m_context);
    m_contextLayout->setContentsMargins(0, 0, 0, 0);
    m_contextLayout->setSpacing(4);
    layout->addWidget(m_context);

    auto* separator = new QWidget(this);
    separator->setFixedWidth(10);
    layout->addWidget(separator);

    m_globalLayout = new QHBoxLayout();
    m_globalLayout->setSpacing(2);
    layout->addLayout(m_globalLayout);

    setLocation(tr("Galaxy"), QString(), false);
}

void GalaxyTopBar::setLocation(const QString& area, const QString& profile, bool canGoBack)
{
    m_back->setVisible(canGoBack);
    if (profile.isEmpty()) {
        m_location->setText(QStringLiteral("<span style=\"color:#b8b2d9\">%1</span>").arg(area.toHtmlEscaped()));
        m_location->setAccessibleName(area);
    } else {
        m_location->setText(QStringLiteral("<span style=\"color:#b8b2d9\">%1 &rsaquo;</span> <b>%2</b>")
                                .arg(area.toHtmlEscaped(), profile.toHtmlEscaped()));
        m_location->setAccessibleName(QStringLiteral("%1, %2").arg(area, profile));
    }
    m_location->setToolTip(profile);
}

QToolButton* GalaxyTopBar::addContextAction(QAction* action, bool textBesideIcon)
{
    auto* button = new QToolButton(m_context);
    button->setDefaultAction(action);
    button->setToolButtonStyle(textBesideIcon ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly);
    button->setAutoRaise(true);
    if (action->menu()) {
        button->setPopupMode(QToolButton::MenuButtonPopup);
    }
    if (textBesideIcon) {
        m_textButtons.append(button);
    }
    m_contextLayout->addWidget(button);
    return button;
}

QToolButton* GalaxyTopBar::addGlobalAction(QAction* action, bool textBesideIcon)
{
    auto* button = new QToolButton(this);
    button->setDefaultAction(action);
    button->setToolButtonStyle(textBesideIcon ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly);
    button->setAutoRaise(true);
    if (action->menu()) {
        button->setPopupMode(QToolButton::InstantPopup);
    }
    m_globalLayout->addWidget(button);
    return button;
}

void GalaxyTopBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Narrow windows: keep the location readable by dropping the brand text and button labels (tooltips remain).
    const bool compact = event->size().width() < 1100;
    m_brand->setVisible(!compact);
    for (auto* button : std::as_const(m_textButtons)) {
        button->setToolButtonStyle(compact ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
    }
    m_back->setToolButtonStyle(compact ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
}

void GalaxyTopBar::setWindowControlsVisible(bool visible)
{
    if (!m_windowControls) {
        m_windowControls = new QWidget(this);
        auto* controls = new QHBoxLayout(m_windowControls);
        controls->setContentsMargins(8, 0, 0, 0);
        controls->setSpacing(2);
        auto addButton = [this, controls](const QString& kind, const QString& name, const QString& objectName) {
            auto* button = new QToolButton(m_windowControls);
            button->setObjectName(objectName);
            button->setIcon(windowGlyph(kind));
            button->setIconSize(QSize(16, 16));
            button->setToolTip(name);
            button->setAccessibleName(name);
            button->setAutoRaise(true);
            button->setFocusPolicy(Qt::NoFocus);
            controls->addWidget(button);
            return button;
        };
        auto* minimize = addButton("minimize", tr("Minimize"), "galaxyWindowButton");
        m_maximize = addButton("maximize", tr("Maximize"), "galaxyWindowButton");
        auto* close = addButton("close", tr("Close"), "galaxyCloseButton");
        connect(minimize, &QToolButton::clicked, this, [this] { window()->showMinimized(); });
        connect(m_maximize, &QToolButton::clicked, this, [this] { window()->isMaximized() ? window()->showNormal() : window()->showMaximized(); });
        connect(close, &QToolButton::clicked, this, [this] { window()->close(); });
        layout()->addWidget(m_windowControls);
    }
    m_windowControls->setVisible(visible);
    updateWindowState();
}

void GalaxyTopBar::updateWindowState()
{
    if (!m_maximize) {
        return;
    }
    const bool maximized = window()->isMaximized();
    m_maximize->setIcon(windowGlyph(maximized ? "restore" : "maximize"));
    m_maximize->setToolTip(maximized ? tr("Restore") : tr("Maximize"));
    m_maximize->setAccessibleName(m_maximize->toolTip());
}

void GalaxyTopBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_windowControls && m_windowControls->isVisible()) {
        if (auto* handle = window()->windowHandle(); handle && handle->startSystemMove()) {
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void GalaxyTopBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_windowControls && m_windowControls->isVisible()) {
        window()->isMaximized() ? window()->showNormal() : window()->showMaximized();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void GalaxyTopBar::setContextVisible(bool visible)
{
    m_context->setVisible(visible);
}
