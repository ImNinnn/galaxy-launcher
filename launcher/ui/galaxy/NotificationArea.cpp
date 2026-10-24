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

#include "NotificationArea.h"

#include <QEnterEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QRegion>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "Application.h"
#include "GalaxySettings.h"
#include "Notifications.h"
#include "settings/SettingsObject.h"

using Kind = Notifications::Kind;

namespace {

QColor kindColor(Kind kind)
{
    switch (kind) {
        case Kind::Success:
            return { 134, 239, 172 };
        case Kind::Warning:
            return { 251, 191, 36 };
        case Kind::Error:
            return { 248, 113, 113 };
        case Kind::Info:
        case Kind::Progress:
            break;
    }
    return { 167, 139, 250 };
}

QString kindName(Kind kind)
{
    switch (kind) {
        case Kind::Success:
            return QObject::tr("Done");
        case Kind::Warning:
            return QObject::tr("Warning");
        case Kind::Error:
            return QObject::tr("Error");
        case Kind::Progress:
            return QObject::tr("In progress");
        case Kind::Info:
            break;
    }
    return QObject::tr("Information");
}

}  // namespace

/** One notification: an accent bar in the kind's color, title, text, progress, actions and a close button. */
class NotificationCard : public QFrame {
   public:
    NotificationCard(int id, QWidget* parent) : QFrame(parent), m_id(id)
    {
        setObjectName("galaxyNotification");
        setAttribute(Qt::WA_StyledBackground);
        auto* outer = new QHBoxLayout(this);
        outer->setContentsMargins(0, 0, 6, 0);
        outer->setSpacing(10);
        m_accent = new QFrame(this);
        m_accent->setFixedWidth(4);
        outer->addWidget(m_accent);

        auto* body = new QVBoxLayout();
        body->setContentsMargins(0, 10, 0, 10);
        body->setSpacing(4);
        m_title = new QLabel(this);
        m_title->setObjectName("galaxyNotificationTitle");
        m_title->setWordWrap(true);
        m_text = new QLabel(this);
        m_text->setObjectName("galaxyNotificationText");
        m_text->setWordWrap(true);
        m_text->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_progress = new QProgressBar(this);
        m_progress->setObjectName("galaxyNotificationProgress");
        m_progress->setTextVisible(false);
        m_progress->setFixedHeight(5);
        m_actions = new QHBoxLayout();
        m_actions->setContentsMargins(0, 4, 0, 0);
        m_actions->setSpacing(6);
        body->addWidget(m_title);
        body->addWidget(m_text);
        body->addWidget(m_progress);
        body->addLayout(m_actions);
        outer->addLayout(body, 1);

        auto* close = new QToolButton(this);
        close->setObjectName("galaxyNotificationClose");
        close->setText(QStringLiteral("✕"));
        close->setAutoRaise(true);
        close->setToolTip(tr("Close"));
        close->setAccessibleName(tr("Close notification"));
        outer->addWidget(close, 0, Qt::AlignTop);
        connect(close, &QToolButton::clicked, this, [this] { Notifications::instance()->dismiss(m_id); });

        m_timer.setSingleShot(true);
        connect(&m_timer, &QTimer::timeout, this, [this] { Notifications::instance()->dismiss(m_id); });
        refresh();
    }

