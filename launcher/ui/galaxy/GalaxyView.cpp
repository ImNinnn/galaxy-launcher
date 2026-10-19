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

#include "GalaxyView.h"

#include <QActionGroup>
#include <QDir>
#include <QGuiApplication>
#include <QWindow>
#include <QFutureWatcher>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtConcurrent>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <set>

#include "Application.h"
#include "settings/SettingsObject.h"
#include "GalaxySettings.h"
#include "InstanceList.h"
#include "StringUtils.h"
#include "SunColor.h"
#include "SystemContent.h"
#include "minecraft/MinecraftInstance.h"

namespace {

// 45 degree elevation, orthographic: the plane's depth axis is foreshortened by sin(45°), heights by cos(45°).
constexpr qreal TiltSin = 0.7071067811865476;
constexpr qreal TiltCos = 0.7071067811865476;

constexpr qreal RingRatio = 0.36;      ///< Rings are tilted further than the orbit plane, like Saturn's.
constexpr qreal RingRotation = -12.0;  ///< Degrees on screen.
constexpr qreal ShadowDrop = 0.9;      ///< Height of the "table" below the orbit plane, local units.
constexpr qreal LaunchDuration = 1.6;
constexpr qreal ReducedLaunchDuration = 0.6;
constexpr qreal TweenDuration = 0.7;
constexpr qreal AppearDuration = 0.5;
constexpr qreal SystemMoveDuration = 0.6;
constexpr qreal SystemCatchUpRate = 0.75;
constexpr qreal BodyCatchUpRate = 1.0;  ///< After a hover, a body moves this much faster until it is back in place.
constexpr qreal ExileSpeed = 0.04;      ///< Radians per second on the shared orbit of disabled content.
constexpr qreal BeamArrive = 0.55;    ///< Seconds for the black hole's beam to reach the system (then it launches).
constexpr qreal BeamDuration = 1.15;  ///< Beam fully faded.  ///< After a hover, a profile turns this much faster until it is back in place.
constexpr qreal NebulaFormDuration = 1.2;
constexpr qreal NebulaReformDuration = 1.8;
constexpr int SkyMargin = 60;            ///< Pixels the nebula layer may shift in each direction.
constexpr int StarTileSize = 512;        ///< Logical pixels of one tileable star layer.
constexpr qreal ShootingStarPeriod = 37.0;
constexpr int OverviewPlanetLimit = 11;  ///< Plus the base game planet: four orbits of three.

const QColor Accent(167, 139, 250);
const QColor AccentStrong(139, 92, 246);
const QColor TextColor(236, 233, 255);
const QColor MutedText(184, 178, 217);
const QColor OrbitColor(176, 160, 255);

qreal clamp01(qreal v)
{
    return std::clamp(v, 0.0, 1.0);
}

QColor withAlphaF(QColor color, qreal alpha)
{
    color.setAlphaF(static_cast<float>(clamp01(alpha)));
    return color;
}

qreal smoothstep(qreal a, qreal b, qreal x)
{
    const qreal t = clamp01((x - a) / (b - a));
    return t * t * (3 - 2 * t);
}

qreal easeInOut(qreal t)
{
    t = clamp01(t);
    return t < 0.5 ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3) / 2;
}

qreal easeOut(qreal t)
{
    t = clamp01(t);
    return 1 - (1 - t) * (1 - t) * (1 - t);
}

qreal lerp(qreal a, qreal b, qreal t)
{
    return a + (b - a) * t;
}

qreal wrapAngle(qreal a)
{
    a = std::fmod(a + M_PI, 2 * M_PI);
    if (a < 0) {
        a += 2 * M_PI;
    }
    return a - M_PI;
}

/** How far a body has been pulled into the sun during the launch sequence. */
struct LaunchPull {
    qreal pull = 0;  ///< 0 = on its orbit, 1 = at the sun.
    qreal scale = 1;
    qreal alpha = 1;
    qreal trailFrom = 0;  ///< Pull value a moment earlier, for the trail.
};

LaunchPull launchPull(qreal t, qreal orbitFraction)
{
    LaunchPull out;
    if (t < 0) {
        return out;
    }
    if (t >= 1.15) {
        // The system reassembles: the animation is purely cosmetic and everything is still installed.
        out.alpha = smoothstep(1.15, LaunchDuration, t);
        return out;
    }
    auto pullAt = [orbitFraction](qreal time) {
        const qreal u = clamp01((time - 0.12 - 0.22 * orbitFraction) / 0.62);
        return u * u;
    };
    out.pull = pullAt(t);
    out.trailFrom = pullAt(t - 0.09);
    out.scale = 1 - 0.75 * out.pull;
    const qreal u = clamp01((t - 0.12 - 0.22 * orbitFraction) / 0.62);
    out.alpha = 1 - smoothstep(0.8, 1.0, u);
    return out;
}

/** Sun brightness and size during the launch sequence. */
std::pair<qreal, qreal> launchSun(qreal t, bool reduced)
{
    if (t < 0) {
        return { 1.0, 1.0 };
    }
    if (reduced) {
        const qreal bump = std::sin(M_PI * clamp01(t / ReducedLaunchDuration));
        return { 1.0 + 0.6 * bump, 1.0 };
    }
    const qreal emphasize = smoothstep(0, 0.3, t) * (1 - smoothstep(1.2, LaunchDuration, t));
    const qreal flash = std::exp(-std::pow((t - 1.0) / 0.13, 2));
    return { 1.0 + 0.3 * emphasize + 1.5 * flash, 1.0 + 0.1 * emphasize + 0.14 * flash };
}

QColor desaturated(const QColor& c)
{
    const int gray = qGray(c.rgb());
    return QColor((c.red() + gray * 2) / 3, (c.green() + gray * 2) / 3, (c.blue() + gray * 2) / 3).darker(125);
}

CelestialPainter::SurfaceStyle disabledStyle(CelestialPainter::SurfaceStyle style)
{
    style.base = desaturated(style.base);
    style.accent = desaturated(style.accent);
    return style;
}

/** A value in [0, 1) derived from 'seed' (well mixed, so nearby seeds give unrelated values). */
qreal rand01(std::uint32_t seed)
{
    seed ^= seed >> 16U;
    seed *= 0x7feb352dU;
    seed ^= seed >> 15U;
    seed *= 0x846ca68bU;
    seed ^= seed >> 16U;
    return seed / 4294967296.0;
}

std::uint32_t stableHash(const QString& key)
{
    std::uint32_t hash = 2166136261U;
    for (const QChar c : key) {
        hash ^= c.unicode();
        hash *= 16777619U;
    }
    hash ^= hash >> 16U;
    hash *= 0x85ebca6bU;
    hash ^= hash >> 13U;
    return hash;
}

/** Where a body rests inside a nebula: a fixed, scattered spot (sparkles do not orbit). */
QPointF sparklePosition(const QString& key, qreal radius)
{
    const auto hash = stableHash(key);
    const qreal angle = (hash % 3600U) / 3600.0 * 2 * M_PI;
    const qreal u = ((hash >> 12U) % 1000U) / 1000.0;
    const qreal distance = radius * (0.2 + 0.8 * std::sqrt(u));
    return QPointF(distance * std::cos(angle), distance * std::sin(angle));
}

qreal twinkle(const QString& key, qreal time)
{
    const auto hash = stableHash(key);
    return 0.5 + 0.5 * std::sin(time * (1.4 + (hash % 100U) / 60.0) + (hash % 628U) / 100.0);
}

/** Strips ".disabled" so toggling a file does not change its colors. */
QString styleKey(const QString& fileName)
{
    return fileName.endsWith(".disabled") ? fileName.chopped(9) : fileName;
}

QString humanSize(qint64 bytes)
{
    return StringUtils::humanReadableFileSize(static_cast<double>(bytes), true);
}

/** Greedy word wrap, so tooltip lines are never cut off. */
QStringList wrapText(const QString& text, const QFontMetricsF& fm, qreal width)
{
    QStringList out;
    QString line;
    for (const auto& word : text.split(' ', Qt::SkipEmptyParts)) {
        const QString candidate = line.isEmpty() ? word : line + ' ' + word;
        if (!line.isEmpty() && fm.horizontalAdvance(candidate) > width) {
            out << line;
            line = word;
        } else {
            line = candidate;
        }
    }
    if (!line.isEmpty()) {
        out << line;
    }
    // A single word longer than the width (e.g. a file name) is elided as a last resort.
    for (auto& l : out) {
        l = fm.elidedText(l, Qt::ElideMiddle, width);
    }
    return out;
}

}  // namespace

GalaxyView::GalaxyView(QWidget* parent) : QWidget(parent)
{
    setObjectName("galaxyView");
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(320, 240);
    setAccessibleName(tr("Galaxy"));

    m_clock.start();
    m_ticker.setTimerType(Qt::PreciseTimer);
    m_ticker.setInterval(16);
    connect(&m_ticker, &QTimer::timeout, this, &GalaxyView::tick);

    // Animations pause while the launcher is in the background and must resume reliably when it comes back.
    // Activation events are not delivered consistently to child widgets on every platform (notably Wayland), so
    // listen to the application state and the window itself, and keep a cheap watchdog as a safety net.
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this] { QTimer::singleShot(0, this, &GalaxyView::updateTicker); });
    m_watchdog.setInterval(1000);
    connect(&m_watchdog, &QTimer::timeout, this, [this] {
        if (!m_ticker.isActive()) {
            updateTicker();
        }
    });
    m_watchdog.start();

    m_systemsRebuildTimer.setSingleShot(true);
    m_systemsRebuildTimer.setInterval(50);
    connect(&m_systemsRebuildTimer, &QTimer::timeout, this, &GalaxyView::rebuildSystems);

    auto* instances = APPLICATION->instances();
    connect(instances, &QAbstractItemModel::rowsInserted, this, &GalaxyView::scheduleSystemsRebuild);
    connect(instances, &QAbstractItemModel::rowsRemoved, this, &GalaxyView::scheduleSystemsRebuild);
    connect(instances, &QAbstractItemModel::modelReset, this, &GalaxyView::scheduleSystemsRebuild);
    connect(instances, &QAbstractItemModel::dataChanged, this, &GalaxyView::scheduleSystemsRebuild);
    connect(instances, &InstanceList::instancesChanged, this, &GalaxyView::scheduleSystemsRebuild);

    // Empty galaxy: an invitation instead of invented content.
    m_emptyState = new QWidget(this);
    m_emptyState->setObjectName("galaxyEmptyState");
    m_emptyState->setAttribute(Qt::WA_StyledBackground);
    auto* layout = new QVBoxLayout(m_emptyState);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(10);
    auto* title = new QLabel(tr("Your galaxy is empty"), m_emptyState);
    title->setObjectName("galaxyEmptyTitle");
    title->setAlignment(Qt::AlignCenter);
    auto* text = new QLabel(tr("Every Minecraft profile becomes a solar system here.\n"
                               "Create a profile to light your first star, or import one from a modpack or another launcher."),
                            m_emptyState);
    text->setObjectName("galaxyEmptyText");
    text->setAlignment(Qt::AlignCenter);
    text->setWordWrap(true);
    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(10);
    auto* create = new QPushButton(tr("Create profile"), m_emptyState);
    create->setObjectName("galaxyPrimaryButton");
    create->setIcon(QIcon::fromTheme("new"));
    auto* import = new QPushButton(tr("Import profile"), m_emptyState);
    import->setIcon(QIcon::fromTheme("viewfolder"));
    buttons->addStretch();
    buttons->addWidget(create);
    buttons->addWidget(import);
    buttons->addStretch();
    layout->addWidget(title);
    layout->addWidget(text);
    layout->addSpacing(6);
    layout->addLayout(buttons);
    connect(create, &QPushButton::clicked, this, &GalaxyView::createProfileRequested);
    connect(import, &QPushButton::clicked, this, &GalaxyView::importProfileRequested);
    m_emptyState->hide();

    reloadSettings();
    rebuildSystems();
}

GalaxyView::~GalaxyView() = default;

// ---------------------------------------------------------------------------------------------------------------
// Settings

void GalaxyView::reloadSettings()
{
    auto* s = APPLICATION->settings();
    m_launchAnimation = s->get(GalaxySettings::LaunchAnimation).toBool();
    m_reducedMotion = s->get(GalaxySettings::ReducedMotion).toBool();
    m_orbitAnimation = s->get(GalaxySettings::OrbitAnimation).toBool();
    m_showOrbits = s->get(GalaxySettings::ShowOrbits).toBool();
    m_backgroundMotion = s->get(GalaxySettings::BackgroundMotion).toBool();
    m_renderQuality = std::clamp(s->get(GalaxySettings::RenderQuality).toInt(), 0, 2);
    m_starfieldQuality = std::clamp(s->get(GalaxySettings::StarfieldQuality).toInt(), 0, 2);
    const qreal objectScale = std::clamp(s->get(GalaxySettings::ObjectScale).toInt(), 50, 200) / 100.0;
    const qreal spacing = std::clamp(s->get(GalaxySettings::Spacing).toInt(), 50, 300) / 100.0;
    m_zoomSpeed = std::clamp(s->get(GalaxySettings::ZoomSpeed).toInt(), 25, 400) / 100.0;

    const bool relayout = !qFuzzyCompare(objectScale, m_objectScale);
    const bool respace = !qFuzzyCompare(spacing, m_spacing);
    m_objectScale = objectScale;
    m_spacing = spacing;
    if (relayout && m_content) {
        relayoutSystem(true);
    }
    if (respace) {
        m_galaxyCameraFitted = true;
        rebuildSystems();
    }
    m_starfieldBuiltQuality = -1;  // rebuild lazily
    m_sphereSprites.clear();
    updateTicker();
    update();
}

int GalaxyView::quality() const
{
    return m_renderQuality;
}

// ---------------------------------------------------------------------------------------------------------------
// Systems (profiles)

void GalaxyView::scheduleSystemsRebuild()
{
    if (!m_systemsRebuildTimer.isActive()) {
        m_systemsRebuildTimer.start();
    }
}

int GalaxyView::systemIndexOf(const QString& id) const
{
    for (int i = 0; i < m_systems.size(); i++) {
        if (m_systems[i].id == id) {
            return i;
        }
    }
    return -1;
}

