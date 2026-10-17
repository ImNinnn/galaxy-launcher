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

#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QPixmap>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <optional>

#include <memory>

#include "CelestialBody.h"
#include "CelestialPainter.h"
#include "GalaxyLayout.h"

class MinecraftInstance;
class SystemContent;
class QLabel;
class QPushButton;

/**
 * The interactive galaxy canvas.
 *
 * Shows every profile as a solar system on an angled plane (orthographic projection, 45 degree elevation), and a
 * single profile's installed content when the user enters its system. Everything drawn comes from Prism's instance
 * list and resource models; the view itself never changes files. It reports user intent through signals and
 * MainWindow performs the actual operations through Prism's existing code paths.
 */
class GalaxyView : public QWidget {
    Q_OBJECT
   public:
    explicit GalaxyView(QWidget* parent = nullptr);
    ~GalaxyView() override;

    enum class Mode { Galaxy, System };
    Mode mode() const { return m_targetMode; }

    /** The profile whose system is shown (or being entered). Empty in galaxy mode. */
    QString currentSystemId() const { return m_targetMode == Mode::System ? m_systemId : QString(); }

    void enterSystem(const QString& instanceId, bool animate = true);
    /** Back to the galaxy. A positive duration overrides the transition length (used when switching systems). */
    void showGalaxy(bool animate = true, qreal duration = 0);

    /** Highlights the profile selected elsewhere (sidebar, list view). */
    void setSelectedInstance(const QString& instanceId);

    /** Dims everything that does not match. Matches by name, file name, author and kind. */
    void setFilter(const QString& text);

    /** How many profiles (galaxy) or installed items (inside a system) match the filter; -1 without a filter. */
    int matchCount() const;

    /** Moves keyboard focus to the next match and brings it into view. */
    void focusNextMatch();

    /** Cosmetic launch sequence for a profile. Never touches files and never delays the actual launch. */
    void playLaunchAnimation(const QString& instanceId);

    /** Whether frames are currently being produced (for tooling and tests). */
    bool isAnimating() const { return m_ticker.isActive(); }

    /** Re-reads the Galaxy settings (Settings > Galaxy). */
    void reloadSettings();

    /** Rescans the shown content. */
    void refresh();

    /** Re-reads profile names, colors and order (e.g. after a sun color change). */
    void refreshProfiles() { scheduleSystemsRebuild(); }

    /** The current content snapshot of the entered system (empty in galaxy mode). */
    QList<CelestialBody> currentBodies() const;

    /** Where things were last drawn, in widget coordinates (null if not visible). For tooling and tests. */
    QPointF systemScreenPosition(const QString& instanceId) const;
    QPointF bodyScreenPosition(const QString& bodyKey) const;
    QPointF sunScreenPosition() const;
    QPointF blackHoleScreenPosition() const;
    /** The key of the body a click at 'pos' would pick (empty for the sun, a system or nothing). */
    QString bodyKeyAt(const QPointF& pos) const;

   signals:
    /** Mode or shown system changed; also emitted when a transition finishes. */
    void navigationChanged();
    void instanceSelected(const QString& instanceId);
    void launchRequested(const QString& instanceId);
    void sunContextMenuRequested(const QString& instanceId, const QPoint& globalPos);
    void bodyActivated(const QString& instanceId, const CelestialBody& body);
    void bodyContextMenuRequested(const QString& instanceId, const CelestialBody& body, const QPoint& globalPos);
    void createProfileRequested();
    void importProfileRequested();
    /** Profile names, colors or running states changed. */
    void profilesUpdated();
    /** The number of filter matches changed (see matchCount()). */
    void filterMatchesChanged(int matches);

   protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool focusNextPrevChild(bool next) override;
    void focusInEvent(QFocusEvent* event) override;
    bool event(QEvent* event) override;

   private:
    struct OverviewSummary {
        bool valid = false;
        int mods = 0;
        int packs = 0;
        int shaders = 0;
        QList<qint64> planetSizes;  ///< Largest enabled mods first, capped for the overview.
        QList<QString> planetKeys;
        QStringList allNames;  ///< Every installed file name, for searching across profiles.
    };

    struct SystemNode {
        QString id;
        QPointer<MinecraftInstance> instance;
        QString name;
        QString subtitle;
        QColor color;
        QPointF pos;      ///< Target position on the galaxy plane.
        QPointF fromPos;  ///< Position when the last move started.
        qreal moveStart = -1;
        OverviewSummary summary;
        bool running = false;
        qreal paused = 0;  ///< Orbit time this system's planets spent paused (hovered).
        qreal lag = 0;     ///< Galactic rotation time this system is behind: it stays still while hovered, then catches up.
    };