    void refresh()
    {
        const auto notification = Notifications::instance()->find(m_id);
        if (!notification) {
            return;
        }
        const QColor color = kindColor(notification->kind);
        m_accent->setStyleSheet(QStringLiteral("background: %1; border-radius: 2px;").arg(color.name()));
        m_title->setText(notification->title);
        m_text->setText(notification->text);
        m_text->setVisible(!notification->text.isEmpty());
        const bool progress = notification->kind == Kind::Progress;
        m_progress->setVisible(progress);
        if (progress) {
            m_progress->setRange(0, notification->progress < 0 ? 0 : 100);  // a busy bar until progress is known
            m_progress->setValue(std::max(0, notification->progress));
        }
        while (auto* item = m_actions->takeAt(0)) {
            if (auto* widget = item->widget()) {
                widget->hide();
                widget->deleteLater();  // may be the button whose click led here
            }
            delete item;
        }
        for (const auto& action : notification->actions) {
            auto* button = new QPushButton(action.label, this);
            button->setObjectName("galaxyNotificationAction");
            button->setCursor(Qt::PointingHandCursor);
            connect(button, &QPushButton::clicked, this, [this, run = action.run, finished = !progress] {
                const int id = m_id;
                if (run) {
                    run();
                }
                if (finished) {
                    Notifications::instance()->dismiss(id);  // done with it
                }
            });
            m_actions->addWidget(button);
            button->show();  // now, not on the next event loop turn, so the card's height includes it
        }
        m_actions->addStretch();
        setAccessibleName(QStringLiteral("%1: %2").arg(kindName(notification->kind), notification->title));
        setAccessibleDescription(notification->text);

        m_timeout = notification->timeout < 0 ? Notifications::defaultTimeout(notification->kind) : notification->timeout;
        if (m_timeout > 0 && !underMouse()) {
            m_timer.start(m_timeout);
        } else {
            m_timer.stop();
        }
    }

    /** Called when the card's content needs a different height. */
    std::function<void()> relayout;

   protected:
    bool event(QEvent* event) override
    {
        const bool result = QFrame::event(event);
        if (event->type() == QEvent::LayoutRequest && relayout && !m_relayouting) {
            m_relayouting = true;
            relayout();
            m_relayouting = false;
        }
        return result;
    }

    void enterEvent(QEnterEvent* event) override
    {
        m_timer.stop();  // reading it: keep it
        QFrame::enterEvent(event);
    }
    void leaveEvent(QEvent* event) override
    {
        if (m_timeout > 0) {
            m_timer.start(std::max(3000, m_timeout / 2));
        }
        QFrame::leaveEvent(event);
    }

   private:
    int m_id;
    int m_timeout = 0;
    bool m_relayouting = false;
    QFrame* m_accent;
    QLabel* m_title;
    QLabel* m_text;
    QProgressBar* m_progress;
    QHBoxLayout* m_actions;
    QTimer m_timer;
};

NotificationArea::NotificationArea(QWidget* parent) : QWidget(parent)
{
    setObjectName("galaxyNotificationArea");
    setStyleSheet(QStringLiteral(
        "QFrame#galaxyNotification { background: #1A1438; border: 1px solid #3A2F70; border-radius: 10px; }"
        "QLabel#galaxyNotificationTitle { color: #ECE9FF; font-weight: bold; }"
        "QLabel#galaxyNotificationText { color: #BDB6E0; }"
        "QProgressBar#galaxyNotificationProgress { background: #2C2358; border: 0; border-radius: 2px; }"
        "QProgressBar#galaxyNotificationProgress::chunk { background: #A78BFA; border-radius: 2px; }"
        "QPushButton#galaxyNotificationAction { color: #C4B5FD; background: #2A2152; border: 1px solid #4C3D96; border-radius: 6px;"
        " padding: 3px 10px; }"
        "QPushButton#galaxyNotificationAction:hover { background: #352A68; }"
        // The close button has the soft red background of the top bar's stop button.
        "QToolButton#galaxyNotificationClose { color: #F3D2D2; background: rgba(248, 113, 113, 34); border: 1px solid rgba(248, 113, 113, 85);"
        " border-radius: 6px; padding: 1px 6px; margin-top: 8px; }"
        "QToolButton#galaxyNotificationClose:hover { background: rgba(248, 113, 113, 70); border-color: rgba(252, 165, 165, 150); }"
        "QToolButton#galaxyNotificationClose:pressed { background: rgba(220, 38, 38, 110); }"));
    parent->installEventFilter(this);
    setGeometry(parent->rect());

    auto* notifications = Notifications::instance();
    notifications->setShown(true);  // Prism's progress windows become notifications from now on
    connect(this, &QObject::destroyed, notifications, [notifications] { notifications->setShown(false); });
    connect(notifications, &Notifications::posted, this, &NotificationArea::add);
    connect(notifications, &Notifications::changed, this, &NotificationArea::refresh);
    connect(notifications, &Notifications::dismissed, this, &NotificationArea::remove);
    for (const auto& notification : notifications->active()) {
        add(notification.id);
    }
    updateMask();
}