void GalaxyView::rebuildSystems()
{
    auto* list = APPLICATION->instances();
    QList<MinecraftInstance*> instances;
    for (int i = 0; i < list->count(); i++) {
        if (auto* inst = list->at(i)) {
            instances.append(inst);
        }
    }
    std::sort(instances.begin(), instances.end(), [](MinecraftInstance* a, MinecraftInstance* b) {
        const int byName = QString::localeAwareCompare(a->name().toLower(), b->name().toLower());
        return byName != 0 ? byName < 0 : a->id() < b->id();
    });

    const auto positions = GalaxyLayout::layoutGalaxy(static_cast<int>(instances.size()), m_spacing);
    const qreal t = now();

    QHash<QString, SystemNode> previous;
    for (const auto& node : std::as_const(m_systems)) {
        if (node.instance) {
            disconnect(node.instance, nullptr, this, nullptr);
        }
        previous.insert(node.id, node);
    }

    QList<SystemNode> systems;
    for (int i = 0; i < instances.size(); i++) {
        auto* inst = instances[i];
        SystemContent::ensureComponentsLoaded(inst);
        SystemNode node;
        node.id = inst->id();
        node.instance = inst;
        node.name = inst->name();
        const auto loaders = SystemContent::loaderSummary(inst);
        node.subtitle = loaders.isEmpty() ? SystemContent::minecraftVersion(inst)
                                          : QStringLiteral("%1 · %2").arg(SystemContent::minecraftVersion(inst), loaders);
        node.color = SunColor::get(inst);
        node.pos = positions[i];
        node.running = inst->isRunning();
        if (auto it = previous.constFind(node.id); it != previous.constEnd()) {
            node.summary = it->summary;
            node.paused = it->paused;
            node.lag = it->lag;
            // Glide to the new spot when the ordering changed (rename, new profile).
            QPointF current = it->pos;
            if (it->moveStart >= 0) {
                current = it->fromPos + (it->pos - it->fromPos) * easeInOut((t - it->moveStart) / SystemMoveDuration);
            }
            if (QLineF(current, node.pos).length() > 1e-3 && !m_reducedMotion) {
                node.fromPos = current;
                node.moveStart = t;
            } else {
                node.fromPos = node.pos;
            }
        } else {
            node.fromPos = node.pos;
            node.summary = m_summaryCache.value(node.id);
        }
        connect(inst, &BaseInstance::runningStatusChanged, this, [this, id = node.id](bool running) {
            const int index = systemIndexOf(id);
            if (index < 0) {
                return;
            }
            m_systems[index].running = running;
            syncNebulae();
            if (!running) {
                // The game may have changed content (e.g. a mod updater); remap this system.
                requestSummary(index);
            }
            updateTicker();
            update();
            emit profilesUpdated();
        });
        systems.append(node);
    }
    m_systems = systems;

    // Only new profiles need a scan here; content changes, finished games and F5 refresh existing ones.
    for (int i = 0; i < m_systems.size(); i++) {
        if (!m_systems[i].summary.valid) {
            requestSummary(i);
        }
    }

    if (!m_systemId.isEmpty() && systemIndexOf(m_systemId) < 0) {
        // The shown profile was deleted or moved away.
        m_targetMode = Mode::Galaxy;
        m_transitionFrom = m_transitionTo = 0;
        m_transitionStart = -1;
        m_content.reset();
        m_bodies.clear();
        m_layout = {};
        m_systemId.clear();
    }

    if (m_galaxyCameraFitted) {
        m_galaxyCamera = galaxyFitCamera();
    }
    m_hover = {};
    m_focus = {};
    updateEmptyState();
    updateTicker();
    update();
    emit navigationChanged();
    emit profilesUpdated();
}