    /** Animated per-node state for the entered system. */
    struct NodeState {
        qreal radiusOffset = 0;  ///< Decays to zero after a relayout so bodies glide to their new orbits.
        qreal angleOffset = 0;
        qreal tweenStart = -1;
        qreal appearStart = -1;  ///< Fade-in start for newly installed content.
        qreal paused = 0;        ///< Orbit time spent paused while hovered.
    };

    enum class HitType { None, System, Sun, Body, BlackHole };
    struct Hit {
        HitType type = HitType::None;
        int index = -1;  ///< System index (galaxy) or node index (system).
        bool operator==(const Hit& o) const { return type == o.type && index == o.index; }
        bool operator!=(const Hit& o) const { return !(*this == o); }
    };

    struct HitShape {
        Hit hit;
        QPointF center;
        qreal radius = 0;  ///< Circle radius; for rings the band half width.
        qreal rx = 0;      ///< Ring ellipse (0 for circles).
        qreal ry = 0;
        qreal rotation = 0;
        bool frontHalfOnly = false;
        bool backHalfOnly = false;
        bool priority = false;  ///< A small circle that wins over other shapes, nearest first (sparkles in a nebula).
    };

    struct Camera {
        QPointF center;  ///< Galaxy plane point at the middle of the widget.
        qreal zoom = 60;  ///< Pixels per galaxy unit.
    };

    // Data
    void rebuildSystems();
    void scheduleSystemsRebuild();
    void requestSummary(int systemIndex);
    void onContentChanged();
    void relayoutSystem(bool animate);
    int systemIndexOf(const QString& id) const;

    // Time and animation
    qreal now() const;
    void tick();
    void updateTicker();
    bool wantsContinuousFrames() const;
    /** The launcher is visible and in front (not minimized, window or application active). */
    bool inForeground() const;
    qreal transitionProgress() const;  ///< Eased 0 (galaxy) .. 1 (system).

    // Geometry
    Camera galaxyFitCamera() const;
    Camera systemApproachCamera(int systemIndex) const;
    Camera currentGalaxyCamera() const;
    qreal systemFitZoom() const;
    QPointF projectGalaxy(const Camera& cam, qreal x, qreal y, qreal z) const;
    QPointF systemPosition(int index) const;
    QPointF systemPosition(int index, qreal lag) const;
    /** Whether system 'index' may move to its position at 'lag' without crowding another system. */
    bool systemLagAllowed(int index, qreal lag) const;
    QPointF nodePosition(int node) const;  ///< Local plane position (x, z) at the current time.
    qreal nodeAngle(int node) const;
    qreal nodeOrbitRadius(int node) const;

    // Painting
    void paintBackground(QPainter& p, qreal transition);
    void paintGalaxyLayer(QPainter& p, qreal fade);
    void paintBlackHole(QPainter& p, const Camera& cam, qreal fade);
    QString blackHoleName() const;
    /** The profile a click on the black hole launches: the chosen one, else the last played (else the selected one). */
    QString blackHoleTarget() const;
    /** Black hole click: a beam to the target system, then the usual launch. */
    void launchFromBlackHole();
    void paintBeam(QPainter& p, const Camera& cam, qreal fade);
    void renameBlackHole();
    void paintSystemLayer(QPainter& p, qreal alpha, QPointF origin, qreal zoom);
    void paintTooltip(QPainter& p);
    void paintStatus(QPainter& p);
    void paintLabel(QPainter& p, const QPointF& anchor, const QString& text, qreal opacity, bool emphasized);
    int quality() const;
    /** Draws a lit sphere through a sprite cache keyed by style, size and quantized lighting. */
    void drawCachedSphere(QPainter& p,
                          QPointF center,
                          qreal radius,
                          const CelestialPainter::SurfaceStyle& style,
                          QPointF light,
                          qreal facing,
                          bool disabled,
                          qreal opacity);

    // Interaction
    Hit hitTest(const QPointF& pos) const;
    Hit hitTest(const QPointF& pos, bool priority) const;  ///< Top-most match among shapes with this priority.
    void setHover(const Hit& hit);
    void activate(const Hit& hit);
    void requestContextMenu(const Hit& hit, const QPoint& globalPos);
    QList<Hit> focusOrder() const;
    QList<Hit> matchingHits() const;
    void bringIntoView(const Hit& hit);
    QPointF hitAnchor(const Hit& hit) const;
    bool matchesFilter(const CelestialBody& body) const;
    bool systemMatchesFilter(const SystemNode& node) const;
    void updateEmptyState();
    void updateAccessibleText();

    // Systems
    QList<SystemNode> m_systems;
    QTimer m_systemsRebuildTimer;
    QHash<QString, OverviewSummary> m_summaryCache;
    QString m_selectedId;