namespace {
constexpr int CardWidth = 340;
constexpr int Margin = 14;
constexpr int Spacing = 8;

/** The card's height for the fixed width, so wrapped text takes exactly the lines it needs. */
void fitCard(QWidget* card)
{
    card->layout()->activate();
    const int height = card->hasHeightForWidth() ? card->heightForWidth(CardWidth) : card->sizeHint().height();
    card->resize(CardWidth, height);
}
}  // namespace

bool NotificationArea::reducedMotion() const
{
    return APPLICATION->settings()->get(GalaxySettings::ReducedMotion).toBool();
}

bool NotificationArea::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parent() && event->type() == QEvent::Resize) {
        setGeometry(parentWidget()->rect());
        arrange(false);
    }
    return QWidget::eventFilter(watched, event);
}

void NotificationArea::add(int id)
{
    auto* card = new NotificationCard(id, this);
    card->setFixedWidth(CardWidth);
    fitCard(card);
    card->relayout = [this, card] {
        fitCard(card);
        arrange(true);
    };
    m_cards.insert(id, card);
    m_order.append(id);
    // Enter from beyond the right edge, at its place in the stack.
    int y = Margin - 2;
    for (int other : std::as_const(m_order)) {
        if (other == id) {
            break;
        }
        y += m_cards[other]->height() + Spacing;
    }
    card->move(width() + 4, y);
    card->show();
    show();
    raise();
    arrange(true);
}

void NotificationArea::refresh(int id)
{
    if (auto* card = m_cards.value(id)) {
        card->refresh();
        fitCard(card);
        arrange(true);
    }
}

void NotificationArea::remove(int id)
{
    auto* card = m_cards.take(id);
    m_order.removeAll(id);
    if (!card) {
        return;
    }
    card->relayout = nullptr;
    // Leave to the right, then let the rest close the gap.
    card->setAttribute(Qt::WA_TransparentForMouseEvents);
    slide(card, QPoint(width() + 4, card->y()), true, [this, card] {
        card->hide();
        card->deleteLater();
        updateMask();
    });
    arrange(true);
}

void NotificationArea::arrange(bool animate)
{
    int y = Margin - 2;
    for (int id : std::as_const(m_order)) {
        auto* card = m_cards[id];
        slide(card, QPoint(width() - CardWidth - Margin, y), animate);
        y += card->height() + Spacing;
    }
    raise();
    updateMask();
}

void NotificationArea::slide(QWidget* card, const QPoint& to, bool animate, const std::function<void()>& done)
{
    auto* running = card->findChild<QPropertyAnimation*>(QStringLiteral("slide"), Qt::FindDirectChildrenOnly);
    if (running) {
        if (running->endValue().toPoint() == to && !done) {
            return;  // already on its way there
        }
        running->stop();
        running->deleteLater();
    }
    if (!animate || reducedMotion() || !isVisible()) {
        card->move(to);
        updateMask();
        if (done) {
            done();
        }
        return;
    }
    auto* animation = new QPropertyAnimation(card, "pos", card);
    animation->setObjectName(QStringLiteral("slide"));
    animation->setDuration(card->x() > width() - CardWidth / 2 || to.x() > width() ? 280 : 220);
    animation->setEasingCurve(to.x() > width() ? QEasingCurve::InCubic : QEasingCurve::OutCubic);
    animation->setStartValue(card->pos());
    animation->setEndValue(to);
    connect(animation, &QPropertyAnimation::valueChanged, this, &NotificationArea::updateMask);
    connect(animation, &QPropertyAnimation::finished, this, [this, done] {
        updateMask();
        if (done) {
            done();
        }
    });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void NotificationArea::updateMask()
{
    // Only the cards take clicks; elsewhere the galaxy or list below stays usable.
    QRegion region;
    for (auto* child : children()) {
        auto* card = qobject_cast<QWidget*>(child);
        if (card && card->objectName() == QStringLiteral("galaxyNotification") && card->isVisible()) {
            region += card->geometry().adjusted(-1, -1, 1, 1);
        }
    }
    setVisible(!region.isEmpty());
    if (!region.isEmpty()) {
        setMask(region);
    }
}