void GalaxyView::requestSummary(int systemIndex)
{
    if (systemIndex < 0 || systemIndex >= m_systems.size()) {
        return;
    }
    auto* inst = m_systems[systemIndex].instance.data();
    if (!inst) {
        return;
    }
    // Paths are resolved here, on the GUI thread; only plain directory listing happens in the worker.
    struct Dirs {
        QString mods, resourcePacks, texturePacks, shaderPacks, dataPacks;
    } dirs{ inst->modsRoot(), inst->resourcePacksDir(), inst->texturePacksDir(), inst->shaderPacksDir(),
            inst->settings()->get("GlobalDataPacksEnabled").toBool() ? inst->dataPacksDir() : QString() };
    const QString id = m_systems[systemIndex].id;

    auto* watcher = new QFutureWatcher<OverviewSummary>(this);
    connect(watcher, &QFutureWatcher<OverviewSummary>::finished, this, [this, watcher, id] {
        const auto summary = watcher->result();
        watcher->deleteLater();
        m_summaryCache.insert(id, summary);
        const int index = systemIndexOf(id);
        if (index >= 0) {
            m_systems[index].summary = summary;
            if (!m_filter.isEmpty()) {
                emit filterMatchesChanged(matchCount());
            }
            update();
        }
    });
    watcher->setFuture(QtConcurrent::run([dirs]() {
        OverviewSummary summary;
        summary.valid = true;
        auto entries = [](const QString& path) {
            if (path.isEmpty()) {
                return QFileInfoList();
            }
            return QDir(path).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        };
        auto enabled = [](const QFileInfo& info) { return !info.fileName().endsWith(".disabled"); };
        QList<std::pair<qint64, QString>> mods;
        for (const auto& info : entries(dirs.mods)) {
            const auto suffix = info.suffix().toLower();
            if (info.isFile() && enabled(info) && (suffix == "jar" || suffix == "zip" || suffix == "litemod")) {
                mods.append({ info.size(), info.fileName() });
            }
            summary.allNames << info.fileName();
        }
        summary.mods = static_cast<int>(mods.size());
        std::sort(mods.begin(), mods.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        for (int i = 0; i < std::min<qsizetype>(OverviewPlanetLimit, mods.size()); i++) {
            summary.planetSizes.append(mods[i].first);
            summary.planetKeys.append(mods[i].second);
        }
        auto isPack = [&enabled](const QFileInfo& info) {
            return enabled(info) && (info.isDir() || info.suffix().compare("zip", Qt::CaseInsensitive) == 0);
        };
        for (const auto& dir : { dirs.resourcePacks, dirs.texturePacks, dirs.dataPacks }) {
            for (const auto& info : entries(dir)) {
                summary.packs += isPack(info) ? 1 : 0;
                summary.allNames << info.fileName();
            }
        }
        for (const auto& info : entries(dirs.shaderPacks)) {
            summary.shaders += isPack(info) ? 1 : 0;
            summary.allNames << info.fileName();
        }
        return summary;
    }));
}

void GalaxyView::setSelectedInstance(const QString& instanceId)
{
    if (m_selectedId == instanceId) {
        return;
    }
    m_selectedId = instanceId;
    update();
}

void GalaxyView::setFilter(const QString& text)
{
    const auto filter = text.trimmed().toLower();
    if (filter == m_filter) {
        return;
    }
    m_filter = filter;
    m_focus = {};
    update();
    emit filterMatchesChanged(matchCount());
}

QList<GalaxyView::Hit> GalaxyView::matchingHits() const
{
    QList<Hit> hits;
    if (m_filter.isEmpty()) {
        return hits;
    }
    if (m_targetMode == Mode::System) {
        for (int n = 0; n < m_layout.nodes.size(); n++) {
            if (matchesFilter(m_bodies[m_layout.nodes[n].body])) {
                hits.append({ HitType::Body, n });
            }
        }
    } else {
        for (int i = 0; i < m_systems.size(); i++) {
            if (systemMatchesFilter(m_systems[i])) {
                hits.append({ HitType::System, i });
            }
        }
    }
    return hits;
}

int GalaxyView::matchCount() const
{
    return m_filter.isEmpty() ? -1 : static_cast<int>(matchingHits().size());
}

void GalaxyView::focusNextMatch()
{
    const auto hits = matchingHits();
    if (hits.isEmpty()) {
        return;
    }
    const int current = static_cast<int>(hits.indexOf(m_focus));
    m_focus = hits[(current + 1) % hits.size()];
    m_keyboardFocus = true;
    bringIntoView(m_focus);
    updateAccessibleText();
    update();
}

void GalaxyView::bringIntoView(const Hit& hit)
{
    const QPointF at = hitAnchor(hit);
    const QRectF safe = QRectF(rect()).adjusted(80, 60, -80, -80);
    if (safe.contains(at)) {
        return;
    }
    const QPointF shift = QRectF(rect()).center() - at;
    if (m_targetMode == Mode::System) {
        m_systemPan += shift;
    } else {
        m_galaxyCameraFitted = false;
        m_galaxyCamera.center -= QPointF(shift.x() / m_galaxyCamera.zoom, shift.y() / (m_galaxyCamera.zoom * TiltSin));
    }
}

bool GalaxyView::matchesFilter(const CelestialBody& body) const
{
    if (m_filter.isEmpty()) {
        return true;
    }
    const QString haystack =
        QStringList{ body.name, body.fileName, body.authors.join(' '), CelestialBody::kindName(body.kind), body.provider }.join(' ').toLower();
    const auto tokens = m_filter.split(' ', Qt::SkipEmptyParts);
    return std::all_of(tokens.begin(), tokens.end(), [&haystack](const QString& token) { return haystack.contains(token); });
}

bool GalaxyView::systemMatchesFilter(const SystemNode& node) const
{
    if (m_filter.isEmpty()) {
        return true;
    }
    const QString haystack = (node.name + ' ' + node.subtitle + ' ' + node.summary.allNames.join(' ')).toLower();
    const auto tokens = m_filter.split(' ', Qt::SkipEmptyParts);
    return std::all_of(tokens.begin(), tokens.end(), [&haystack](const QString& token) { return haystack.contains(token); });
}

void GalaxyView::updateEmptyState()
{
    const bool empty = m_systems.isEmpty();
    m_emptyState->setVisible(empty);
    if (empty) {
        const int w = std::min(520, width() - 32);
        const int h = m_emptyState->hasHeightForWidth() ? m_emptyState->heightForWidth(w) : m_emptyState->sizeHint().height();
        const QSize size(w, std::max(h, m_emptyState->minimumSizeHint().height()));
        m_emptyState->resize(size);
        // Below the black hole, which stays visible in an empty galaxy.
        m_emptyState->move((width() - size.width()) / 2, std::min(height() - size.height() - 16, height() / 2 + 90));
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Entering and leaving systems

void GalaxyView::enterSystem(const QString& instanceId, bool animate)
{
    const int index = systemIndexOf(instanceId);
    if (index < 0) {
        return;
    }
    if (m_targetMode == Mode::System && m_systemId == instanceId) {
        m_pendingSystem.clear();
        return;
    }
    // Switching from one system to another: fly out to the galaxy first, then into the new system.
    const bool leavingOther = !m_systemId.isEmpty() && m_systemId != instanceId &&
                              (m_targetMode == Mode::System || m_transitionStart >= 0);
    if (animate && leavingOther && !m_reducedMotion) {
        m_pendingSystem = instanceId;
        if (m_targetMode == Mode::System) {
            showGalaxy(true, 0.6);
        }
        return;
    }
    m_pendingSystem.clear();
    const qreal current = leavingOther ? 0.0 : transitionProgress();

    m_systemId = instanceId;
    m_labelFade = labelTarget();  // a profile that is already playing opens without names
    m_content = std::make_unique<SystemContent>(m_systems[index].instance.data());
    connect(m_content.get(), &SystemContent::changed, this, &GalaxyView::onContentChanged);
    m_bodies = m_content->bodies();
    m_nodeStates.clear();
    m_nodeByKey.clear();
    relayoutSystem(false);
    m_systemZoom = 1.0;
    m_systemPan = {};
    m_hover = {};
    m_focus = {};

    m_targetMode = Mode::System;
    m_transitionFrom = animate ? current : 1.0;
    m_transitionTo = 1.0;
    m_transitionDuration = m_reducedMotion ? 0.25 : 0.85;
    m_transitionStart = now();
    updateTicker();
    update();
    emit navigationChanged();
}

void GalaxyView::showGalaxy(bool animate, qreal duration)
{
    if (m_targetMode == Mode::Galaxy) {
        m_pendingSystem.clear();
        return;
    }
    if (duration <= 0) {
        m_pendingSystem.clear();  // an explicit "back" cancels a queued switch
    }
    m_targetMode = Mode::Galaxy;
    m_transitionFrom = animate ? transitionProgress() : 0.0;
    m_transitionTo = 0.0;
    m_transitionDuration = m_reducedMotion ? 0.25 : (duration > 0 ? duration : 0.75);
    m_transitionStart = now();
    // The zoom and pan used inside the system fade out with the transition (they are scaled by its progress), so the
    // camera glides back instead of jumping; they are reset once the galaxy is reached.
    m_hover = {};
    m_focus = {};
    if (!animate) {
        m_systemZoom = 1.0;
        m_systemPan = {};
        m_content.reset();
        m_bodies.clear();
        m_layout = {};
        m_systemId.clear();
    }
    updateTicker();
    update();
    emit navigationChanged();
}

qreal GalaxyView::transitionProgress() const
{
    if (m_transitionStart < 0) {
        return m_transitionTo;
    }
    const qreal t = clamp01((now() - m_transitionStart) / m_transitionDuration);
    return lerp(m_transitionFrom, m_transitionTo, easeInOut(t));
}

QList<CelestialBody> GalaxyView::currentBodies() const
{
    return m_targetMode == Mode::System ? m_bodies : QList<CelestialBody>();
}

QPointF GalaxyView::systemScreenPosition(const QString& instanceId) const
{
    const Hit hit{ HitType::System, systemIndexOf(instanceId) };
    for (const auto& shape : m_hitShapes) {
        if (shape.hit == hit) {
            return shape.center;
        }
    }
    return {};
}

QPointF GalaxyView::bodyScreenPosition(const QString& bodyKey) const
{
    const int node = m_nodeByKey.value(bodyKey, -1);
    for (const auto& shape : m_hitShapes) {
        if (shape.hit.type == HitType::Body && shape.hit.index == node) {
            // Rings: a point on the near half of the band.
            return shape.rx > 0 ? shape.center + QPointF(0, shape.ry) : shape.center;
        }
    }
    return {};
}

qreal GalaxyView::bodyLag(const QString& bodyKey) const
{
    const int node = m_nodeByKey.value(bodyKey, -1);
    return node >= 0 && node < m_nodeStates.size() ? m_nodeStates[node].paused : 0.0;
}

QPointF GalaxyView::blackHoleScreenPosition() const
{
    for (const auto& shape : m_hitShapes) {
        if (shape.hit.type == HitType::BlackHole) {
            return shape.center;
        }
    }
    return {};
}

QString GalaxyView::bodyKeyAt(const QPointF& pos) const
{
    const Hit hit = hitTest(pos);
    if (hit.type != HitType::Body || hit.index >= m_layout.nodes.size()) {
        return {};
    }
    return m_bodies.value(m_layout.nodes[hit.index].body).key;
}

QPointF GalaxyView::sunScreenPosition() const
{
    for (const auto& shape : m_hitShapes) {
        if (shape.hit.type == HitType::Sun) {
            return shape.center;
        }
    }
    return {};
}

void GalaxyView::refresh()
{
    if (m_content) {
        m_content->refresh();
    }
    for (int i = 0; i < m_systems.size(); i++) {
        requestSummary(i);
    }
    scheduleSystemsRebuild();
}

void GalaxyView::onContentChanged()
{
    if (!m_content) {
        return;
    }
    m_bodies = m_content->bodies();
    relayoutSystem(true);
    // Keep the overview in sync with what we just saw.
    const int index = systemIndexOf(m_systemId);
    if (index >= 0) {
        requestSummary(index);
    }
    if (!m_filter.isEmpty()) {
        emit filterMatchesChanged(matchCount());
    }
    update();
}

void GalaxyView::relayoutSystem(bool animate)
{
    // Remember where every body currently is, so a relayout glides instead of jumping.
    struct Was {
        qreal angle;
        qreal radius;
    };
    QHash<QString, Was> was;
    for (auto it = m_nodeByKey.constBegin(); it != m_nodeByKey.constEnd(); ++it) {
        if (it.value() < m_layout.nodes.size()) {
            was.insert(it.key(), { nodeAngle(it.value()), nodeOrbitRadius(it.value()) });
        }
    }

    GalaxyLayout::Params params;
    params.objectScale = m_objectScale;
    m_layout = GalaxyLayout::layoutSystem(m_bodies, params);
    m_nodeStates = QList<NodeState>(m_layout.nodes.size());
    m_nodeByKey.clear();

    const qreal t = now();
    const bool tween = animate && !m_reducedMotion;
    for (int n = 0; n < m_layout.nodes.size(); n++) {
        const auto& node = m_layout.nodes[n];
        const auto& key = m_bodies[node.body].key;
        m_nodeByKey.insert(key, n);
        auto it = was.constFind(key);
        if (it == was.constEnd()) {
            if (animate) {
                m_nodeStates[n].appearStart = t;
            }
            continue;
        }
        if (tween && node.role != GalaxyLayout::Node::Role::Ring) {
            auto& state = m_nodeStates[n];
            state.angleOffset = wrapAngle(it->angle - (node.phase + node.speed * m_orbitTime));
            state.radiusOffset = it->radius - node.orbitRadius;
            state.tweenStart = t;
        }
    }
    m_hover = {};
    m_focus = {};
    updateTicker();
}

// ---------------------------------------------------------------------------------------------------------------
// Animation clock

qreal GalaxyView::now() const
{
    return static_cast<qreal>(m_clock.nsecsElapsed()) / 1e9;
}

bool GalaxyView::wantsContinuousFrames() const
{
    const qreal t = now();
    if (m_transitionStart >= 0 && t - m_transitionStart < m_transitionDuration) {
        return true;
    }
    if (m_launchStart >= 0 || m_beamStart >= 0 || nebulaAnimating() || !qFuzzyCompare(m_labelFade, labelTarget())) {
        return true;
    }
    for (const auto& state : m_nodeStates) {
        if ((state.tweenStart >= 0 && t - state.tweenStart < TweenDuration) ||
            (state.appearStart >= 0 && t - state.appearStart < AppearDuration)) {
            return true;
        }
    }
    for (const auto& node : m_systems) {
        if (node.moveStart >= 0 && t - node.moveStart < SystemMoveDuration) {
            return true;
        }
    }
    // Orbital motion only while the launcher is in front: no background animation while playing.
    if (!inForeground() || m_reducedMotion) {
        return false;
    }
    if (m_backgroundMotion && m_starfieldQuality > 0) {
        return true;
    }
    if (!m_nebulae.isEmpty()) {
        return true;  // the clouds swirl and the sparkles twinkle
    }
    if (m_orbitAnimation && !m_systems.isEmpty()) {
        return true;
    }
    return std::any_of(m_systems.begin(), m_systems.end(), [](const SystemNode& n) { return n.running; });
}

bool GalaxyView::inForeground() const
{
    auto* top = window();
    if (!top || !isVisible() || (top->windowState() & Qt::WindowMinimized)) {
        return false;
    }
    return top->isActiveWindow() || QGuiApplication::applicationState() == Qt::ApplicationActive;
}

void GalaxyView::updateTicker()
{
    if (isVisible() && wantsContinuousFrames()) {
        // Slow orbital motion looks smooth at ~30 fps; transitions and the launch sequence get ~60 fps.
        const qreal t = now();
        bool fast = m_launchStart >= 0 || (m_transitionStart >= 0 && t - m_transitionStart < m_transitionDuration);
        for (const auto& state : m_nodeStates) {
            fast = fast || (state.tweenStart >= 0 && t - state.tweenStart < TweenDuration) ||
                   (state.appearStart >= 0 && t - state.appearStart < AppearDuration);
        }
        for (const auto& node : m_systems) {
            fast = fast || (node.moveStart >= 0 && t - node.moveStart < SystemMoveDuration);
        }
        m_ticker.setInterval(fast ? 16 : 33);
        if (!m_ticker.isActive()) {
            m_lastTick = t;
            m_ticker.start();
        }
    } else {
        m_ticker.stop();
    }
}

void GalaxyView::tick()
{
    const qreal t = now();
    const qreal dt = std::clamp(t - m_lastTick, 0.0, 0.1);  // frame-rate independent, robust to stalls
    m_lastTick = t;

    const bool foreground = inForeground();
    if (m_orbitAnimation && !m_reducedMotion && foreground) {
        m_orbitTime += dt;
        // Whatever is under the cursor stops moving, so it is easy to read and click: a hovered planet, a hovered
        // moon or ring together with its planet, or a hovered profile (its planets, and its place in the galaxy).
        const Hit hovered = m_hover;
        const int hoveredSystem = hovered.type == HitType::System && hovered.index < m_systems.size() ? hovered.index : -1;
        const int hoveredNode = hovered.type == HitType::Body && hovered.index < m_nodeStates.size() ? hovered.index : -1;
        const int hoveredParent = hoveredNode >= 0 ? m_layout.nodes[hoveredNode].parent : -1;
        // A body falls behind (stays still) while it is hovered, but never so far that it would reach a neighbour on
        // its orbit; once the cursor leaves, it moves a little faster until it is back in its place.
        QHash<std::pair<int, int>, int> onOrbit;
        for (const auto& node : m_layout.nodes) {
            onOrbit[{ node.parent, node.orbit }]++;
        }
        const QList<int> exile = exileSlots();
        const auto exiled = std::count_if(exile.begin(), exile.end(), [](int slot) { return slot >= 0; });
        for (int n = 0; n < m_nodeStates.size() && n < m_layout.nodes.size(); n++) {
            const auto& node = m_layout.nodes[n];
            auto& state = m_nodeStates[n];
            if (n == hoveredNode || n == hoveredParent) {
                const qreal spacing = 2 * M_PI / std::max(1, onOrbit.value({ node.parent, node.orbit }));
                qreal maxLag = 0.4 * spacing / std::max(1e-3, std::abs(node.speed));
                if (exile.value(n, -1) >= 0) {
                    maxLag = std::min(maxLag, 0.4 * (2 * M_PI / std::max<qsizetype>(1, exiled)) / ExileSpeed);
                }
                state.paused = std::max(state.paused, std::min(state.paused + dt, maxLag));
            } else if (state.paused > 0) {
                state.paused = std::max(0.0, state.paused - dt * BodyCatchUpRate);
            }
        }
        for (int i = 0; i < m_systems.size(); i++) {
            auto& node = m_systems[i];
            if (i == hoveredSystem) {
                node.paused += dt;  // the whole mini system stops together, so nothing can collide
            } else if (node.paused > 0) {
                node.paused = std::max(0.0, node.paused - dt * BodyCatchUpRate);
            }
        }
        m_galaxyTime += dt;
        // A hovered profile falls behind the galaxy's rotation (it stays still) and catches up after; it only moves
        // relative to the others while that keeps it clear of them, otherwise it is carried along.
        for (int i = 0; i < m_systems.size(); i++) {
            auto& node = m_systems[i];
            qreal lag = node.lag;
            if (i == hoveredSystem) {
                lag += dt;
            } else if (lag > 0) {
                lag = std::max(0.0, lag - dt * SystemCatchUpRate);
            }
            if (lag != node.lag && systemLagAllowed(i, lag)) {
                node.lag = lag;
            }
        }
    }
    if (m_backgroundMotion && !m_reducedMotion && foreground) {
        m_driftTime += dt;
    }

    if (m_transitionStart >= 0 && t - m_transitionStart >= m_transitionDuration) {
        m_transitionStart = -1;
        m_transitionFrom = m_transitionTo;
        if (m_transitionTo == 0.0) {
            // Fully back in the galaxy: stop watching the profile's folders.
            m_systemZoom = 1.0;
            m_systemPan = {};
            m_content.reset();
            m_bodies.clear();
            m_layout = {};
            m_nodeStates.clear();
            m_nodeByKey.clear();
            m_systemId.clear();
        }
        emit navigationChanged();
        if (m_transitionTo == 0.0 && !m_pendingSystem.isEmpty()) {
            const auto next = m_pendingSystem;
            m_pendingSystem.clear();
            enterSystem(next);
        }
    }

    syncNebulae();
    if (m_launchStart >= 0 && t - m_launchStart >= (m_reducedMotion ? ReducedLaunchDuration : LaunchDuration)) {
        m_launchStart = -1;
        m_launchId.clear();
    }
    updatePasser(t);
    if (m_beamStart >= 0) {
        const qreal bt = t - m_beamStart;
        if (!m_beamFired && bt >= BeamArrive) {
            m_beamFired = true;
            // Queued: launching may open dialogs, which must not run inside the animation tick.
            QMetaObject::invokeMethod(this, [this, id = m_beamId] { emit launchRequested(id); }, Qt::QueuedConnection);
        }
        if (bt >= BeamDuration) {
            m_beamStart = -1;
            m_beamId.clear();
        }
    }
    // Names fade out quickly (they must be gone before the sparkles appear) and back in more slowly.
    if (const qreal target = labelTarget(); target < m_labelFade) {
        m_labelFade = std::max(target, m_labelFade - dt / 0.25);
    } else if (target > m_labelFade) {
        m_labelFade = std::min(target, m_labelFade + dt / 1.0);
    }

    // Re-evaluate hover: bodies move under a still cursor.
    if (m_mouseInside && !m_dragging) {
        setHover(hitTest(m_mousePos));
    }
    update();
    updateTicker();
}

void GalaxyView::syncNebulae()
{
    const qreal t = now();
    QSet<QString> present;
    for (const auto& node : std::as_const(m_systems)) {
        present.insert(node.id);
        auto it = m_nebulae.find(node.id);
        const bool launching = node.id == m_launchId && m_launchStart >= 0;
        if (node.running) {
            if (it == m_nebulae.end()) {
                if (!launching) {
                    m_nebulae.insert(node.id, { t, false, -1 });
                } else if (m_reducedMotion || t - m_launchStart >= 1.0) {
                    // The pull-in is complete: the sun explodes into the nebula.
                    m_nebulae.insert(node.id, { t, !m_reducedMotion, -1 });
                    m_launchStart = -1;
                    m_launchId.clear();
                }
            } else if (it->reformStart >= 0) {
                *it = { t, false, -1 };  // launched again while reforming
            }
        } else if (it != m_nebulae.end()) {
            const qreal reformDuration = m_reducedMotion ? 0.35 : NebulaReformDuration;
            if (it->reformStart < 0) {
                it->reformStart = t;
            } else if (t - it->reformStart > reformDuration) {
                m_nebulae.erase(it);
            }
        }
    }
    for (auto it = m_nebulae.begin(); it != m_nebulae.end();) {
        it = present.contains(it.key()) ? std::next(it) : m_nebulae.erase(it);
    }
}

GalaxyView::NebulaMix GalaxyView::nebulaMix(const QString& instanceId) const
{
    NebulaMix mix;
    const auto it = m_nebulae.constFind(instanceId);
    if (it == m_nebulae.constEnd()) {
        return mix;
    }
    const qreal t = now();
    const qreal form = m_reducedMotion ? 0.35 : NebulaFormDuration;
    const qreal reform = m_reducedMotion ? 0.35 : NebulaReformDuration;
    mix.form = std::max(1e-3, clamp01((t - it->formStart) / form));
    mix.reform = it->reformStart >= 0 ? clamp01((t - it->reformStart) / reform) : 0.0;
    mix.fromSun = it->fromSun;
    return mix;
}

qreal GalaxyView::labelTarget() const
{
    if (m_systemId.isEmpty()) {
        return 1;
    }
    if (m_launchId == m_systemId && m_launchStart >= 0) {
        return 0;
    }
    // Prism reports "running" from the start of launch preparation, so this covers the whole launch.
    const int index = systemIndexOf(m_systemId);
    const auto it = m_nebulae.constFind(m_systemId);
    const bool reforming = it != m_nebulae.constEnd() && it->reformStart >= 0;
    return index >= 0 && m_systems[index].running && !reforming ? 0 : 1;
}

bool GalaxyView::nebulaAnimating() const
{
    const qreal t = now();
    for (const auto& state : m_nebulae) {
        if (t - state.formStart < NebulaFormDuration || state.reformStart >= 0) {
            return true;
        }
    }
    return false;
}

void GalaxyView::playLaunchAnimation(const QString& instanceId)
{
    if (!m_launchAnimation && !m_reducedMotion) {
        return;
    }
    m_launchId = instanceId;
    m_launchStart = now();
    updateTicker();
    update();
}

// ---------------------------------------------------------------------------------------------------------------
// Geometry

QPointF GalaxyView::projectGalaxy(const Camera& cam, qreal x, qreal y, qreal z) const
{
    return QPointF(width() / 2.0 + (x - cam.center.x()) * cam.zoom, height() / 2.0 + ((z - cam.center.y()) * TiltSin - y * TiltCos) * cam.zoom);
}

GalaxyView::Camera GalaxyView::galaxyFitCamera() const
{
    // Centred on the black hole. The galaxy turns, so frame the whole circle the systems sweep through.
    Camera cam;
    cam.center = QPointF(0, 0);
    qreal radius = GalaxyLayout::BlackHoleRadius * 3;
    for (const auto& node : m_systems) {
        radius = std::max(radius, std::hypot(node.pos.x(), node.pos.y()));
    }
    radius += GalaxyLayout::OverviewRadius + 0.6;
    const qreal zoomX = width() / (2 * radius);
    const qreal zoomZ = height() / (2 * radius * TiltSin + 1.6);  // room for labels below the front systems
    cam.zoom = std::clamp(std::min(zoomX, zoomZ) * 0.92, 12.0, 190.0);
    return cam;
}

qreal GalaxyView::systemFitZoom() const
{
    // Large systems open framed on their inner orbits; zooming out reaches the rest.
    const qreal extent = std::max(m_layout.viewExtent, GalaxyLayout::SunRadius * 3);
    const qreal zoomX = width() / (2 * extent);
    const qreal zoomY = height() / (2 * extent * TiltSin + 2.4 * GalaxyLayout::SunRadius);
    return std::max(4.0, std::min(zoomX, zoomY) * 0.9);
}

GalaxyView::Camera GalaxyView::systemApproachCamera(int systemIndex) const
{
    Camera cam;
    cam.center = systemPosition(systemIndex);
    const qreal extent = std::max(m_layout.extent, GalaxyLayout::SunRadius * 3);
    cam.zoom = extent * systemFitZoom() / GalaxyLayout::OverviewRadius;
    return cam;
}

GalaxyView::Camera GalaxyView::currentGalaxyCamera() const
{
    const qreal e = transitionProgress();
    const int index = systemIndexOf(m_systemId);
    if (e <= 0 || index < 0) {
        return m_galaxyCamera;
    }
    const Camera target = systemApproachCamera(index);
    Camera cam;
    cam.center = m_galaxyCamera.center + (target.center - m_galaxyCamera.center) * e;
    cam.zoom = std::exp(lerp(std::log(m_galaxyCamera.zoom), std::log(target.zoom), e));
    return cam;
}

QPointF GalaxyView::systemPosition(int index) const
{
    return systemPosition(index, m_systems[index].lag);
}

bool GalaxyView::systemLagAllowed(int index, qreal lag) const
{
    // Closer than this, two overview systems would touch; a move that only increases the distance is always fine.
    constexpr qreal minDistance = 2 * GalaxyLayout::OverviewRadius + 0.4;
    const QPointF from = systemPosition(index);
    const QPointF to = systemPosition(index, lag);
    for (int j = 0; j < m_systems.size(); j++) {
        if (j == index) {
            continue;
        }
        const QPointF other = systemPosition(j);
        const qreal after = QLineF(to, other).length();
        if (after < minDistance && after < QLineF(from, other).length()) {
            return false;
        }
    }
    return true;
}

QPointF GalaxyView::systemPosition(int index, qreal lag) const
{
    const auto& node = m_systems[index];
    QPointF base = node.pos;
    if (node.moveStart >= 0) {
        const qreal u = easeInOut((now() - node.moveStart) / SystemMoveDuration);
        base = node.fromPos + (node.pos - node.fromPos) * u;
    }
    // The whole galaxy slowly turns around its black hole.
    const qreal angle = GalaxyLayout::galacticSpeed(std::hypot(base.x(), base.y())) * (m_galaxyTime - lag);
    const qreal c = std::cos(angle);
    const qreal s = std::sin(angle);
    return QPointF(base.x() * c - base.y() * s, base.x() * s + base.y() * c);
}

qreal GalaxyView::nodeAngle(int n) const
{
    const auto& node = m_layout.nodes[n];
    qreal angle = node.phase + node.speed * (m_orbitTime - (n < m_nodeStates.size() ? m_nodeStates[n].paused : 0.0));
    if (n < m_nodeStates.size()) {
        const auto& state = m_nodeStates[n];
        if (state.tweenStart >= 0) {
            angle += state.angleOffset * (1 - easeOut((now() - state.tweenStart) / TweenDuration));
        }
    }
    return angle;
}

qreal GalaxyView::nodeOrbitRadius(int n) const
{
    const auto& node = m_layout.nodes[n];
    qreal radius = node.orbitRadius;
    if (n < m_nodeStates.size()) {
        const auto& state = m_nodeStates[n];
        if (state.tweenStart >= 0) {
            radius += state.radiusOffset * (1 - easeOut((now() - state.tweenStart) / TweenDuration));
        }
    }
    return radius;
}

QPointF GalaxyView::nodePosition(int n) const
{
    const auto& node = m_layout.nodes[n];
    if (node.role == GalaxyLayout::Node::Role::Ring) {
        return nodePosition(node.parent);
    }
    const qreal angle = nodeAngle(n);
    const qreal radius = nodeOrbitRadius(n);
    const QPointF offset(radius * std::cos(angle), radius * std::sin(angle));
    if (node.role == GalaxyLayout::Node::Role::Moon) {
        return nodePosition(node.parent) + offset;
    }
    return offset;
}

// ---------------------------------------------------------------------------------------------------------------
// Painting

void GalaxyView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, m_renderQuality > 0);
    m_hitShapes.clear();

    const qreal e = transitionProgress();
    syncNebulae();

    paintBackground(p, e);

    if (e < 1) {
        paintGalaxyLayer(p, 1 - e);
    }
    if (e > 0 && m_content) {
        // Far behind the system; it fades together with the system view when leaving it.
        paintPasser(p, smoothstep(0.2, 0.85, e));
        const int index = systemIndexOf(m_systemId);
        if (index >= 0) {
            const Camera cam = currentGalaxyCamera();
            const QPointF sys = systemPosition(index);
            const QPointF origin = projectGalaxy(cam, sys.x(), 0, sys.y()) + m_systemPan * e;
            const qreal extent = std::max(m_layout.extent, GalaxyLayout::SunRadius * 3);
            const qreal zoom = GalaxyLayout::OverviewRadius * cam.zoom / extent * lerp(1, m_systemZoom, e);
            paintSystemLayer(p, smoothstep(0.2, 0.85, e), origin, zoom);
        }
    }

    paintStatus(p);
    paintTooltip(p);

    if (hasFocus() && m_keyboardFocus && m_focus.type == HitType::None && m_systems.size() > 0) {
        // Keyboard users need to see that the canvas has focus.
        p.save();
        p.setPen(QPen(withAlphaF(Accent, 0.5), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(1, 1, -2, -2));
        p.restore();
    }
}

void GalaxyView::paintBackground(QPainter& p, qreal e)
{
    // Cached layers, rebuilt only when size, scale or quality change.
    const qreal dpr = devicePixelRatioF();
    if (m_starfieldSize != size() || !qFuzzyCompare(m_starfieldDpr, dpr) || m_starfieldBuiltQuality != m_starfieldQuality) {
        m_skyBase = CelestialPainter::renderSkyBase(size(), SkyMargin, dpr, m_starfieldQuality);
        for (int layer = 0; layer < 2; layer++) {
            m_starTiles[layer] = CelestialPainter::renderStarTile(StarTileSize, layer, dpr, m_starfieldQuality);
        }
        m_starfieldSize = size();
        m_starfieldDpr = dpr;
        m_starfieldBuiltQuality = m_starfieldQuality;
    }

    // Where the camera is, in screen pixels: panning, zooming and flying into a system all move it.
    const Camera cam = currentGalaxyCamera();
    QPointF view(cam.center.x() * cam.zoom, cam.center.y() * TiltSin * cam.zoom);
    view -= m_systemPan * e;
    if (m_reducedMotion) {
        view *= 0.25;  // still follows the camera, but barely
    }
    // A slow drift of its own, so the sky is never completely still.
    const QPointF drift(m_driftTime * 5.0, m_driftTime * 1.6);

    // Nebulae: the farthest layer, moving least; eased so it never runs out of margin.
    const QPointF nebula = view * 0.006 + drift * 0.15;
    auto soft = [](qreal v) { return SkyMargin * std::tanh(v / SkyMargin); };
    p.drawPixmap(QPointF(-SkyMargin - soft(nebula.x()), -SkyMargin - soft(nebula.y())), m_skyBase);
    if (m_starfieldQuality <= 0) {
        return;
    }

    auto wrap = [](qreal v) {
        v = std::fmod(v, static_cast<qreal>(StarTileSize));
        return v < 0 ? v + StarTileSize : v;
    };
    // Distant stars, then near stars which move most (parallax depth).
    const QPointF far = view * 0.02 + drift * 0.45;
    p.drawTiledPixmap(QRectF(rect()), m_starTiles[0], QPointF(wrap(far.x()), wrap(far.y())));
    if (m_starfieldQuality >= 2) {

        const QPointF near = view * 0.055 + drift;
        if (e > 0 && e < 1 && !m_reducedMotion) {
            // Near stars rush outward while travelling between the galaxy and a system.
            const qreal scale = 1 + 0.1 * std::sin(M_PI * e);
            p.save();
            p.translate(width() / 2.0, height() / 2.0);
            p.scale(scale, scale);
            p.translate(-width() / 2.0, -height() / 2.0);
            // The larger area starts one view up and left; shift the tiles by the same amount so they stay in place.
            p.drawTiledPixmap(QRectF(rect()).adjusted(-width(), -height(), width(), height()), m_starTiles[1],
                              QPointF(wrap(near.x() - width()), wrap(near.y() - height())));
            p.restore();
        } else {
            p.drawTiledPixmap(QRectF(rect()), m_starTiles[1], QPointF(wrap(near.x()), wrap(near.y())));
        }
    }

    // Now and then a faint shooting star crosses the sky.
    if (m_backgroundMotion && !m_reducedMotion && m_renderQuality >= 1) {
        const int cycle = static_cast<int>(m_driftTime / ShootingStarPeriod);
        const qreal age = m_driftTime - cycle * ShootingStarPeriod - 8.0;  // 8 s into each period
        if (cycle >= 0 && age >= 0 && age < 1.2) {
            const auto seed = static_cast<std::uint32_t>(cycle) * 2654435761U;
            const qreal sx = (seed % 1000U) / 1000.0 * width();
            const qreal sy = ((seed >> 10U) % 1000U) / 1000.0 * height() * 0.5;
            const QPointF dir(0.82, 0.57);
            const qreal u = age / 1.2;
            const QPointF head = QPointF(sx, sy) + dir * (u * 420);
            const QPointF tail = head - dir * 140;
            const qreal alpha = std::sin(M_PI * u) * 0.55;
            QLinearGradient trail(tail, head);
            trail.setColorAt(0, QColor(220, 210, 255, 0));
            trail.setColorAt(1, withAlphaF(QColor(240, 235, 255), alpha));
            p.save();
            p.setPen(QPen(QBrush(trail), 1.6, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(tail, head);
            p.restore();
        }
    }
}

void GalaxyView::paintLabel(QPainter& p, const QPointF& anchor, const QString& text, qreal opacity, bool emphasized)
{
    if (text.isEmpty() || opacity <= 0.01) {
        return;
    }
    QFont font = p.font();
    font.setBold(emphasized);
    if (!emphasized) {
        font.setPointSizeF(font.pointSizeF() * 0.9);
    }
    const QFontMetricsF fm(font);
    const QString elided = fm.elidedText(text, Qt::ElideRight, emphasized ? 240 : 160);
    const QSizeF textSize(fm.horizontalAdvance(elided), fm.height());
    QRectF box(anchor.x() - textSize.width() / 2 - 7, anchor.y(), textSize.width() + 14, textSize.height() + 4);
    p.save();
    p.setOpacity(p.opacity() * opacity);
    p.setFont(font);
    p.setPen(emphasized ? QPen(withAlphaF(Accent, 0.55), 1) : Qt::NoPen);
    p.setBrush(QColor(12, 9, 30, emphasized ? 215 : 165));
    p.drawRoundedRect(box, box.height() / 2, box.height() / 2);
    p.setPen(emphasized ? TextColor : MutedText);
    p.drawText(box, Qt::AlignCenter, elided);
    p.restore();
}

QString GalaxyView::blackHoleName() const
{
    const auto name = APPLICATION->settings()->get(GalaxySettings::BlackHoleName).toString().trimmed();
    return name.isEmpty() ? tr("Galaxy Core") : name;
}

void GalaxyView::updatePasser(qreal t)
{
    const bool inSystem = m_targetMode == Mode::System && m_transitionStart < 0 && !m_systemId.isEmpty();
    const bool allowed = m_backgroundMotion && !m_reducedMotion && inForeground();
    if (!inSystem || !allowed) {
        if (m_targetMode != Mode::System && m_transitionStart < 0) {
            m_passer.reset();  // back in the galaxy; it faded out together with the system view
        }
        m_nextPass = -1;
        return;
    }
    // GALAXY_DEV_PASSER=1 (developer tooling): one passes right away, quickly; =2 always another profile.
    static const int devPasserMode = qEnvironmentVariableIntValue("GALAXY_DEV_PASSER");
    static const bool devPasser = devPasserMode > 0;
    if (m_passer && t - m_passer->start > m_passer->duration) {
        m_passer.reset();
    }
    if (m_nextPass < 0) {
        m_nextPass = t + (devPasser ? 0.5 : 45 + rand01(stableHash(m_systemId) ^ static_cast<std::uint32_t>(t)) * 105);
    }
    if (m_passer || t < m_nextPass) {
        return;
    }
    const auto seed = static_cast<std::uint32_t>(t * 1000) ^ stableHash(m_systemId);
    QStringList others;
    for (const auto& node : std::as_const(m_systems)) {
        if (node.id != m_systemId) {
            others << node.id;
        }
    }
    Passer passer;
    if (!others.isEmpty() && (devPasserMode == 2 || rand01(seed) > 0.35)) {
        passer.systemId = others.at(static_cast<int>(rand01(seed ^ 0x9e3779b9U) * others.size()) % others.size());
    }
    passer.start = t;
    passer.duration = devPasser ? 6 : 22 + rand01(seed ^ 0x85ebca6bU) * 13;
    const bool leftToRight = rand01(seed ^ 0xc2b2ae35U) > 0.5;
    const qreal y0 = 0.15 + rand01(seed ^ 0x27d4eb2fU) * 0.7;
    const qreal y1 = std::clamp(y0 + (rand01(seed ^ 0x165667b1U) - 0.5) * 0.6, 0.1, 0.9);
    passer.from = QPointF(leftToRight ? -0.12 : 1.12, y0);
    passer.to = QPointF(leftToRight ? 1.12 : -0.12, y1);
    passer.size = 22 + rand01(seed ^ 0xd3a2646cU) * 20;
    m_passer = passer;
    m_nextPass = t + passer.duration + (devPasser ? 1.0 : 45 + rand01(seed ^ 0xfd7046c5U) * 105);
}

void GalaxyView::paintPasser(QPainter& p, qreal alpha)
{
    if (!m_passer || alpha <= 0.01) {
        return;
    }
    const auto& passer = *m_passer;
    const qreal u = clamp01((now() - passer.start) / passer.duration);
    const QPointF at((passer.from.x() + (passer.to.x() - passer.from.x()) * u) * width() - m_systemPan.x() * 0.1,
                     (passer.from.y() + (passer.to.y() - passer.from.y()) * u) * height() - m_systemPan.y() * 0.1);
    // Distant and dim; it eases in and out at the ends of its path.
    const qreal opacity = alpha * 0.6 * smoothstep(0, 0.08, u) * (1 - smoothstep(0.92, 1, u));
    const int q = std::min(quality(), 1);
    if (passer.systemId.isEmpty()) {
        // At the same scale as a passing profile, like in the galaxy.
        CelestialPainter::drawBlackHole(p, at, GalaxyLayout::BlackHoleRadius * passer.size * 1.35, -m_driftTime * 6.0, q, opacity);
        return;
    }
    const int index = systemIndexOf(passer.systemId);
    if (index < 0) {
        return;
    }
    // Drawn the way the galaxy shows it, only small and far away.
    p.save();
    p.setOpacity(p.opacity() * opacity);
    paintMiniSystem(p, index, at, passer.size * 1.35, -1, false);
    p.restore();
}

QString GalaxyView::blackHoleTarget() const
{
    const auto chosen = APPLICATION->settings()->get(GalaxySettings::BlackHoleProfile).toString();
    if (!chosen.isEmpty() && systemIndexOf(chosen) >= 0) {
        return chosen;
    }
    QString latest;
    qint64 latestTime = 0;
    for (const auto& node : std::as_const(m_systems)) {
        if (node.instance && node.instance->lastLaunch() > latestTime) {
            latest = node.id;
            latestTime = node.instance->lastLaunch();
        }
    }
    if (!latest.isEmpty()) {
        return latest;
    }
    if (systemIndexOf(m_selectedId) >= 0) {
        return m_selectedId;
    }
    return m_systems.isEmpty() ? QString() : m_systems.first().id;
}

void GalaxyView::launchFromBlackHole()
{
    const auto id = blackHoleTarget();
    if (id.isEmpty() || m_beamStart >= 0) {
        return;
    }
    if (m_reducedMotion || !m_launchAnimation) {
        emit launchRequested(id);
        return;
    }
    m_beamId = id;
    m_beamStart = now();
    m_beamFired = false;
    updateTicker();
    update();
}

void GalaxyView::paintBeam(QPainter& p, const Camera& cam, qreal fade)
{
    const int index = systemIndexOf(m_beamId);
    if (m_beamStart < 0 || index < 0) {
        return;
    }
    const qreal bt = now() - m_beamStart;
    const qreal reach = easeOut(clamp01(bt / BeamArrive));
    const qreal alpha = fade * (bt < BeamArrive ? 1.0 : 1 - clamp01((bt - BeamArrive) / (BeamDuration - BeamArrive)));
    if (alpha <= 0.01) {
        return;
    }
    const QPointF from = projectGalaxy(cam, 0, 0, 0);
    const QPointF target = systemPosition(index);
    const QPointF to = projectGalaxy(cam, target.x(), 0, target.y());
    const QPointF head = from + (to - from) * reach;
    const QColor glow(176, 140, 255);

    p.save();
    p.setOpacity(p.opacity() * alpha);
    p.setBrush(Qt::NoBrush);
    // Soft glow, then a bright core, then a spark at the head.
    p.setPen(QPen(withAlphaF(glow, 0.22), 12, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(from, head);
    p.setPen(QPen(withAlphaF(glow, 0.55), 5, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(from, head);
    p.setPen(QPen(QColor(255, 250, 255, 230), 1.8, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(from, head);
    QRadialGradient spark(head, 16);
    spark.setColorAt(0, QColor(255, 255, 255, 240));
    spark.setColorAt(0.4, withAlphaF(glow, 0.6));
    spark.setColorAt(1, withAlphaF(glow, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(spark);
    p.drawEllipse(head, 16, 16);
    if (bt >= BeamArrive) {
        // The system lights up where the beam lands.
        const qreal ring = clamp01((bt - BeamArrive) / (BeamDuration - BeamArrive));
        const qreal radius = (GalaxyLayout::OverviewRadius * 0.6 + ring * GalaxyLayout::OverviewRadius) * cam.zoom;
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(withAlphaF(glow, 0.8 * (1 - ring)), 2.5));
        p.drawEllipse(to, radius, radius * TiltSin);
    }
    p.restore();
}

void GalaxyView::renameBlackHole()
{
    bool ok = false;
    const auto name = QInputDialog::getText(this, tr("Rename black hole"), tr("Name of the black hole at the centre of your galaxy:"),
                                            QLineEdit::Normal, blackHoleName(), &ok)
                          .trimmed();
    if (ok) {
        APPLICATION->settings()->set(GalaxySettings::BlackHoleName, name);  // empty restores the default
        update();
    }
}

void GalaxyView::paintBlackHole(QPainter& p, const Camera& cam, qreal fade)
{
    const QPointF c = projectGalaxy(cam, 0, 0, 0);
    const qreal r = std::max(6.0, GalaxyLayout::BlackHoleRadius * cam.zoom);
    const qreal alpha = std::pow(fade, 1.5);
    if (alpha <= 0.01) {
        return;
    }
    const bool hovered = m_hover.type == HitType::BlackHole;
    CelestialPainter::drawBlackHole(p, c, r * (hovered ? 1.05 : 1.0), -m_galaxyTime * 25.0 - m_driftTime * 4.0, quality(), alpha);
    HitShape shape;
    shape.hit = { HitType::BlackHole, -1 };
    shape.center = c;
    shape.radius = std::max(r * 1.4, 14.0);
    m_hitShapes.append(shape);
    paintLabel(p, c + QPointF(0, r * 1.9 + 4), blackHoleName(), alpha, hovered);
}

void GalaxyView::paintGalaxyLayer(QPainter& p, qreal fade)
{
    if (m_systems.isEmpty()) {
        paintBlackHole(p, currentGalaxyCamera(), fade);
        return;
    }
    const Camera cam = currentGalaxyCamera();
    const qreal t = now();
    const int q = quality();
    const int target = m_targetMode == Mode::System || m_transitionStart >= 0 ? systemIndexOf(m_systemId) : -1;

    // Faint galactic core, so the systems read as one galaxy rather than loose dots.
    if (q >= 1) {
        qreal maxR = 0;
        for (const auto& node : m_systems) {
            maxR = std::max(maxR, std::hypot(node.pos.x(), node.pos.y()));
        }
        const QPointF core = projectGalaxy(cam, 0, 0, 0);
        const qreal rx = (maxR + GalaxyLayout::OverviewRadius * 2) * cam.zoom;
        p.save();
        p.setOpacity(fade);
        p.translate(core);
        p.scale(1.0, TiltSin);
        QRadialGradient glow(QPointF(0, 0), rx);
        glow.setColorAt(0, QColor(120, 80, 220, 46));
        glow.setColorAt(0.5, QColor(90, 60, 190, 18));
        glow.setColorAt(1, QColor(60, 40, 160, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawEllipse(QPointF(0, 0), rx, rx);
        p.restore();
    }

    // Far systems first.
    QList<int> order(m_systems.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [this](int a, int b) { return systemPosition(a).y() < systemPosition(b).y(); });

    struct PendingLabel {
        QPointF anchor;
        QString text;
        qreal opacity;
        bool emphasized;
        bool running;
    };
    QList<PendingLabel> labels;

    bool blackHoleDrawn = false;
    for (int i : order) {
        const auto& node = m_systems[i];
        const QPointF pos = systemPosition(i);
        if (!blackHoleDrawn && pos.y() >= 0) {
            paintBlackHole(p, cam, fade);
            blackHoleDrawn = true;
        }
        const QPointF c = projectGalaxy(cam, pos.x(), 0, pos.y());
        const qreal zoom = cam.zoom;

        qreal alpha = i == target ? 1 - smoothstep(0.15, 0.75, 1 - fade) : std::pow(fade, 1.5);
        const bool match = systemMatchesFilter(node);
        if (!match) {
            alpha *= 0.25;
        }
        if (alpha <= 0.01) {
            continue;
        }

        // Cull systems that are entirely off screen.
        const qreal reach = (GalaxyLayout::OverviewRadius + 0.4) * zoom;
        if (c.x() < -reach || c.x() > width() + reach || c.y() < -reach || c.y() > height() + reach) {
            continue;
        }

        const bool hovered = m_hover == Hit{ HitType::System, i };
        const bool focused = m_focus == Hit{ HitType::System, i };
        const bool selected = node.id == m_selectedId;
        const qreal launchT = node.id == m_launchId && m_launchStart >= 0 ? t - m_launchStart : -1;

        p.save();
        p.setOpacity(alpha);

        // Selection and hover: an outlined disc on the plane, not just a color change.
        if (selected || hovered || focused) {
            const qreal rx = GalaxyLayout::OverviewRadius * 1.08 * zoom;
            p.setBrush(QColor(140, 100, 255, hovered ? 34 : 22));
            QPen pen(withAlphaF(Accent, selected ? 0.85 : 0.5), selected ? 1.6 : 1.0, selected ? Qt::SolidLine : Qt::DashLine);
            p.setPen(pen);
            p.drawEllipse(c, rx, rx * TiltSin);
        }

        paintMiniSystem(p, i, c, zoom, launchT, hovered);
        p.restore();

        // Hit area: the whole mini system.
        HitShape shape;
        shape.hit = { HitType::System, i };
        shape.center = c;
        shape.rx = GalaxyLayout::OverviewRadius * zoom;
        shape.ry = std::max(shape.rx * TiltSin, 16.0);
        shape.rx = std::max(shape.rx, 16.0);
        m_hitShapes.append(shape);

        labels.append({ c + QPointF(0, (GalaxyLayout::OverviewRadius * TiltSin + 0.15) * zoom), node.name, alpha,
                        selected || hovered || focused, node.running });
    }

    if (!blackHoleDrawn) {
        paintBlackHole(p, cam, fade);
    }

    paintBeam(p, cam, fade);

    // Labels last, front to back, skipping ones that would overlap a nearer label.
    QList<QRectF> placed;
    for (auto it = labels.rbegin(); it != labels.rend(); ++it) {
        const QFontMetricsF fm(font());
        const QRectF box(it->anchor.x() - 90, it->anchor.y(), 180, fm.height() + 4);
        const bool overlaps = std::any_of(placed.begin(), placed.end(), [&box](const QRectF& r) { return r.intersects(box); });
        if (overlaps && !it->emphasized) {
            continue;
        }
        placed.append(box);
        paintLabel(p, it->anchor, it->text, it->opacity, it->emphasized);
        if (it->running) {
            paintLabel(p, it->anchor + QPointF(0, fm.height() + 6), tr("▶ Running"), it->opacity, false);
        }
    }
}

QList<int> GalaxyView::exileSlots() const
{
    // Disabled content, in node order; a ring on a disabled planet goes with its planet.
    QList<int> places(m_layout.nodes.size(), -1);
    int next = 0;
    for (int n = 0; n < m_layout.nodes.size(); n++) {
        const auto& node = m_layout.nodes[n];
        const bool parentExiled = node.parent >= 0 && node.parent < n && places[node.parent] >= 0;
        if (node.body < m_bodies.size() &&
            (!m_bodies[node.body].enabled || (node.role == GalaxyLayout::Node::Role::Ring && parentExiled))) {
            places[n] = next++;
        }
    }
    return places;
}

void GalaxyView::paintMiniSystem(QPainter& p, int index, QPointF c, qreal zoom, qreal launchT, bool hovered)
{
    // A profile as the galaxy shows it: its sun with the largest mods on a few orbits (also used for the profiles
    // that drift by behind an entered system).
    const auto& node = m_systems[index];
    const qreal t = now();
    const int q = quality();
    const auto [sunIntensity, sunScale] = launchSun(launchT, m_reducedMotion);

    // Overview orbits and planets: the largest installed mods, as a summary.
    const auto& summary = node.summary;
    const int planets = static_cast<int>(summary.planetSizes.size());
    // Same rule as inside a system: at most three planets per orbit.
    constexpr int overviewOrbits = 4;
    const qreal orbitRadii[overviewOrbits] = { 0.52, 0.72, 0.92, 1.12 };
    constexpr int perOrbit = GalaxyLayout::MaxPlanetsPerOrbit;
    if (m_showOrbits && q >= 1) {
        for (int k = 0; k < overviewOrbits && k * perOrbit < planets + 1; k++) {
            CelestialPainter::drawOrbit(p, c, orbitRadii[k] * zoom, orbitRadii[k] * zoom * TiltSin, OrbitColor, 0.13);
        }
    }

    struct Mini {
        qreal depth;
        QPointF at;
        qreal r;
        CelestialPainter::SurfaceStyle style;
        QPointF light;
        qreal facing;
        int ring;  // 1 = draw a shader ring around it
        QString key;
        qreal sparkle = 0;
    };
    QList<Mini> minis;
    const auto nebula = nebulaMix(node.id);
    const qreal nebulaAmount = nebula.amount();
    if (nebula.active()) {
        const qreal grow = nebula.reform > 0 ? 1 - 0.85 * easeInOut(nebula.reform) : 0.25 + 0.75 * easeOut(nebula.form);
        CelestialPainter::drawNebula(p, c, GalaxyLayout::OverviewRadius * 0.95 * zoom * grow, node.color, m_driftTime + m_orbitTime,
                                     stableHash(node.id), q, nebulaAmount);
    }
    int slot = 0;
    // Slot 0: the base game planet, hosting packs and shaders.
    QList<std::pair<qint64, QString>> items;
    items.append({ -2, QString() });
    for (int k = 0; k < planets; k++) {
        items.append({ summary.planetSizes[k], summary.planetKeys[k] });
    }
    for (const auto& [bytes, key] : items) {
        const int orbit = std::min(slot / perOrbit, overviewOrbits - 1);
        const int index = slot - orbit * perOrbit;
        const int onOrbit = std::min(perOrbit, static_cast<int>(items.size()) - orbit * perOrbit);
        const qreal speed = 0.11 * std::pow(orbitRadii[0] / orbitRadii[orbit], 1.5);
        const qreal angle = orbit * 2.4 + 2 * M_PI * index / std::max(1, onOrbit) + speed * (m_orbitTime - node.paused);
        qreal radius = orbitRadii[orbit];
        const auto pull = launchPull(launchT >= 0 && !m_reducedMotion ? launchT : -1, radius / orbitRadii[overviewOrbits - 1]);
        radius *= 1 - pull.pull;
        qreal x = radius * std::cos(angle);
        qreal z = radius * std::sin(angle);
        Mini mini;
        mini.key = node.id + '/' + key;
        if (nebula.active()) {
            const QPointF orbitPos(x, z);
            const QPointF spot = sparklePosition(mini.key, GalaxyLayout::OverviewRadius * 0.85);
            const QPointF at = nebula.reform > 0 ? spot + (orbitPos - spot) * easeInOut(nebula.reform)
                                                 : (nebula.fromSun ? QPointF() : orbitPos) +
                                                       (spot - (nebula.fromSun ? QPointF() : orbitPos)) * easeOut(nebula.form);
            x = at.x();
            z = at.y();
            mini.sparkle = nebulaAmount;
        }
        mini.depth = z;
        mini.at = c + QPointF(x * zoom, z * TiltSin * zoom);
        const bool base = bytes == -2;
        const qreal size = base ? 0.11 : 0.04 + 0.07 * GalaxyLayout::sizeFraction(bytes, GalaxyLayout::PlanetMinBytes,
                                                                                  GalaxyLayout::PlanetMaxBytes);
        mini.r = std::max(1.4, size * m_objectScale * zoom * pull.scale);
        mini.style = base ? CelestialPainter::baseGameStyle() : CelestialPainter::styleForKey(styleKey(key), false);
        const qreal len = std::max(1e-6, std::hypot(x, z));
        mini.light = QPointF(-x / len, (-z / len) * TiltSin - 0.35 * TiltCos);
        mini.facing = (-z / len) * TiltCos + 0.35 * TiltSin;
        mini.ring = base && summary.shaders > 0 ? 1 : 0;
        if (pull.alpha > 0.01) {
            minis.append(mini);
        }
        slot++;
    }

    auto drawMini = [&p, q](const Mini& mini, bool front) {
        if (mini.ring) {
            CelestialPainter::drawRingHalf(p, mini.at, mini.r * 1.9, mini.r * 1.9 * RingRatio, std::max(1.0, mini.r * 0.35), RingRotation,
                                           QColor(220, 205, 255), front, q);
        }
    };
    std::sort(minis.begin(), minis.end(), [](const Mini& a, const Mini& b) { return a.depth < b.depth; });
    bool sunDrawn = false;
    auto drawSun = [&]() {
        const qreal sunR = std::max(4.0, GalaxyLayout::OverviewSunRadius * zoom * sunScale * (hovered ? 1.08 : 1.0));
        const qreal explode = nebula.active() ? (nebula.reform > 0 ? 0.3 + 0.7 * nebula.reform : 1 + 1.6 * nebula.form) : 1.0;
        CelestialPainter::drawSun(p, c, sunR * explode, node.color, sunIntensity * (hovered ? 1.15 : 1.0), q, 1 - nebulaAmount);
        if (node.running && nebulaAmount < 0.5) {
            // Running: a pulsing outline plus a text badge (never color alone).
            const qreal pulse = m_reducedMotion ? 0.5 : 0.5 + 0.5 * std::sin(t * 3.2);
            p.setPen(QPen(withAlphaF(QColor(134, 239, 172), 0.45 + 0.4 * pulse), 1.5));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, sunR * (1.45 + 0.15 * pulse), sunR * (1.45 + 0.15 * pulse));
        }
        sunDrawn = true;
    };
    for (const auto& mini : minis) {
        if (!sunDrawn && mini.depth >= 0) {
            drawSun();
        }
        if (mini.sparkle > 0.01) {
            CelestialPainter::drawSparkle(p, mini.at, 1.4, mini.style.base, twinkle(mini.key, t), mini.sparkle);
        }
        if (mini.sparkle < 0.99) {
            p.save();
            p.setOpacity(p.opacity() * (1 - mini.sparkle));
            drawMini(mini, false);
            CelestialPainter::drawSphere(p, mini.at, mini.r * (1 - 0.6 * mini.sparkle), mini.style, mini.light, mini.facing, std::min(q, 1));
            drawMini(mini, true);
            p.restore();
        }
    }
    if (!sunDrawn) {
        drawSun();
    }
}

void GalaxyView::paintSystemLayer(QPainter& p, qreal alpha, QPointF origin, qreal zoom)
{
    if (alpha <= 0.01 || m_bodies.isEmpty()) {
        return;
    }
    const int index = systemIndexOf(m_systemId);
    if (index < 0) {
        return;
    }
    const auto& system = m_systems[index];
    const qreal t = now();
    const int q = quality();
    const qreal launchT = m_launchId == m_systemId && m_launchStart >= 0 ? t - m_launchStart : -1;
    const bool launchMotion = launchT >= 0 && !m_reducedMotion;
    const qreal extent = std::max(m_layout.extent, GalaxyLayout::SunRadius * 3);
    const bool transitioning = m_transitionStart >= 0;

    auto project = [origin, zoom](qreal x, qreal y, qreal z) { return origin + QPointF(x * zoom, (z * TiltSin - y * TiltCos) * zoom); };
    const auto nebula = nebulaMix(system.id);
    const qreal nebulaAmount = nebula.amount();
    const qreal nebulaRadius = std::clamp(m_layout.viewExtent * 0.7, 2.4, 10.0);

    p.save();
    p.setOpacity(alpha);

    // The nebula a playing profile's sun turned into: behind everything.
    if (nebula.active()) {
        const qreal grow = nebula.reform > 0 ? 1 - 0.85 * easeInOut(nebula.reform) : 0.25 + 0.75 * easeOut(nebula.form);
        CelestialPainter::drawNebula(p, origin, nebulaRadius * zoom * grow, system.color, m_driftTime + m_orbitTime, stableHash(system.id), q,
                                     nebulaAmount);
    }

    // Orbit paths: subtle, behind everything, never hit-testable.
    if (m_showOrbits && q >= 1 && !launchMotion && nebulaAmount < 0.01) {
        for (qreal r : m_layout.orbitRadii) {
            CelestialPainter::drawOrbit(p, origin, r * zoom, r * zoom * TiltSin, OrbitColor, 0.16);
        }
    }

    // Per node: position, size and launch state.
    struct Placed {
        QPointF local;  // (x, z)
        QPointF screen;
        qreal r = 0;  // screen radius (sphere) or band width (ring)
        qreal alpha = 1;
        qreal scale = 1;
        LaunchPull pull;
        qreal sparkle = 0;  ///< 0 = a planet/moon/ring, 1 = a sparkle in the nebula
        bool match = true;
        bool visible = true;
    };
    QList<Placed> placed(m_layout.nodes.size());

    // Disabled content does not go into the sun or the nebula: it moves out to one shared orbit beyond the others
    // while the game launches and runs, and comes back when it stops. Rings of a disabled planet go with it.
    const QList<int> exileSlot = exileSlots();
    const auto exiledCount = std::count_if(exileSlot.begin(), exileSlot.end(), [](int slot) { return slot >= 0; });
    qreal exileWeight = 0;
    if (launchMotion) {
        exileWeight = easeInOut(clamp01(launchT / 0.7)) * (1 - easeInOut(clamp01((launchT - (LaunchDuration - 0.4)) / 0.4)));
    }
    if (nebula.active()) {
        exileWeight = std::max(exileWeight, nebula.reform > 0 ? 1 - easeInOut(nebula.reform) : nebula.fromSun ? 1.0 : easeOut(nebula.form));
    }
    const qreal exileRadius = extent * 1.3;
    auto exilePosition = [&](int n) {
        const auto& node = m_layout.nodes[n];
        if (node.role == GalaxyLayout::Node::Role::Ring && node.parent >= 0 && exileSlot[node.parent] >= 0) {
            n = node.parent;  // a ring stays around its planet
        }
        const qreal paused = n < m_nodeStates.size() ? m_nodeStates[n].paused : 0.0;  // stops when hovered, like the rest
        const qreal angle = 2 * M_PI * exileSlot[n] / std::max<qsizetype>(1, exiledCount) + ExileSpeed * (m_orbitTime - paused);
        return QPointF(exileRadius * std::cos(angle), exileRadius * std::sin(angle));
    };

    for (int n = 0; n < m_layout.nodes.size(); n++) {
        const auto& node = m_layout.nodes[n];
        const auto& body = m_bodies[node.body];
        auto& pl = placed[n];
        pl.local = nodePosition(n);
        if (exileSlot[n] >= 0) {
            pl.local += (exilePosition(n) - pl.local) * exileWeight;
        } else if (launchMotion) {
            const qreal frac = std::hypot(pl.local.x(), pl.local.y()) / extent;
            pl.pull = launchPull(launchT, frac);
            pl.local *= 1 - pl.pull.pull;
            pl.scale = pl.pull.scale;
            pl.alpha = pl.pull.alpha;
        }
        if (nebula.active() && exileSlot[n] < 0) {
            // Fly out to a fixed spot in the nebula (from the sun after a launch), or back to the orbit when reforming.
            const QPointF orbit = nodePosition(n);
            const QPointF spot = sparklePosition(body.key, nebulaRadius);
            if (nebula.reform > 0) {
                pl.local = spot + (orbit - spot) * easeInOut(nebula.reform);
            } else {
                const QPointF start = nebula.fromSun ? QPointF() : orbit;
                pl.local = start + (spot - start) * easeOut(nebula.form);
            }
            pl.sparkle = nebulaAmount;
            pl.scale = 1 - 0.7 * pl.sparkle;
            pl.alpha = 1;
        }
        if (n < m_nodeStates.size() && m_nodeStates[n].appearStart >= 0) {
            pl.alpha *= easeOut((t - m_nodeStates[n].appearStart) / AppearDuration);
        }
        pl.match = matchesFilter(body);
        if (!pl.match) {
            pl.alpha *= 0.22;
        }
        pl.screen = project(pl.local.x(), 0, pl.local.y());
        pl.r = node.radius * zoom * pl.scale;
        if (node.role == GalaxyLayout::Node::Role::Ring) {
            pl.r = std::max(1.0, node.radius * zoom * pl.scale);
        }
    }

    // The shared orbit of disabled content, while the system is a nebula.
    if (m_showOrbits && q >= 1 && exiledCount > 0 && nebula.active() && exileWeight > 0.01) {
        CelestialPainter::drawOrbit(p, origin, exileRadius * zoom, exileRadius * zoom * TiltSin, OrbitColor, 0.16 * exileWeight);
    }

    // Moon orbits around their host.
    if (m_showOrbits && q >= 1 && !launchMotion && nebulaAmount < 0.01) {
        std::set<std::pair<int, qreal>> drawn;
        for (int n = 0; n < m_layout.nodes.size(); n++) {
            const auto& node = m_layout.nodes[n];
            if (node.role != GalaxyLayout::Node::Role::Moon || node.orbitRadius * zoom < 10) {
                continue;
            }
            const std::pair<int, qreal> key{ node.parent, node.orbitRadius };
            if (drawn.count(key) > 0) {
                continue;
            }
            drawn.insert(key);
            CelestialPainter::drawOrbit(p, placed[node.parent].screen, node.orbitRadius * zoom, node.orbitRadius * zoom * TiltSin, OrbitColor,
                                        0.1);
        }
    }

    // Contact shadows on the table below the orbit plane.
    if (q >= 2 && !transitioning) {
        for (int n = 0; n < m_layout.nodes.size(); n++) {
            const auto& node = m_layout.nodes[n];
            if (node.role == GalaxyLayout::Node::Role::Ring) {
                continue;
            }
            const auto& pl = placed[n];
            const QPointF at = project(pl.local.x(), -ShadowDrop, pl.local.y());
            CelestialPainter::drawShadow(p, at, pl.r * 1.1, pl.r * 1.1 * TiltSin * 0.55, 0.45 * pl.alpha * (1 - pl.sparkle));
        }
    }

    // Depth sorted draw list. Rings split into a far half (behind the planet) and a near half (in front).
    enum class Item { Sun, Sphere, RingBack, RingFront };
    struct DrawItem {
        qreal depth;
        Item item;
        int node;
    };
    QList<DrawItem> items;
    items.reserve(m_layout.nodes.size() + 8);
    items.append({ 0.0, Item::Sun, -1 });
    for (int n = 0; n < m_layout.nodes.size(); n++) {
        const auto& node = m_layout.nodes[n];
        const qreal depth = placed[n].local.y() * TiltCos;
        if (node.role == GalaxyLayout::Node::Role::Ring) {
            items.append({ depth - 1e-4, Item::RingBack, n });
            items.append({ depth + 1e-4, Item::RingFront, n });
        } else {
            items.append({ depth, Item::Sphere, n });
        }
    }
    std::stable_sort(items.begin(), items.end(), [](const DrawItem& a, const DrawItem& b) { return a.depth < b.depth; });

    auto [sunIntensity, sunScale] = launchSun(launchT, m_reducedMotion);
    if (nebula.active()) {
        // Exploding: the sun swells and fades into the clouds; reforming: it condenses back.
        sunScale *= nebula.reform > 0 ? 0.3 + 0.7 * easeOut(nebula.reform) : 1 + 1.6 * easeOut(nebula.form);
        sunIntensity += nebula.reform > 0 ? 0.0 : 1.2 * (1 - nebula.form);
    }
    const bool sunHovered = m_hover.type == HitType::Sun || m_focus.type == HitType::Sun;
    const qreal sunR = GalaxyLayout::SunRadius * zoom * sunScale * (sunHovered ? 1.04 : 1.0);

    for (const auto& item : items) {
        if (item.item == Item::Sun) {
            CelestialPainter::drawSun(p, origin, sunR, system.color, sunIntensity * (sunHovered ? 1.12 : 1.0), q, 1 - nebulaAmount);
            if (system.running && nebulaAmount < 0.5) {  // the nebula itself shows that the game is running
                const qreal pulse = m_reducedMotion ? 0.5 : 0.5 + 0.5 * std::sin(t * 3.2);
                p.setPen(QPen(withAlphaF(QColor(134, 239, 172), 0.45 + 0.4 * pulse), 2));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(origin, sunR * (1.3 + 0.08 * pulse), sunR * (1.3 + 0.08 * pulse));
            }
            HitShape shape;
            shape.hit = { HitType::Sun, -1 };
            shape.center = origin;
            shape.radius = std::max(sunR, 16.0);
            m_hitShapes.append(shape);
            continue;
        }

        const int n = item.node;
        const auto& node = m_layout.nodes[n];
        const auto& body = m_bodies[node.body];
        const auto& pl = placed[n];
        if (pl.alpha <= 0.01) {
            continue;
        }
        const Hit hit{ HitType::Body, n };
        const bool emphasized = m_hover == hit || m_focus == hit;

        if (item.item == Item::RingBack || item.item == Item::RingFront) {
            const auto& host = placed[node.parent];
            const qreal rx = node.orbitRadius * zoom * host.scale;
            const QColor color = body.enabled ? QColor(214, 200, 255) : QColor(150, 145, 165);
            const qreal ringAlpha = pl.alpha * (emphasized ? 1.0 : 0.85) * (1 - pl.sparkle);
            CelestialPainter::drawRingHalf(p, host.screen, rx, rx * RingRatio, pl.r, RingRotation, emphasized ? color.lighter(115) : color,
                                           item.item == Item::RingFront, q, ringAlpha);
            if (pl.sparkle > 0.01 && item.item == Item::RingFront) {
                CelestialPainter::drawSparkle(p, pl.screen, emphasized ? 2.6 : 2.0, color, twinkle(body.key, t), pl.sparkle);
            }
            if (pl.sparkle > 0.5) {
                if (item.item == Item::RingFront) {
                    HitShape spark;
                    spark.hit = hit;
                    spark.center = pl.screen;
                    spark.radius = 7;
                    spark.priority = true;
                    m_hitShapes.append(spark);
                }
                continue;
            }
            HitShape shape;
            shape.hit = hit;
            shape.center = host.screen;
            shape.rx = rx;
            shape.ry = rx * RingRatio;
            shape.radius = std::max(pl.r / 2, 4.0);
            shape.rotation = RingRotation;
            shape.frontHalfOnly = item.item == Item::RingFront;
            shape.backHalfOnly = item.item == Item::RingBack;
            m_hitShapes.append(shape);
            continue;
        }

        // Trail while being pulled into the sun.
        if (launchMotion && pl.pull.pull > 0.02) {
            const QPointF from = nodePosition(n) * (1 - pl.pull.trailFrom);
            const QPointF a = project(from.x(), 0, from.y());
            QLinearGradient trail(a, pl.screen);
            const QColor tc = body.kind == CelestialBody::Kind::BaseGame ? QColor(140, 190, 255) : system.color;
            trail.setColorAt(0, withAlphaF(tc, 0));
            trail.setColorAt(1, withAlphaF(tc, 0.55 * pl.alpha));
            p.setPen(QPen(QBrush(trail), std::max(1.0, pl.r * 0.9), Qt::SolidLine, Qt::RoundCap));
            p.drawLine(a, pl.screen);
        }

        const QPointF dir(-pl.local.x(), -pl.local.y());
        const qreal len = std::max(1e-6, std::hypot(dir.x(), dir.y()));
        const QPointF light(dir.x() / len, (dir.y() / len) * TiltSin - 0.35 * TiltCos);
        const qreal facing = (dir.y() / len) * TiltCos + 0.35 * TiltSin;

        CelestialPainter::SurfaceStyle style;
        if (body.kind == CelestialBody::Kind::BaseGame) {
            style = CelestialPainter::baseGameStyle();
        } else {
            const auto key = styleKey(body.fileName);
            auto it = m_styleCache.constFind(key);
            if (it == m_styleCache.constEnd()) {
                it = m_styleCache.insert(key, CelestialPainter::styleForKey(key, body.isMoon()));
            }
            style = *it;
        }
        if (!body.enabled) {
            style = disabledStyle(style);
        }
        const qreal r = std::max(node.role == GalaxyLayout::Node::Role::Moon ? 1.6 : 2.2, pl.r);
        if (pl.sparkle > 0.01) {
            CelestialPainter::drawSparkle(p, pl.screen, (emphasized ? 2.8 : 2.0) + std::min(2.0, node.radius * 2), style.base, twinkle(body.key, t),
                                          pl.alpha * pl.sparkle);
            if (pl.sparkle > 0.5) {
                HitShape spark;
                spark.hit = hit;
                spark.center = pl.screen;
                spark.radius = 7;
                spark.priority = true;
                m_hitShapes.append(spark);
                continue;
            }
        }
        const qreal sphereAlpha = pl.alpha * (1 - pl.sparkle);
        if (transitioning || launchMotion || pl.sparkle > 0.01) {
            // Sizes change every frame: caching would only add work.
            CelestialPainter::drawSphere(p, pl.screen, r, style, light, facing, q, sphereAlpha);
        } else {
            drawCachedSphere(p, pl.screen, r, style, light, facing, !body.enabled, sphereAlpha);
        }

        // Non-color cues: dashed outline for disabled content, a badge for problems, a ring for focus/hover/match.
        p.save();
        p.setOpacity(p.opacity() * sphereAlpha);
        p.setBrush(Qt::NoBrush);
        if (body.updateAvailable) {
            // An update is available: a red outline (dashed when the content is also disabled) and a red "!".
            p.setPen(QPen(QColor(248, 81, 73), 2, body.enabled ? Qt::SolidLine : Qt::DashLine));
            p.drawEllipse(pl.screen, r + 2.5, r + 2.5);
        } else if (!body.enabled) {
            p.setPen(QPen(QColor(200, 196, 220, 170), 1, Qt::DashLine));
            p.drawEllipse(pl.screen, r + 2, r + 2);
        }
        if (emphasized) {
            p.setPen(QPen(Accent, 2));
            p.drawEllipse(pl.screen, r + 4, r + 4);
        } else if (!m_filter.isEmpty() && pl.match) {
            p.setPen(QPen(withAlphaF(Accent, 0.85), 1.5));
            p.drawEllipse(pl.screen, r + 3, r + 3);
        }
        if ((body.updateAvailable || !body.issues.isEmpty()) && r >= 3) {
            const QPointF badge = pl.screen + QPointF(r * 0.75 + 2, -r * 0.75 - 2);
            p.setPen(Qt::NoPen);
            p.setBrush(body.updateAvailable ? QColor(248, 81, 73) : QColor(251, 191, 36));
            p.drawEllipse(badge, 5.5, 5.5);
            p.setPen(body.updateAvailable ? QColor(Qt::white) : QColor(30, 20, 0));
            QFont f = p.font();
            f.setBold(true);
            f.setPointSizeF(std::max(6.0, f.pointSizeF() * 0.7));
            p.setFont(f);
            p.drawText(QRectF(badge.x() - 5.5, badge.y() - 5.5, 11, 11), Qt::AlignCenter, "!");
        }
        p.restore();

        HitShape shape;
        shape.hit = hit;
        shape.center = pl.screen;
        shape.radius = std::max(r, node.role == GalaxyLayout::Node::Role::Moon ? 6.0 : 7.0);
        m_hitShapes.append(shape);
    }
    p.restore();

    // Labels: the sun's name, and planet names when there are few enough to stay readable.
    QList<QRectF> taken;
    const QFontMetricsF fm(font());
    auto tryLabel = [&](const QPointF& anchor, const QString& text, qreal opacity, bool emphasized) {
        const qreal w = std::min(emphasized ? 254.0 : 174.0, fm.horizontalAdvance(text) + 14);
        const QRectF box(anchor.x() - w / 2, anchor.y(), w, fm.height() + 4);
        if (!emphasized && std::any_of(taken.begin(), taken.end(), [&box](const QRectF& r) { return r.intersects(box); })) {
            return;
        }
        taken.append(box);
        paintLabel(p, anchor, text, opacity, emphasized);
    };
    p.save();
    p.setOpacity(alpha);
    tryLabel(origin + QPointF(0, sunR + 8), system.name, 1.0, true);
    // Project names fade out as soon as Launch is pressed, stay hidden while the game runs, and fade back in as the
    // bodies reform.
    const qreal labelFade = easeInOut(m_labelFade);
    if (labelFade > 0.01) {
        int planets = 0;
        for (const auto& node : m_layout.nodes) {
            planets += node.role == GalaxyLayout::Node::Role::Planet ? 1 : 0;
        }
        // Front-most first so near labels win.
        QList<int> byDepth;
        for (int n = 0; n < m_layout.nodes.size(); n++) {
            byDepth.append(n);
        }
        std::sort(byDepth.begin(), byDepth.end(), [&placed](int a, int b) { return placed[a].local.y() > placed[b].local.y(); });
        for (int n : byDepth) {
            const auto& node = m_layout.nodes[n];
            const auto& body = m_bodies[node.body];
            const auto& pl = placed[n];
            const Hit hit{ HitType::Body, n };
            const bool emphasized = m_hover == hit || m_focus == hit;
            const bool filtered = !m_filter.isEmpty() && pl.match;
            // Named whenever there is room; overlapping labels are skipped below.
            const bool always = node.role == GalaxyLayout::Node::Role::Planet && pl.r >= 6 && (planets <= 60 || pl.r >= 9);
            if (!emphasized && !filtered && !always) {
                continue;
            }
            if (node.role == GalaxyLayout::Node::Role::Ring) {
                continue;  // rings are identified through their tooltip
            }
            tryLabel(pl.screen + QPointF(0, pl.r + 5), body.name, labelFade * pl.alpha * (emphasized || filtered ? 1.0 : 0.85), emphasized);
        }
    }
    p.restore();
}

void GalaxyView::drawCachedSphere(QPainter& p,
                                  QPointF center,
                                  qreal radius,
                                  const CelestialPainter::SurfaceStyle& style,
                                  QPointF light,
                                  qreal facing,
                                  bool disabled,
                                  qreal opacity)
{
    const qreal dpr = devicePixelRatioF();
    if (!qFuzzyCompare(dpr, m_spriteDpr) || m_sphereSprites.size() > 2000) {
        m_sphereSprites.clear();
        m_spriteDpr = dpr;
    }
    if (radius > 160) {
        CelestialPainter::drawSphere(p, center, radius, style, light, facing, quality(), opacity);
        return;
    }
    // Quantize: half-pixel radius steps, 48 light directions (7.5°), 8 facing levels.
    constexpr int angleBuckets = 48;
    constexpr int facingBuckets = 8;
    const auto r2 = static_cast<quint64>(std::lround(radius * 2)) & 0xFFFU;
    const qreal angle = std::atan2(light.y(), light.x());
    const auto angleBucket = static_cast<quint64>((std::lround(angle / (2 * M_PI) * angleBuckets) % angleBuckets + angleBuckets) % angleBuckets);
    const auto facingBucket = static_cast<quint64>(std::clamp<long>(std::lround((facing + 1) / 2 * (facingBuckets - 1)), 0, facingBuckets - 1));
    const quint64 key = (static_cast<quint64>(style.seed) << 32U) | (r2 << 20U) | (angleBucket << 14U) | (facingBucket << 11U) |
                        (static_cast<quint64>(quality()) << 9U) | (disabled ? 1U << 8U : 0U) | (style.surface == CelestialPainter::Surface::Terran ? 1U : 0U);

    auto it = m_sphereSprites.constFind(key);
    if (it == m_sphereSprites.constEnd()) {
        const qreal r = static_cast<qreal>(r2) / 2.0;
        const int side = static_cast<int>(std::ceil(2 * r + 4));
        QPixmap sprite(QSize(side, side) * dpr);
        sprite.setDevicePixelRatio(dpr);
        sprite.fill(Qt::transparent);
        QPainter sp(&sprite);
        sp.setRenderHint(QPainter::Antialiasing, quality() > 0);
        const qreal a = static_cast<qreal>(angleBucket) / angleBuckets * 2 * M_PI;
        const qreal f = static_cast<qreal>(facingBucket) / (facingBuckets - 1) * 2 - 1;
        CelestialPainter::drawSphere(sp, QPointF(side / 2.0, side / 2.0), r, style, QPointF(std::cos(a), std::sin(a)), f, quality());
        sp.end();
        it = m_sphereSprites.insert(key, sprite);
    }
    const qreal side = it->width() / dpr;
    p.save();
    p.setOpacity(p.opacity() * opacity);
    p.drawPixmap(center - QPointF(side / 2.0, side / 2.0), *it);
    p.restore();
}

void GalaxyView::paintStatus(QPainter& p)
{
    QString status;
    if (m_content && m_targetMode == Mode::System && m_content->isLoading()) {
        status = tr("Scanning installed content…");
    } else if (m_targetMode == Mode::Galaxy &&
               std::any_of(m_systems.begin(), m_systems.end(), [](const SystemNode& n) { return !n.summary.valid; })) {
        status = tr("Mapping your galaxy…");
    }
    if (m_targetMode == Mode::System && m_content && !m_content->isLoading() && m_bodies.size() <= 1) {
        status = tr("Nothing installed yet: use \"Add content\" to browse mods, packs and shaders");
    }
    if (status.isEmpty()) {
        return;
    }
    const QFontMetricsF fm(font());
    const QRectF box(14, 12, fm.horizontalAdvance(status) + 24, fm.height() + 10);
    p.save();
    p.setPen(QPen(withAlphaF(Accent, 0.45), 1));
    p.setBrush(QColor(14, 10, 34, 220));
    p.drawRoundedRect(box, box.height() / 2, box.height() / 2);
    p.setPen(MutedText);
    p.drawText(box, Qt::AlignCenter, status);
    p.restore();
}

void GalaxyView::paintTooltip(QPainter& p)
{
    const Hit hit = m_hover.type != HitType::None ? m_hover : m_focus;
    if (hit.type == HitType::None || m_dragging) {
        return;
    }
    QString title;
    QStringList lines;
    QString hint;
    if (hit.type == HitType::BlackHole) {
        title = blackHoleName();
        lines << tr("The black hole at the heart of your galaxy");
        lines << tr("%n profile(s) orbit it", "", static_cast<int>(m_systems.size()));
        const auto target = blackHoleTarget();
        const int index = systemIndexOf(target);
        hint = index >= 0 ? tr("Click to launch %1 · Right-click for options").arg(m_systems[index].name) : tr("Right-click for options");
    } else if (hit.type == HitType::System && hit.index < m_systems.size()) {
        const auto& node = m_systems[hit.index];
        title = node.name;
        lines << node.subtitle;
        if (node.summary.valid) {
            lines << tr("%n mod(s)", "", node.summary.mods) + " · " + tr("%n pack(s)", "", node.summary.packs) + " · " +
                         tr("%n shader(s)", "", node.summary.shaders);
        }
        if (node.running) {
            lines << tr("Running");
        }
        hint = tr("Click to enter · Right-click for profile actions");
    } else if (hit.type == HitType::Sun) {
        const int index = systemIndexOf(m_systemId);
        if (index < 0) {
            return;
        }
        const auto& node = m_systems[index];
        title = node.name;
        lines << node.subtitle;
        lines << (node.running ? tr("Running") : tr("Profile sun"));
        hint = node.running ? tr("Click to show the game console · Right-click for profile actions")
                            : tr("Click to launch · Right-click for profile actions");
    } else if (hit.type == HitType::Body && hit.index < m_layout.nodes.size()) {
        const auto& body = m_bodies[m_layout.nodes[hit.index].body];
        title = body.name;
        if (body.kind == CelestialBody::Kind::BaseGame) {
            lines << tr("Base game (not a mod)");
            if (!body.description.isEmpty()) {
                lines << body.description;
            }
            lines << tr("Packs orbit here as moons; shaders ring it unless a shader loader mod is installed");
            hint = tr("Click for details");
        } else {
            QStringList kind{ CelestialBody::kindName(body.kind) };
            if (!body.version.isEmpty()) {
                kind << body.version;
            }
            lines << kind.join(" · ");
            if (!body.authors.isEmpty()) {
                lines << tr("by %1").arg(body.authors.join(", "));
            }
            lines << (body.hasMetadata ? tr("Source: %1").arg(body.provider) : tr("Local file (no source platform metadata)"));
            lines << (body.bytes >= 0 ? humanSize(body.bytes) : tr("Size unknown (shown at a default size)"));
            if (body.updateAvailable) {
                lines << tr("⬆ Update available: %1").arg(body.latestVersion.isEmpty() ? tr("newer version") : body.latestVersion);
            }
            if (!body.enabled) {
                lines << tr("Disabled");
            }
            for (const auto& issue : body.issues) {
                lines << "⚠ " + issue;
            }
            hint = tr("Click for details · Right-click for actions");
        }
    } else {
        return;
    }

    QFont titleFont = font();
    titleFont.setBold(true);
    QFont textFont = font();
    QFont hintFont = font();
    hintFont.setPointSizeF(hintFont.pointSizeF() * 0.88);
    const QFontMetricsF tfm(titleFont), fm(textFont), hfm(hintFont);
    const qreal maxWidth = std::min(380.0, width() - 40.0);
    QStringList titleLines = wrapText(title, tfm, maxWidth);
    QStringList bodyLines;
    for (const auto& line : lines) {
        bodyLines << wrapText(line, fm, maxWidth);
    }
    QStringList hintLines = wrapText(hint, hfm, maxWidth);
    qreal w = 0;
    for (const auto& line : titleLines) {
        w = std::max(w, tfm.horizontalAdvance(line));
    }
    for (const auto& line : bodyLines) {
        w = std::max(w, fm.horizontalAdvance(line));
    }
    for (const auto& line : hintLines) {
        w = std::max(w, hfm.horizontalAdvance(line));
    }
    w = std::min(std::ceil(w) + 1, maxWidth);
    const qreal h = titleLines.size() * tfm.height() + bodyLines.size() * fm.height() + 6 + hintLines.size() * hfm.height();
    const QSizeF box(w + 24, h + 18);

    // Next to the cursor (or the focused object), flipped and clamped so it is never clipped by the window.
    QPointF anchor = m_hover.type != HitType::None ? m_mousePos : hitAnchor(hit);
    QPointF at = anchor + QPointF(18, 18);
    if (at.x() + box.width() > width() - 8) {
        at.setX(anchor.x() - 18 - box.width());
    }
    if (at.y() + box.height() > height() - 8) {
        at.setY(anchor.y() - 18 - box.height());
    }
    at.setX(std::clamp(at.x(), 8.0, std::max(8.0, width() - 8 - box.width())));
    at.setY(std::clamp(at.y(), 8.0, std::max(8.0, height() - 8 - box.height())));

    p.save();
    const QRectF rect(at, box);
    p.setPen(QPen(withAlphaF(Accent, 0.6), 1));
    p.setBrush(QColor(16, 11, 38, 240));
    p.drawRoundedRect(rect, 10, 10);
    qreal y = rect.top() + 9;
    p.setFont(titleFont);
    p.setPen(TextColor);
    for (const auto& line : titleLines) {
        p.drawText(QRectF(rect.left() + 12, y, w, tfm.height()), Qt::AlignLeft | Qt::AlignVCenter, line);
        y += tfm.height();
    }
    p.setFont(textFont);
    p.setPen(MutedText);
    for (const auto& line : bodyLines) {
        p.drawText(QRectF(rect.left() + 12, y, w, fm.height()), Qt::AlignLeft | Qt::AlignVCenter, line);
        y += fm.height();
    }
    y += 6;
    p.setFont(hintFont);
    p.setPen(Accent.lighter(110));
    for (const auto& line : hintLines) {
        p.drawText(QRectF(rect.left() + 12, y, w, hfm.height()), Qt::AlignLeft | Qt::AlignVCenter, line);
        y += hfm.height();
    }
    p.restore();
}

// ---------------------------------------------------------------------------------------------------------------
// Interaction

GalaxyView::Hit GalaxyView::hitTest(const QPointF& pos) const
{
    // Sparkles first, even over the sun: they are tiny, so the one closest to the cursor wins.
    Hit nearest;
    qreal nearestDistance = std::numeric_limits<qreal>::infinity();
    for (const auto& s : m_hitShapes) {
        const qreal distance = QLineF(pos, s.center).length();
        if (s.priority && distance <= s.radius && distance < nearestDistance) {
            nearest = s.hit;
            nearestDistance = distance;
        }
    }
    if (nearest.type != HitType::None) {
        return nearest;
    }
    // Then everything else; shapes were recorded back to front while painting, so the last match is the top-most.
    return hitTest(pos, false);
}

GalaxyView::Hit GalaxyView::hitTest(const QPointF& pos, bool priority) const
{
    for (auto it = m_hitShapes.crbegin(); it != m_hitShapes.crend(); ++it) {
        const auto& s = *it;
        if (s.priority != priority) {
            continue;
        }
        const QPointF d = pos - s.center;
        if (s.rx > 0 && s.radius > 0) {
            // Ring band: transform into the ring's frame and measure distance to the ellipse.
            const qreal a = qDegreesToRadians(-s.rotation);
            const qreal x = d.x() * std::cos(a) - d.y() * std::sin(a);
            const qreal y = d.x() * std::sin(a) + d.y() * std::cos(a);
            if ((s.frontHalfOnly && y < 0) || (s.backHalfOnly && y >= 0)) {
                continue;
            }
            const qreal q = std::hypot(x / s.rx, y / s.ry);
            if (q < 1e-6) {
                continue;
            }
            const qreal distance = std::abs(q - 1) * std::hypot(x, y) / q;
            if (distance <= s.radius) {
                return s.hit;
            }
        } else if (s.rx > 0) {
            if (std::pow(d.x() / s.rx, 2) + std::pow(d.y() / s.ry, 2) <= 1) {
                return s.hit;
            }
        } else if (std::hypot(d.x(), d.y()) <= s.radius) {
            return s.hit;
        }
    }
    return {};
}

QPointF GalaxyView::hitAnchor(const Hit& hit) const
{
    for (const auto& s : m_hitShapes) {
        if (s.hit == hit) {
            return s.center + QPointF(s.rx > 0 && s.radius == 0 ? 0 : s.radius, 0);
        }
    }
    return rect().center();
}

void GalaxyView::setHover(const Hit& hit)
{
    if (hit == m_hover) {
        return;
    }
    m_hover = hit;
    setCursor(hit.type == HitType::None ? Qt::ArrowCursor : Qt::PointingHandCursor);
    update();
}

void GalaxyView::activate(const Hit& hit)
{
    switch (hit.type) {
        case HitType::System:
            if (hit.index < m_systems.size()) {
                const auto id = m_systems[hit.index].id;
                emit instanceSelected(id);
                enterSystem(id);
            }
            break;
        case HitType::Sun:
            if (!m_systemId.isEmpty()) {
                emit launchRequested(m_systemId);
            }
            break;
        case HitType::Body:
            if (hit.index < m_layout.nodes.size()) {
                emit bodyActivated(m_systemId, m_bodies[m_layout.nodes[hit.index].body]);
            }
            break;
        case HitType::BlackHole:
            launchFromBlackHole();
            break;
        case HitType::None:
            break;
    }
}

void GalaxyView::requestContextMenu(const Hit& hit, const QPoint& globalPos)
{
    switch (hit.type) {
        case HitType::System:
            if (hit.index < m_systems.size()) {
                emit instanceSelected(m_systems[hit.index].id);
                emit sunContextMenuRequested(m_systems[hit.index].id, globalPos);
            }
            return;
        case HitType::Sun:
            emit sunContextMenuRequested(m_systemId, globalPos);
            return;
        case HitType::Body:
            if (hit.index < m_layout.nodes.size()) {
                emit bodyContextMenuRequested(m_systemId, m_bodies[m_layout.nodes[hit.index].body], globalPos);
            }
            return;
        case HitType::BlackHole: {
            QMenu menu(this);
            auto* header = menu.addAction(blackHoleName());
            header->setEnabled(false);
            menu.addSeparator();
            // What a click launches: the last played profile, or one chosen here.
            auto* launches = menu.addMenu(tr("Click launches"));
            const auto chosen = APPLICATION->settings()->get(GalaxySettings::BlackHoleProfile).toString();
            auto* group = new QActionGroup(launches);
            auto addChoice = [&](const QString& label, const QString& id) {
                auto* action = launches->addAction(label, this, [id] { APPLICATION->settings()->set(GalaxySettings::BlackHoleProfile, id); });
                action->setCheckable(true);
                action->setChecked(id == chosen || (id.isEmpty() && systemIndexOf(chosen) < 0));
                group->addAction(action);
            };
            addChoice(tr("The last played profile"), QString());
            launches->addSeparator();
            for (const auto& node : std::as_const(m_systems)) {
                addChoice(node.name, node.id);
            }
            menu.addAction(tr("Rename black hole…"), this, [this] { renameBlackHole(); });
            menu.addAction(tr("Centre the view"), this, [this] {
                m_galaxyCameraFitted = true;
                m_galaxyCamera = galaxyFitCamera();
                update();
            });
            menu.exec(globalPos);
            return;
        }
        case HitType::None:
            break;
    }
    // Empty space.
    QMenu menu(this);
    if (m_targetMode == Mode::System) {
        menu.addAction(QIcon::fromTheme("arrow-left"), tr("Back to galaxy"), this, [this] { showGalaxy(); });
    }
    menu.addAction(QIcon::fromTheme("new"), tr("New profile…"), this, &GalaxyView::createProfileRequested);
    menu.addAction(QIcon::fromTheme("viewfolder"), tr("Import profile…"), this, &GalaxyView::importProfileRequested);
    menu.addSeparator();
    menu.addAction(tr("Reset view"), this, [this] {
        m_galaxyCameraFitted = true;
        m_galaxyCamera = galaxyFitCamera();
        m_systemZoom = 1.0;
        m_systemPan = {};
        update();
    });
    menu.exec(globalPos);
}

QList<GalaxyView::Hit> GalaxyView::focusOrder() const
{
    QList<Hit> order;
    if (m_targetMode == Mode::Galaxy) {
        for (int i = 0; i < m_systems.size(); i++) {
            order.append({ HitType::System, i });
        }
        return order;
    }
    order.append({ HitType::Sun, -1 });
    for (int n = 0; n < m_layout.nodes.size(); n++) {
        order.append({ HitType::Body, n });
    }
    return order;
}

bool GalaxyView::focusNextPrevChild(bool next)
{
    m_keyboardFocus = true;
    // Tab walks through the objects in the canvas before leaving it.
    const auto order = focusOrder();
    if (hasFocus() && !order.isEmpty()) {
        const int current = static_cast<int>(order.indexOf(m_focus));
        int index = current + (next ? 1 : -1);
        if (current < 0) {
            index = next ? 0 : static_cast<int>(order.size()) - 1;
        }
        if (index >= 0 && index < order.size()) {
            m_focus = order[index];
            updateAccessibleText();
            update();
            return true;
        }
        m_focus = {};
        update();
    }
    return QWidget::focusNextPrevChild(next);
}

void GalaxyView::updateAccessibleText()
{
    QString text;
    if (m_focus.type == HitType::System && m_focus.index < m_systems.size()) {
        text = tr("Profile %1, %2").arg(m_systems[m_focus.index].name, m_systems[m_focus.index].subtitle);
    } else if (m_focus.type == HitType::Sun) {
        text = tr("Sun of %1: activate to launch").arg(m_systems.value(systemIndexOf(m_systemId)).name);
    } else if (m_focus.type == HitType::Body && m_focus.index < m_layout.nodes.size()) {
        const auto& body = m_bodies[m_layout.nodes[m_focus.index].body];
        text = QStringLiteral("%1, %2%3").arg(body.name, CelestialBody::kindName(body.kind), body.enabled ? QString() : tr(", disabled"));
    }
    setAccessibleDescription(text);
}

bool GalaxyView::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        // Tooltips are painted on the canvas itself so they follow moving objects and never leave the window.
        return true;
    }
    return QWidget::event(event);
}

void GalaxyView::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF pos = event->position();
    m_mouseInside = true;
    if (m_pressButton != Qt::NoButton && !m_dragging && (pos.toPoint() - m_pressPos).manhattanLength() > 4) {
        m_dragging = true;
        setCursor(Qt::ClosedHandCursor);
    }
    if (m_dragging) {
        const QPointF delta = pos - m_dragLast;
        if (m_targetMode == Mode::System) {
            m_systemPan += delta;
        } else {
            m_galaxyCameraFitted = false;
            m_galaxyCamera.center -= QPointF(delta.x() / m_galaxyCamera.zoom, delta.y() / (m_galaxyCamera.zoom * TiltSin));
        }
        m_dragLast = pos;
        update();
    } else {
        m_mousePos = pos;
        setHover(hitTest(pos));
        update();  // the tooltip follows the cursor
    }
}

void GalaxyView::focusInEvent(QFocusEvent* event)
{
    m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason ||
                      event->reason() == Qt::ShortcutFocusReason;
    QWidget::focusInEvent(event);
    update();
}

void GalaxyView::mousePressEvent(QMouseEvent* event)
{
    m_keyboardFocus = false;
    setFocus(Qt::MouseFocusReason);
    m_pressPos = event->position().toPoint();
    m_dragLast = event->position();
    m_pressButton = event->button();
    m_dragging = false;
    if (event->button() == Qt::BackButton) {
        showGalaxy();
    }
}

void GalaxyView::mouseReleaseEvent(QMouseEvent* event)
{
    const bool wasDragging = m_dragging;
    m_dragging = false;
    m_pressButton = Qt::NoButton;
    if (wasDragging) {
        setCursor(Qt::ArrowCursor);
        setHover(hitTest(event->position()));
        return;
    }
    const Hit hit = hitTest(event->position());
    m_focus = {};
    if (event->button() == Qt::LeftButton) {
        activate(hit);
    } else if (event->button() == Qt::RightButton) {
        requestContextMenu(hit, event->globalPosition().toPoint());
    }
}

void GalaxyView::wheelEvent(QWheelEvent* event)
{
    const qreal steps = event->angleDelta().y() / 120.0;
    if (qFuzzyIsNull(steps)) {
        return;
    }
    const qreal factor = std::pow(1.18, steps * m_zoomSpeed);
    const QPointF cursor = event->position();
    if (m_targetMode == Mode::System) {
        // Zoom out far enough to see the whole system, in far enough to read the smallest planets.
        const qreal minZoom = std::min(0.4, 0.85 * std::max(m_layout.viewExtent, GalaxyLayout::SunRadius * 3) /
                                                std::max(m_layout.extent, GalaxyLayout::SunRadius * 3));
        const qreal newZoom = std::clamp(m_systemZoom * factor, minZoom, 12.0);
        const qreal applied = newZoom / m_systemZoom;
        // Keep the point under the cursor fixed.
        const int index = systemIndexOf(m_systemId);
        if (index >= 0) {
            const Camera cam = currentGalaxyCamera();
            const QPointF sys = systemPosition(index);
            const QPointF origin = projectGalaxy(cam, sys.x(), 0, sys.y()) + m_systemPan;
            const QPointF v = cursor - origin;
            m_systemPan += v - v * applied;
        }
        m_systemZoom = newZoom;
    } else {
        const QPointF mid(width() / 2.0, height() / 2.0);
        const QPointF before((cursor.x() - mid.x()) / m_galaxyCamera.zoom, (cursor.y() - mid.y()) / (m_galaxyCamera.zoom * TiltSin));
        const qreal newZoom = std::clamp(m_galaxyCamera.zoom * factor, 6.0, 600.0);
        const QPointF after((cursor.x() - mid.x()) / newZoom, (cursor.y() - mid.y()) / (newZoom * TiltSin));
        m_galaxyCamera.center += before - after;
        m_galaxyCamera.zoom = newZoom;
        m_galaxyCameraFitted = false;
    }
    event->accept();
    update();
}

void GalaxyView::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
        case Qt::Key_Escape:
        case Qt::Key_Backspace:
            if (m_targetMode == Mode::System) {
                showGalaxy();
                return;
            }
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            if (m_focus.type != HitType::None) {
                activate(m_focus);
                return;
            }
            break;
        case Qt::Key_Menu:
            if (m_focus.type != HitType::None) {
                requestContextMenu(m_focus, mapToGlobal(hitAnchor(m_focus).toPoint()));
                return;
            }
            break;
        case Qt::Key_F10:
            if (event->modifiers() & Qt::ShiftModifier && m_focus.type != HitType::None) {
                requestContextMenu(m_focus, mapToGlobal(hitAnchor(m_focus).toPoint()));
                return;
            }
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
        case Qt::Key_Minus: {
            const int delta = event->key() == Qt::Key_Minus ? -120 : 120;
            QWheelEvent wheel(QPointF(width() / 2.0, height() / 2.0), mapToGlobal(QPointF(width() / 2.0, height() / 2.0)), QPoint(),
                              QPoint(0, delta), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            wheelEvent(&wheel);
            return;
        }
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down: {
            const qreal step = 40;
            const QPointF delta(event->key() == Qt::Key_Left ? step : event->key() == Qt::Key_Right ? -step : 0,
                                event->key() == Qt::Key_Up ? step : event->key() == Qt::Key_Down ? -step : 0);
            if (m_targetMode == Mode::System) {
                m_systemPan += delta;
            } else {
                m_galaxyCameraFitted = false;
                m_galaxyCamera.center -= QPointF(delta.x() / m_galaxyCamera.zoom, delta.y() / (m_galaxyCamera.zoom * TiltSin));
            }
            update();
            return;
        }
        case Qt::Key_F5:
            refresh();
            return;
        default:
            break;
    }
    QWidget::keyPressEvent(event);
}

void GalaxyView::leaveEvent(QEvent* event)
{
    m_mouseInside = false;
    setHover({});
    QWidget::leaveEvent(event);
}

void GalaxyView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_galaxyCameraFitted) {
        m_galaxyCamera = galaxyFitCamera();
    }
    updateEmptyState();
}

void GalaxyView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (!m_windowSignalsConnected) {
        if (auto* handle = window()->windowHandle()) {
            connect(handle, &QWindow::activeChanged, this, [this] { QTimer::singleShot(0, this, &GalaxyView::updateTicker); });
            connect(handle, &QWindow::windowStateChanged, this, [this] { QTimer::singleShot(0, this, &GalaxyView::updateTicker); });
            m_windowSignalsConnected = true;
        }
    }
    updateTicker();
}

void GalaxyView::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    m_ticker.stop();
}

void GalaxyView::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::ActivationChange || event->type() == QEvent::WindowStateChange) {
        updateTicker();
        update();
    }
}