    // Entered system
    QString m_systemId;
    std::unique_ptr<SystemContent> m_content;
    QList<CelestialBody> m_bodies;
    GalaxyLayout::SystemLayout m_layout;
    QList<NodeState> m_nodeStates;
    QHash<QString, int> m_nodeByKey;
    QHash<QString, CelestialPainter::SurfaceStyle> m_styleCache;

    // Navigation
    Mode m_targetMode = Mode::Galaxy;
    QString m_pendingSystem;  ///< Entered once the current system has been left (switching between systems).
    qreal m_transitionFrom = 0;
    qreal m_transitionTo = 0;
    qreal m_transitionStart = -1;
    qreal m_transitionDuration = 0.85;
    Camera m_galaxyCamera;        ///< User-controlled galaxy camera.
    bool m_galaxyCameraFitted = true;
    Camera m_transitionStartCam;  ///< Galaxy camera when entering started.
    qreal m_systemZoom = 1.0;     ///< User zoom multiplier inside a system.
    QPointF m_systemPan;          ///< User pan inside a system, in pixels.

    // Launch animation
    QString m_launchId;
    qreal m_launchStart = -1;
    QString m_beamId;       ///< Profile the black hole's beam is travelling to.
    qreal m_beamStart = -1;
    bool m_beamFired = false;  ///< The launch was requested when the beam arrived.
    qreal m_labelFade = 1;  ///< Opacity of project names in the entered system; follows labelTarget().


    // Now and then, far behind an entered system, the black hole or another profile drifts by.
    struct Passer {
        QString systemId;  ///< Empty for the black hole.
        qreal start = 0;
        qreal duration = 30;
        QPointF from;  ///< Fractions of the view; the path starts and ends off screen.
        QPointF to;
        qreal size = 20;  ///< Pixels.
    };
    std::optional<Passer> m_passer;
    qreal m_nextPass = -1;  ///< When the next one may appear (in system view).
    void updatePasser(qreal t);
    void paintPasser(QPainter& p, qreal alpha);

    // A playing profile's sun becomes a nebula and its content sparkles; it reforms when the game stops.
    struct NebulaState {
        qreal formStart = -1;
        bool fromSun = false;  ///< Exploding out of the sun (after the launch pull-in) rather than from the orbits.
        qreal reformStart = -1;
    };
    struct NebulaMix {
        qreal form = 0;    ///< 0..1 explosion progress
        qreal reform = 0;  ///< 0..1 reforming progress
        bool fromSun = false;
        qreal amount() const { return form * (1 - reform); }
        bool active() const { return form > 0; }
    };
    QHash<QString, NebulaState> m_nebulae;
    void syncNebulae();
    NebulaMix nebulaMix(const QString& instanceId) const;
    bool nebulaAnimating() const;
    /** 0 while the entered profile launches or plays (its sparkles are unnamed), 1 otherwise. */
    qreal labelTarget() const;

    // Clock
    QElapsedTimer m_clock;
    qreal m_lastTick = 0;
    qreal m_orbitTime = 0;
    qreal m_galaxyTime = 0;  ///< Galactic rotation (a hovered profile lags behind it, see SystemNode::lag).
    QTimer m_ticker;
    QTimer m_watchdog;  ///< Restarts frames if an activation change was missed.
    bool m_windowSignalsConnected = false;

    // Interaction state
    Hit m_hover;
    Hit m_focus;
    QPointF m_mousePos{ -1, -1 };
    bool m_mouseInside = false;
    QPoint m_pressPos;
    QPointF m_dragLast;
    bool m_dragging = false;
    Qt::MouseButton m_pressButton = Qt::NoButton;
    mutable QList<HitShape> m_hitShapes;
    QString m_filter;
    bool m_keyboardFocus = false;  ///< Focus arrived via the keyboard: show the focus frame.

    // Settings
    bool m_launchAnimation = true;
    bool m_reducedMotion = false;
    bool m_orbitAnimation = true;
    bool m_showOrbits = true;
    bool m_backgroundMotion = true;
    int m_renderQuality = 2;
    int m_starfieldQuality = 2;
    qreal m_objectScale = 1.0;
    qreal m_spacing = 1.0;
    qreal m_zoomSpeed = 1.0;

    // Sprite cache for spheres (cleared when it grows too large or the screen scale changes)
    QHash<quint64, QPixmap> m_sphereSprites;
    qreal m_spriteDpr = 0;

    // Cached background: a sky base (with a margin to shift) and two tileable star layers for parallax
    QPixmap m_skyBase;
    QPixmap m_starTiles[2];
    qreal m_driftTime = 0;  ///< Advances only while the background is allowed to move.
    QSize m_starfieldSize;
    qreal m_starfieldDpr = 0;
    int m_starfieldBuiltQuality = -1;

    // Empty state
    QWidget* m_emptyState = nullptr;
};
