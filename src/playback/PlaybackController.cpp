#include <algorithm>
#include <memory>
#include "PlaybackController.h"

#include "VideoFrameSource.h"
#include "../history/ResumePolicy.h"
#include "../library/LibraryManager.h"
#include "../playlists/M3u.h"
#include "../settings/AppSettings.h"
#include "../subtitles/SubtitleFinder.h"
#include "../utilities/MediaFiles.h"
#include "../utilities/Redact.h"

#include <QClipboard>
#include <QCollator>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QTimer>
#include <QHash>
#include <QVector>
#include <QColor>
#include <QImage>
#include <QLocale>
#include <QVariant>
#include <QStringList>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QLatin1String>
#include <cmath>

Q_LOGGING_CATEGORY(lcPlayer, "lumina.player")

namespace lumina {

namespace {
constexpr int kProgressSaveMs = 5000;
const QVector<double> kSpeeds = {0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 3.0, 4.0};

bool looksLikeUrl(const QString &s)
{
    const QUrl u(s);
    return u.isValid() && u.scheme().size() > 1;   // "C:/x" has a one-letter scheme: that is a path
}
QString urlToDisplay(const QUrl &u)
{
    QUrl c = u;
    c.setUserInfo(QString());
    c.setQuery(QString());
    const QString name = QFileInfo(c.path()).fileName();
    return name.isEmpty() ? c.host() : name;
}
} // namespace

PlaybackController::PlaybackController(AppSettings *settings, LibraryManager *library, QObject *parent)
    : QObject(parent), m_settings(settings), m_library(library)
{
    QStringList args;
    if (qEnvironmentVariableIsSet("LUMINA_AUDIO_DUMMY")) args << QStringLiteral("--aout=dummy");
    args += subtitleStyleArgs();
    if (m_settings->getBool(QStringLiteral("performance/verboseEngineLog"))) args << QStringLiteral("--verbose=2");
    m_engine = new VlcEngine(args, this);
    if (const QString dev = m_settings->getString(QStringLiteral("audio/device")); !dev.isEmpty()) m_engine->setAudioDevice(dev);

    m_saveTimer.setInterval(kProgressSaveMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &PlaybackController::savePosition);
    m_prefsTimer.setSingleShot(true);
    m_prefsTimer.setInterval(1200);
    connect(&m_prefsTimer, &QTimer::timeout, this, &PlaybackController::saveMediaPrefs);
    m_loopTimer.setInterval(40);
    connect(&m_loopTimer, &QTimer::timeout, this, &PlaybackController::checkLoop);

    connect(m_engine, &VlcEngine::stateChanged, this, &PlaybackController::onState);
    connect(m_engine, &VlcEngine::timeChanged, this, &PlaybackController::onTime);
    connect(m_engine, &VlcEngine::lengthChanged, this, &PlaybackController::onLength);
    connect(m_engine, &VlcEngine::endReached, this, &PlaybackController::onEnd);
    connect(m_engine, &VlcEngine::errorOccurred, this, &PlaybackController::onError);
    connect(m_engine, &VlcEngine::tracksChanged, this, &PlaybackController::onTracks);
    connect(m_engine, &VlcEngine::seekableChanged, this, &PlaybackController::seekableChanged);
    connect(m_engine, &VlcEngine::bufferingChanged, this, [this](float p) { m_buffering = p; emit bufferingChanged(); });
    connect(m_engine, &VlcEngine::chapterChanged, this, [this] { emit chaptersChanged(); });
    connect(&m_queue, &QueueModel::currentIndexChanged, this, &PlaybackController::queueStateChanged);
    connect(&m_queue, &QueueModel::countChanged, this, &PlaybackController::queueStateChanged);

    // restore persisted global state
    m_engine->setVolume(m_settings->getInt(QStringLiteral("audio/volume")));
    m_engine->setMuted(m_settings->getBool(QStringLiteral("audio/muted")));
    m_repeat = m_settings->getString(QStringLiteral("playback/repeat"));
    if (m_repeat != QLatin1String("one") && m_repeat != QLatin1String("all")) m_repeat = QStringLiteral("off");
    m_shuffle = m_settings->getBool(QStringLiteral("playback/shuffle"));
    m_eqEnabled = m_settings->getBool(QStringLiteral("audio/eqEnabled"));
    m_eqPreset = m_settings->getInt(QStringLiteral("audio/eqPreset"));
    m_eqPreamp = m_settings->getDouble(QStringLiteral("audio/eqPreamp"));
    for (const QVariant &v : m_settings->get(QStringLiteral("audio/eqBands")).toList()) m_eqBands << v.toFloat();
    m_adjust = {{QStringLiteral("brightness"), 1.0}, {QStringLiteral("contrast"), 1.0}, {QStringLiteral("saturation"), 1.0},
                {QStringLiteral("gamma"), 1.0}, {QStringLiteral("hue"), 0.0}};
}

PlaybackController::~PlaybackController() { shutdown(); }

// ===================================================================================== state

QString PlaybackController::stateName() const
{
    switch (m_engine->state()) {
    case VlcEngine::State::Idle: return QStringLiteral("idle");
    case VlcEngine::State::Opening: return QStringLiteral("opening");
    case VlcEngine::State::Buffering: return QStringLiteral("buffering");
    case VlcEngine::State::Playing: return QStringLiteral("playing");
    case VlcEngine::State::Paused: return QStringLiteral("paused");
    case VlcEngine::State::Stopped: return QStringLiteral("stopped");
    case VlcEngine::State::Ended: return QStringLiteral("ended");
    case VlcEngine::State::Error: return QStringLiteral("error");
    }
    return QStringLiteral("idle");
}

bool PlaybackController::isBusy() const
{
    const auto s = m_engine->state();
    return s == VlcEngine::State::Opening || s == VlcEngine::State::Buffering;
}

void PlaybackController::setError(const QString &e)
{
    m_error = e;
    emit errorChanged();
}

void PlaybackController::onState(VlcEngine::State s)
{
    if (s == VlcEngine::State::Playing) {
        m_saveTimer.start();
        if (m_awaitResumeChoice) {              // waiting for the resume / start-over decision
            m_engine->pause();
        } else if (!m_tracksRestored) {
            applyMediaPrefs();
        }
        if (!m_error.isEmpty()) setError(QString());
    } else if (s == VlcEngine::State::Paused || s == VlcEngine::State::Stopped || s == VlcEngine::State::Ended ||
               s == VlcEngine::State::Error) {
        m_saveTimer.stop();
        savePosition();
    }
    emit stateChanged();
}

void PlaybackController::onTime(qint64 ms)
{
    m_positionMs = ms;
    emit positionChanged();
}

void PlaybackController::onLength(qint64 ms)
{
    if (ms == m_durationMs) return;
    m_durationMs = ms;
    emit durationChanged();
    refreshChapters();
}

void PlaybackController::onError(const QString &msg)
{
    setError(msg);
    emit message(msg, QStringLiteral("error"));
    qCWarning(lcPlayer) << "playback error:" << redactLogLine(msg);
}

void PlaybackController::onTracks()
{
    m_details.clear();
    const StreamDetails d = m_engine->streamDetails();
    if (d.valid) {
        m_details = {{QStringLiteral("videoCodec"), d.videoCodec}, {QStringLiteral("audioCodec"), d.audioCodec},
                     {QStringLiteral("width"), d.width}, {QStringLiteral("height"), d.height}, {QStringLiteral("fps"), d.fps},
                     {QStringLiteral("videoBitrate"), d.videoBitrate}, {QStringLiteral("audioBitrate"), d.audioBitrate},
                     {QStringLiteral("channels"), d.channels}, {QStringLiteral("sampleRate"), d.sampleRate}};
    }
    emit detailsChanged();
    emit tracksChanged();
    refreshChapters();
    if (!m_tracksRestored && m_engine->state() == VlcEngine::State::Playing) applyMediaPrefs();
}

void PlaybackController::onEnd()
{
    // finished: completed items restart from the beginning next time
    if (m_mediaId > 0 && !m_isStream)
        m_library->saveProgress(m_mediaId, m_durationMs, m_durationMs, true);
    m_saveTimer.stop();
    if (m_repeat == QLatin1String("one") && m_queue.currentIndex() >= 0) { playIndex(m_queue.currentIndex()); return; }
    if (!m_settings->getBool(QStringLiteral("playback/autoPlayNext"))) { emit stateChanged(); return; }
    const int n = pickNext(true);
    if (n < 0) { emit playbackEnded(); return; }
    m_autoAdvancing = true;
    startEntry(n, true);
    m_autoAdvancing = false;
}

// ===================================================================================== queue navigation

bool PlaybackController::canNext() const
{
    const int n = m_queue.count();
    if (n == 0) return false;
    if (m_repeat == QLatin1String("all") || m_shuffle) return n > 1 || m_repeat == QLatin1String("all");
    return m_queue.currentIndex() >= 0 && m_queue.currentIndex() + 1 < n;
}

bool PlaybackController::canPrevious() const
{
    if (m_queue.count() == 0) return false;
    return m_positionMs > 3000 || m_queue.currentIndex() > 0 || m_repeat == QLatin1String("all") || !m_shuffleHistory.isEmpty();
}

int PlaybackController::pickNext(bool forward)
{
    const int n = m_queue.count();
    const int cur = m_queue.currentIndex();
    if (n == 0) return -1;
    if (!m_shuffle) {
        if (forward) return cur + 1 < n ? cur + 1 : (m_repeat == QLatin1String("all") ? 0 : -1);
        return cur - 1 >= 0 ? cur - 1 : (m_repeat == QLatin1String("all") ? n - 1 : -1);
    }
    if (!forward) {
        while (!m_shuffleHistory.isEmpty()) {
            const int i = m_shuffleHistory.takeLast();
            if (i != cur && m_queue.isValid(i)) return i;
        }
        return -1;
    }
    QVector<int> candidates;
    for (int i = 0; i < n; ++i) if (i != cur && !m_shufflePlayed.contains(i)) candidates << i;
    if (candidates.isEmpty()) {
        if (m_repeat != QLatin1String("all")) return -1;
        m_shufflePlayed.clear();
        for (int i = 0; i < n; ++i) if (i != cur) candidates << i;
        if (candidates.isEmpty()) return cur;       // single-item queue repeating
    }
    return candidates.at(int(QRandomGenerator::global()->bounded(quint32(candidates.size()))));
}

bool PlaybackController::next()
{
    const int n = pickNext(true);
    if (n < 0) return false;
    return startEntry(n, false);
}

bool PlaybackController::previous()
{
    // like most players: within the first seconds go to the previous item, otherwise restart this one
    if (m_positionMs > 3000 || m_queue.count() <= 1) { seekTo(0); return true; }
    const int n = pickNext(false);
    if (n < 0) { seekTo(0); return true; }
    if (m_shuffle) { /* history already popped */ }
    return startEntry(n, false);
}

bool PlaybackController::playIndex(int index) { return m_queue.isValid(index) && startEntry(index, false); }

// ===================================================================================== opening

QVector<QueueEntry> PlaybackController::entriesFor(const QVariantList &locations) const
{
    QVector<QueueEntry> out;
    QCollator coll;
    coll.setNumericMode(true);
    coll.setCaseSensitivity(Qt::CaseInsensitive);

    auto add = [&](const QUrl &url) {
        QueueEntry e;
        e.url = url;
        e.isStream = !url.isLocalFile();
        e.mediaId = m_library->registerUrl(url);
        const QVariantMap info = e.mediaId > 0 ? m_library->mediaInfo(e.mediaId) : QVariantMap();
        e.title = info.value(QStringLiteral("title")).toString();
        if (e.title.isEmpty()) e.title = e.isStream ? urlToDisplay(url) : QFileInfo(url.toLocalFile()).completeBaseName();
        e.durationMs = info.value(QStringLiteral("durationMs")).toLongLong();
        out << e;
    };

    for (const QVariant &v : locations) {
        const QString s = v.toString().trimmed();
        if (s.isEmpty()) continue;
        QUrl u = looksLikeUrl(s) ? QUrl(s) : QUrl::fromLocalFile(s);
        if (u.isLocalFile()) {
            const QFileInfo fi(u.toLocalFile());
            if (fi.isDir()) {
                QStringList files;
                QDirIterator it(fi.absoluteFilePath(), media::mediaNameFilters(), QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot,
                                QDirIterator::Subdirectories);
                while (it.hasNext()) { const QString p = it.next(); if (!it.fileInfo().isDir() && media::isMedia(p)) files << p; }
                std::sort(files.begin(), files.end(), [&](const QString &a, const QString &b) { return coll.compare(a, b) < 0; });
                for (const QString &f : files) add(QUrl::fromLocalFile(f));
                continue;
            }
            if (media::isPlaylist(fi.fileName())) {
                for (const auto &e : m3u::readFile(fi.absoluteFilePath()))
                    add(looksLikeUrl(e.location) ? QUrl(e.location) : QUrl::fromLocalFile(e.location));
                continue;
            }
            if (media::isSubtitle(fi.fileName())) continue;     // handled by the caller (loadSubtitleFile)
        }
        add(u);
    }
    return out;
}

QVector<QueueEntry> PlaybackController::entriesForIds(const QVariantList &ids) const
{
    QVector<QueueEntry> out;
    for (const QVariant &v : ids) {
        const qint64 id = v.toLongLong();
        const QVariantMap info = m_library->mediaInfo(id);
        if (info.isEmpty()) continue;
        QueueEntry e;
        e.mediaId = id;
        e.url = m_library->urlFor(id);
        e.title = info.value(QStringLiteral("title")).toString();
        e.isStream = !e.url.isLocalFile();
        e.durationMs = info.value(QStringLiteral("durationMs")).toLongLong();
        out << e;
    }
    return out;
}

bool PlaybackController::open(const QVariantList &locations, int startIndex)
{
    // Subtitle files in a drop are applied to the current video rather than replacing it.
    QVariantList media;
    QStringList subtitles;
    for (const QVariant &v : locations) {
        const QString s = v.toString();
        if (media::isSubtitle(s)) subtitles << s; else media << s;
    }
    if (media.isEmpty()) {
        bool any = false;
        for (const QString &s : subtitles) any = loadSubtitleFile(s) || any;
        return any;
    }
    const QVector<QueueEntry> entries = entriesFor(media);
    if (entries.isEmpty()) {
        emit message(tr("Nothing playable was found."), QStringLiteral("warning"));
        return false;
    }
    m_shuffleHistory.clear();
    m_shufflePlayed.clear();
    const int start = qBound(0, startIndex, int(entries.size()) - 1);
    m_queue.setEntries(entries, start);
    const bool ok = startEntry(start, false);
    for (const QString &s : subtitles) loadSubtitleFile(s);
    return ok;
}

bool PlaybackController::openStream(const QString &urlText, const QString &user, const QString &password)
{
    QUrl url(urlText.trimmed(), QUrl::StrictMode);
    if (!url.isValid() || url.scheme().size() < 2 || url.isLocalFile()) {
        emit message(tr("That is not a valid stream address."), QStringLiteral("error"));
        return false;
    }
    // Credentials are only ever combined in memory for this one load; LibraryManager::registerUrl strips them.
    if (!user.isEmpty()) { url.setUserName(user); url.setPassword(password); }
    QueueEntry e;
    e.url = url;
    e.isStream = true;
    e.mediaId = m_library->registerUrl(url);
    e.title = urlToDisplay(url);
    m_queue.setEntries({e}, 0);
    m_shuffleHistory.clear();
    m_shufflePlayed.clear();
    return startEntry(0, false);
}

bool PlaybackController::openMediaIds(const QVariantList &ids, int startIndex)
{
    const QVector<QueueEntry> entries = entriesForIds(ids);
    if (entries.isEmpty()) return false;
    m_shuffleHistory.clear();
    m_shufflePlayed.clear();
    const int start = qBound(0, startIndex, int(entries.size()) - 1);
    m_queue.setEntries(entries, start);
    return startEntry(start, false);
}

bool PlaybackController::openFolder(const QString &folder) { return open({folder}, 0); }

bool PlaybackController::openPlaylistFile(const QString &path)
{
    QString err;
    const auto entries = m3u::readFile(path, &err);
    if (entries.isEmpty()) {
        emit message(err.isEmpty() ? tr("The playlist is empty.") : err, QStringLiteral("warning"));
        return false;
    }
    QVariantList locs;
    for (const auto &e : entries) locs << e.location;
    return open(locs, 0);
}

int PlaybackController::enqueue(const QVariantList &locations, bool playNext)
{
    const QVector<QueueEntry> entries = entriesFor(locations);
    if (entries.isEmpty()) return 0;
    const bool wasEmpty = m_queue.count() == 0;
    if (playNext) m_queue.insertNext(entries); else m_queue.append(entries);
    if (wasEmpty && !isActive()) startEntry(0, false);
    return int(entries.size());
}

int PlaybackController::enqueueMediaIds(const QVariantList &ids, bool playNext)
{
    const QVector<QueueEntry> entries = entriesForIds(ids);
    if (entries.isEmpty()) return 0;
    const bool wasEmpty = m_queue.count() == 0;
    if (playNext) m_queue.insertNext(entries); else m_queue.append(entries);
    if (wasEmpty && !isActive()) startEntry(0, false);
    return int(entries.size());
}

bool PlaybackController::saveQueueAsPlaylistFile(const QString &path)
{
    QVector<m3u::Entry> out;
    for (const QueueEntry &e : m_queue.entries()) {
        QUrl u = e.url;
        u.setUserInfo(QString());   // never write credentials into a playlist file
        out.push_back({u.isLocalFile() ? u.toLocalFile() : u.toString(), e.title, e.durationMs / 1000});
    }
    QString err;
    const bool ok = m3u::writeFile(path, out, &err);
    if (!ok) emit message(err, QStringLiteral("error"));
    return ok;
}

QStringList PlaybackController::mediaOptionsFor(const QUrl &url) const
{
    QStringList o;
    if (!url.isLocalFile()) o << QStringLiteral(":network-caching=%1").arg(m_settings->getInt(QStringLiteral("performance/networkCacheMs")));
    if (m_settings->getString(QStringLiteral("performance/hwDecoding")) == QLatin1String("none"))
        o << QStringLiteral(":avcodec-hw=none");
    return o;
}

// libVLC's text renderer reads these when the instance is created (media-level options are ignored), so they are
// instance arguments; changing them restarts the engine instance (see applySubtitleStyle()).
QStringList PlaybackController::subtitleStyleArgs() const
{
    QStringList o;
    const QString font = m_settings->getString(QStringLiteral("subtitles/font"));
    if (!font.isEmpty()) o << QStringLiteral("--freetype-font=%1").arg(font);
    // "size" is the font size in pixels at a 360p reference height; libVLC wants a divisor of the video height.
    const int size = qBound(6, m_settings->getInt(QStringLiteral("subtitles/size")), 120);
    o << QStringLiteral("--freetype-rel-fontsize=%1").arg(qMax(4, int(std::lround(360.0 / size))));
    const QColor c(m_settings->getString(QStringLiteral("subtitles/color")));
    if (c.isValid()) o << QStringLiteral("--freetype-color=%1").arg(c.rgb() & 0xFFFFFF);
    if (m_settings->getBool(QStringLiteral("subtitles/bold"))) o << QStringLiteral("--freetype-bold");   // boolean flag: has no =value form
    static const int outline[] = {0, 2, 4, 6};
    o << QStringLiteral("--freetype-outline-thickness=%1").arg(outline[qBound(0, m_settings->getInt(QStringLiteral("subtitles/outline")), 3)]);
    o << QStringLiteral("--freetype-shadow-opacity=%1").arg(m_settings->getBool(QStringLiteral("subtitles/shadow")) ? 128 : 0);
    o << QStringLiteral("--freetype-background-opacity=%1").arg(qBound(0, m_settings->getInt(QStringLiteral("subtitles/bgOpacity")), 255));
    o << QStringLiteral("--sub-margin=%1").arg(qMax(0, m_settings->getInt(QStringLiteral("subtitles/marginBottom"))));
    return o;
}

bool PlaybackController::startEntry(int index, bool autoAdvance)
{
    if (!m_queue.isValid(index)) return false;
    // rapid switching: persist where we were, then everything below belongs to the new media
    savePosition();
    saveMediaPrefs();
    ++m_loadSerial;

    // skip files that vanished when auto-advancing; for an explicit request show the error instead
    int idx = index;
    for (int guard = 0; guard < m_queue.count(); ++guard) {
        const QueueEntry &e = m_queue.at(idx);
        if (!e.url.isLocalFile() || QFileInfo::exists(e.url.toLocalFile())) break;
        if (!autoAdvance) break;
        emit message(tr("Skipped missing file: %1").arg(e.title), QStringLiteral("warning"));
        const int nx = idx + 1 < m_queue.count() ? idx + 1 : -1;
        if (nx < 0) { emit playbackEnded(); return false; }
        idx = nx;
    }

    if (m_shuffle) {
        const int cur = m_queue.currentIndex();
        if (cur >= 0 && cur != idx) m_shuffleHistory << cur;
        m_shufflePlayed.insert(idx);
    }
    const QueueEntry e = m_queue.at(idx);
    m_queue.setCurrent(idx);

    m_url = e.url;
    m_title = e.title;
    m_mediaId = e.mediaId;
    m_isStream = e.isStream;
    m_positionMs = 0;
    m_durationMs = e.durationMs;
    m_lastSavedMs = -100000;
    m_tracksRestored = false;
    m_prefSubtitle = -2;
    m_subtitleChosenByUser = false;
    m_userSubtitleFile.clear();
    m_externalSubs.clear();
    m_resumePrompt.clear();
    m_awaitResumeChoice = false;
    m_pendingSeekMs = -1;
    m_loopA = m_loopB = -1;
    m_loopTimer.stop();
    setError(QString());
    emit loopChanged();
    emit resumePromptChanged();
    emit mediaChanged();
    emit durationChanged();
    emit positionChanged();

    if (e.url.isLocalFile() && !QFileInfo::exists(e.url.toLocalFile())) {
        setError(tr("This file could not be found. It may have been moved, renamed or deleted, or its drive is not connected."));
        m_engine->stop();
        emit stateChanged();
        return false;
    }

    // per-media preferences (speed, delays, aspect, ...) are loaded before the media starts
    m_prefsLoaded = (m_settings->getBool(QStringLiteral("playback/rememberPerMedia")) && m_mediaId > 0 && !m_isStream)
                        ? m_library->loadPrefs(m_mediaId) : QVariantMap();
    m_aspect = m_prefsLoaded.value(QStringLiteral("aspect"), m_settings->getString(QStringLiteral("playback/defaultAspect"))).toString();
    if (m_aspect.isEmpty()) m_aspect = QStringLiteral("auto");
    m_zoom = m_prefsLoaded.value(QStringLiteral("zoom"), 1.0).toDouble();
    m_rotation = m_prefsLoaded.value(QStringLiteral("rotation"), 0).toInt();
    emit videoLayoutChanged();

    QStringList options = mediaOptionsFor(e.url);

    // resume handling
    if (!m_isStream && m_mediaId > 0) {
        const QVariantMap ri = m_library->resumeInfo(m_mediaId);
        if (ri.value(QStringLiteral("canResume")).toBool()) {
            const qint64 pos = ri.value(QStringLiteral("positionMs")).toLongLong();
            const QString mode = m_settings->getString(QStringLiteral("playback/resume"));
            if (mode == QLatin1String("always")) {
                options << QStringLiteral(":start-time=%1").arg(double(pos) / 1000.0, 0, 'f', 3);
            } else if (mode == QLatin1String("ask")) {
                m_awaitResumeChoice = true;
                m_resumePrompt = {{QStringLiteral("positionMs"), pos}, {QStringLiteral("durationMs"), ri.value(QStringLiteral("durationMs"))},
                                  {QStringLiteral("title"), m_title}};
                emit resumePromptChanged();
            }
        }
    }

    qCInfo(lcPlayer) << "opening" << redactUrl(m_url.toString()) << "options:" << options.size();
    const bool ok = m_engine->load(m_url, options);
    m_engine->setRate(float(m_prefsLoaded.value(QStringLiteral("speed"), m_settings->getDouble(QStringLiteral("playback/defaultSpeed"))).toDouble()));
    emit rateChanged();
    applyEq();
    if (!m_isStream) discoverSubtitles();
    if (m_mediaId > 0 && !m_isStream) { m_library->recordPlay(m_mediaId); }
    refreshBookmarks();
    emit queueStateChanged();
    emit stateChanged();
    return ok;
}

void PlaybackController::closeMedia()
{
    savePosition();
    saveMediaPrefs();
    m_engine->stop();
    m_url.clear(); m_title.clear(); m_mediaId = 0; m_isStream = false;
    m_positionMs = 0; m_durationMs = 0;
    m_awaitResumeChoice = false; m_resumePrompt.clear();
    m_queue.setEntries({}, -1);
    emit mediaChanged(); emit positionChanged(); emit durationChanged(); emit resumePromptChanged(); emit stateChanged();
}

// ===================================================================================== transport

void PlaybackController::play()
{
    if (m_awaitResumeChoice) { declineResume(); return; }
    if (m_engine->state() == VlcEngine::State::Ended || m_engine->state() == VlcEngine::State::Stopped) {
        if (m_queue.currentIndex() >= 0) { startEntry(m_queue.currentIndex(), false); return; }
    }
    m_engine->play();
}
void PlaybackController::pause() { m_engine->pause(); }
void PlaybackController::togglePause()
{
    if (m_awaitResumeChoice) { acceptResume(); return; }
    const auto s = m_engine->state();
    if (s == VlcEngine::State::Idle) return;
    if (s == VlcEngine::State::Ended || s == VlcEngine::State::Stopped || s == VlcEngine::State::Error) { play(); return; }
    m_engine->togglePause();
}
void PlaybackController::stop()
{
    savePosition();
    m_engine->stop();
    m_positionMs = 0;
    emit positionChanged();
}
void PlaybackController::restart()
{
    if (m_queue.currentIndex() >= 0 && (m_engine->state() == VlcEngine::State::Ended || m_engine->state() == VlcEngine::State::Stopped))
        startEntry(m_queue.currentIndex(), false);
    else
        seekTo(0);
}

void PlaybackController::seekTo(qint64 ms)
{
    if (!m_engine->isSeekable() && m_engine->state() != VlcEngine::State::Paused) return;
    if (m_durationMs > 0) ms = std::clamp<qint64>(ms, qint64(0), qint64(m_durationMs));
    else ms = qMax<qint64>(0, ms);
    m_engine->setTime(ms);
    m_positionMs = ms;                // optimistic update keeps the seek bar responsive
    emit positionChanged();
}
void PlaybackController::seekFraction(double f)
{
    if (m_durationMs > 0) seekTo(qint64(qBound(0.0, f, 1.0) * double(m_durationMs)));
}
void PlaybackController::seekRelative(qint64 d) { seekTo(m_positionMs + d); }
void PlaybackController::frameStep() { m_engine->nextFrame(); }

qint64 PlaybackController::smallStepMs() const { return qint64(m_settings->getInt(QStringLiteral("playback/seekSmall"))) * 1000; }
qint64 PlaybackController::largeStepMs() const { return qint64(m_settings->getInt(QStringLiteral("playback/seekLarge"))) * 1000; }

void PlaybackController::setVolume(int v)
{
    v = qBound(0, v, 200);
    if (v == m_engine->volume()) return;
    m_engine->setVolume(v);
    m_settings->set(QStringLiteral("audio/volume"), v);
    if (v > 0 && m_engine->isMuted()) setMuted(false);
    emit volumeChanged();
}
void PlaybackController::setMuted(bool m)
{
    m_engine->setMuted(m);
    m_settings->set(QStringLiteral("audio/muted"), m);
    emit volumeChanged();
}
void PlaybackController::volumeStep(int d) { setVolume(volume() + d); }

void PlaybackController::setRate(double r)
{
    r = qBound(0.25, r, 4.0);
    if (!m_engine->setRate(float(r))) return;
    emit rateChanged();
    scheduleSavePrefs();
}
QVariantList PlaybackController::speedChoices() const
{
    QVariantList l;
    for (double s : kSpeeds) l << s;
    return l;
}
void PlaybackController::speedStep(int dir)
{
    const double cur = rate();
    int best = 0;
    for (int i = 0; i < kSpeeds.size(); ++i) if (std::abs(kSpeeds[i] - cur) < std::abs(kSpeeds[best] - cur)) best = i;
    // from an off-table speed the first step lands on the neighbour in that direction
    int target = best;
    if (std::abs(kSpeeds[best] - cur) < 1e-6) target = best + (dir > 0 ? 1 : -1);
    else if ((dir > 0) != (kSpeeds[best] > cur)) target = best + (dir > 0 ? 1 : -1);
    setRate(kSpeeds[qBound(0, target, int(kSpeeds.size()) - 1)]);
}

void PlaybackController::setRepeatMode(const QString &m)
{
    const QString v = (m == QLatin1String("one") || m == QLatin1String("all")) ? m : QStringLiteral("off");
    if (v == m_repeat) return;
    m_repeat = v;
    m_settings->set(QStringLiteral("playback/repeat"), v);
    emit modeChanged();
    emit queueStateChanged();
}
void PlaybackController::cycleRepeat()
{
    setRepeatMode(m_repeat == QLatin1String("off") ? QStringLiteral("all") : m_repeat == QLatin1String("all") ? QStringLiteral("one") : QStringLiteral("off"));
}
void PlaybackController::setShuffle(bool s)
{
    if (s == m_shuffle) return;
    m_shuffle = s;
    m_shuffleHistory.clear();
    m_shufflePlayed.clear();
    if (s && m_queue.currentIndex() >= 0) m_shufflePlayed.insert(m_queue.currentIndex());
    m_settings->set(QStringLiteral("playback/shuffle"), s);
    emit modeChanged();
    emit queueStateChanged();
}

// ----------------------------------------------------------------------------------- chapters

void PlaybackController::refreshChapters()
{
    m_chapters.clear();
    for (const ChapterInfo &c : m_engine->chapters())
        m_chapters << QVariantMap{{QStringLiteral("name"), c.name.trimmed()}, {QStringLiteral("startMs"), c.startMs}, {QStringLiteral("durationMs"), c.durationMs}};
    emit chaptersChanged();
}
QVariantList PlaybackController::chapters() const { return m_chapters; }
void PlaybackController::setChapter(int i) { m_engine->setChapter(i); }
void PlaybackController::nextChapter()
{
    const int n = m_engine->chapterCount(), c = m_engine->currentChapter();
    if (n > 0 && c + 1 < n) m_engine->setChapter(c + 1);
}
void PlaybackController::previousChapter()
{
    const int c = m_engine->currentChapter();
    if (c > 0) m_engine->setChapter(c - 1); else seekTo(0);
}

// ----------------------------------------------------------------------------------- A-B loop

void PlaybackController::cycleAbLoop()
{
    if (m_loopA < 0) setLoop(m_positionMs, -1);
    else if (m_loopB < 0) {
        if (m_positionMs <= m_loopA + 200) { emit message(tr("Point B must come after point A."), QStringLiteral("warning")); return; }
        setLoop(m_loopA, m_positionMs);
    } else clearLoop();
}
void PlaybackController::setLoop(qint64 a, qint64 b)
{
    m_loopA = a; m_loopB = b;
    if (m_loopA >= 0 && m_loopB > m_loopA) m_loopTimer.start(); else m_loopTimer.stop();
    emit loopChanged();
}
void PlaybackController::clearLoop() { setLoop(-1, -1); }
void PlaybackController::checkLoop()
{
    if (m_loopA < 0 || m_loopB <= m_loopA || m_engine->state() != VlcEngine::State::Playing) return;
    const qint64 t = m_engine->time();
    if (t >= m_loopB || t < m_loopA - 1500) m_engine->setTime(m_loopA);
}

// ----------------------------------------------------------------------------------- resume prompt

void PlaybackController::acceptResume()
{
    if (!m_awaitResumeChoice) return;
    const qint64 pos = m_resumePrompt.value(QStringLiteral("positionMs")).toLongLong();
    m_awaitResumeChoice = false;
    m_resumePrompt.clear();
    emit resumePromptChanged();
    m_engine->setTime(pos);
    m_positionMs = pos;
    emit positionChanged();
    m_engine->play();
}
void PlaybackController::declineResume()
{
    if (!m_awaitResumeChoice) return;
    m_awaitResumeChoice = false;
    m_resumePrompt.clear();
    emit resumePromptChanged();
    m_engine->play();
}

// ===================================================================================== audio

QVariantList PlaybackController::audioTracks() const
{
    QVariantList l;
    for (const TrackInfo &t : m_engine->audioTracks()) l << QVariantMap{{QStringLiteral("id"), t.id}, {QStringLiteral("name"), t.name}};
    return l;
}
void PlaybackController::setAudioTrack(int id) { m_engine->setAudioTrack(id); emit tracksChanged(); scheduleSavePrefs(); }
void PlaybackController::cycleAudioTrack()
{
    const auto tr = m_engine->audioTracks();
    QVector<int> ids;
    for (const auto &t : tr) if (t.id >= 0) ids << t.id;
    if (ids.size() < 2) { emit message(QObject::tr("No other audio track."), QStringLiteral("info")); return; }
    const int i = ids.indexOf(m_engine->currentAudioTrack());
    setAudioTrack(ids.at((i + 1) % ids.size()));
}
void PlaybackController::setAudioDelayMs(qint64 ms)
{
    ms = std::clamp<qint64>(ms, qint64(-30000), qint64(30000));
    m_engine->setAudioDelayMs(ms);
    emit delaysChanged();
    scheduleSavePrefs();
}
QVariantList PlaybackController::audioDevices() const
{
    QVariantList l;
    for (const TrackInfo &t : m_engine->audioDevices()) {
        const QStringList p = t.name.split(QLatin1Char('\t'));
        l << QVariantMap{{QStringLiteral("id"), p.value(0)}, {QStringLiteral("name"), p.value(1).isEmpty() ? p.value(0) : p.value(1)}};
    }
    return l;
}
bool PlaybackController::setAudioDevice(const QString &id)
{
    const bool ok = m_engine->setAudioDevice(id);
    if (ok) m_settings->set(QStringLiteral("audio/device"), id);
    return ok;
}
bool PlaybackController::setStereoMode(const QString &mode)
{
    static const QHash<QString, int> modes = {{QStringLiteral("default"), 1}, {QStringLiteral("stereo"), 1}, {QStringLiteral("reverse"), 2},
                                              {QStringLiteral("left"), 3}, {QStringLiteral("right"), 4}, {QStringLiteral("dolby"), 5}};
    if (!modes.contains(mode)) return false;
    m_stereoMode = mode;
    return m_engine->setAudioChannelMode(modes.value(mode));
}

QVariantList PlaybackController::eqFrequencies() const
{
    QVariantList l;
    for (float f : VlcEngine::equalizerBandFrequencies()) l << double(f);
    return l;
}
QVariantList PlaybackController::eqBands() const
{
    QVariantList l;
    const int n = int(VlcEngine::equalizerBandFrequencies().size());
    for (int i = 0; i < n; ++i) l << (i < m_eqBands.size() ? double(m_eqBands[i]) : 0.0);
    return l;
}
void PlaybackController::applyEq()
{
    const int n = int(VlcEngine::equalizerBandFrequencies().size());
    while (m_eqBands.size() < n) m_eqBands << 0.f;
    m_engine->setEqualizer(m_eqEnabled, float(m_eqPreamp), m_eqBands);
}
void PlaybackController::persistEq()
{
    m_settings->set(QStringLiteral("audio/eqEnabled"), m_eqEnabled);
    m_settings->set(QStringLiteral("audio/eqPreset"), m_eqPreset);
    m_settings->set(QStringLiteral("audio/eqPreamp"), m_eqPreamp);
    m_settings->set(QStringLiteral("audio/eqBands"), eqBands());
}
void PlaybackController::setEqEnabled(bool on) { m_eqEnabled = on; applyEq(); persistEq(); emit eqChanged(); }
void PlaybackController::applyEqPreset(int index)
{
    float pre = 0;
    QVector<float> bands;
    if (!m_engine->setEqualizerPreset(index, &pre, &bands)) return;
    m_eqPreset = index; m_eqPreamp = double(pre); m_eqBands = bands; m_eqEnabled = true;
    applyEq(); persistEq(); emit eqChanged();
}
void PlaybackController::setEqPreamp(double db) { m_eqPreamp = qBound(-20.0, db, 20.0); m_eqPreset = -1; applyEq(); persistEq(); emit eqChanged(); }
void PlaybackController::setEqBand(int i, double db)
{
    if (i < 0 || i >= m_eqBands.size()) return;
    m_eqBands[i] = float(qBound(-20.0, db, 20.0));
    m_eqPreset = -1;
    applyEq(); persistEq(); emit eqChanged();
}
QStringList PlaybackController::eqProfileNames() const
{
    return m_settings->get(QStringLiteral("audio/eqProfiles")).toMap().keys();
}
bool PlaybackController::saveEqProfile(const QString &nameIn)
{
    const QString name = nameIn.simplified();
    if (name.isEmpty() || name.size() > 40) return false;
    QVariantMap profiles = m_settings->get(QStringLiteral("audio/eqProfiles")).toMap();
    profiles[name] = QVariantMap{{QStringLiteral("preamp"), m_eqPreamp}, {QStringLiteral("bands"), eqBands()}};
    return m_settings->set(QStringLiteral("audio/eqProfiles"), profiles);
}
bool PlaybackController::loadEqProfile(const QString &name)
{
    const QVariantMap p = m_settings->get(QStringLiteral("audio/eqProfiles")).toMap().value(name).toMap();
    if (p.isEmpty()) return false;
    m_eqPreamp = p.value(QStringLiteral("preamp")).toDouble();
    m_eqBands.clear();
    for (const QVariant &v : p.value(QStringLiteral("bands")).toList()) m_eqBands << v.toFloat();
    m_eqPreset = -1; m_eqEnabled = true;
    applyEq(); persistEq(); emit eqChanged();
    return true;
}
bool PlaybackController::deleteEqProfile(const QString &name)
{
    QVariantMap profiles = m_settings->get(QStringLiteral("audio/eqProfiles")).toMap();
    if (profiles.remove(name) == 0) return false;
    return m_settings->set(QStringLiteral("audio/eqProfiles"), profiles);
}

// ===================================================================================== subtitles

QVariantList PlaybackController::subtitleTracks() const
{
    QVariantList l;
    for (const TrackInfo &t : m_engine->subtitleTracks()) l << QVariantMap{{QStringLiteral("id"), t.id}, {QStringLiteral("name"), t.name}};
    return l;
}
void PlaybackController::setSubtitleTrack(int id)
{
    m_subtitleChosenByUser = true;
    m_engine->setSubtitleTrack(id);
    emit tracksChanged();
    scheduleSavePrefs();
}
void PlaybackController::cycleSubtitleTrack()
{
    const auto tr = m_engine->subtitleTracks();     // includes the "Disable" entry (id -1)
    if (tr.isEmpty()) return;
    const int cur = m_engine->currentSubtitleTrack();
    int idx = -1;
    for (int i = 0; i < tr.size(); ++i) if (tr[i].id == cur) idx = i;
    setSubtitleTrack(tr[(idx + 1) % tr.size()].id);
}
bool PlaybackController::loadSubtitleFile(const QString &pathOrUrl)
{
    QString path = pathOrUrl;
    if (looksLikeUrl(path)) path = QUrl(path).toLocalFile();
    if (!QFileInfo::exists(path) || !media::isSubtitle(path)) {
        emit message(tr("That is not a subtitle file."), QStringLiteral("warning"));
        return false;
    }
    if (!isActive()) { emit message(tr("Open a video first, then add subtitles."), QStringLiteral("info")); return false; }
    const bool ok = m_engine->addSubtitleFile(path, true);
    if (ok) {
        m_userSubtitleFile = path;
        m_subtitleChosenByUser = true;
        emit message(tr("Subtitles loaded: %1").arg(QFileInfo(path).fileName()), QStringLiteral("success"));
        scheduleSavePrefs();
    } else emit message(tr("libVLC could not load these subtitles."), QStringLiteral("error"));
    emit tracksChanged();
    return ok;
}
void PlaybackController::setSubtitleDelayMs(qint64 ms)
{
    ms = std::clamp<qint64>(ms, qint64(-60000), qint64(60000));
    m_engine->setSubtitleDelayMs(ms);
    emit delaysChanged();
    scheduleSavePrefs();
}

void PlaybackController::discoverSubtitles()
{
    m_externalSubs.clear();
    if (!m_url.isLocalFile()) return;
    const QVector<SubtitleCandidate> found = findExternalSubtitles(m_url.toLocalFile());
    const QStringList prefs = m_settings->getList(QStringLiteral("subtitles/languages"));
    QVector<SubtitleCandidate> sorted = found;
    std::stable_sort(sorted.begin(), sorted.end(), [&](const SubtitleCandidate &a, const SubtitleCandidate &b) {
        const int ra = languageRank(a.language, prefs), rb = languageRank(b.language, prefs);
        return ra != rb ? ra < rb : a.score > b.score;
    });
    for (const auto &c : sorted)
        m_externalSubs << QVariantMap{{QStringLiteral("path"), c.path}, {QStringLiteral("name"), QFileInfo(c.path).fileName()},
                                      {QStringLiteral("language"), c.language}};
    emit tracksChanged();
}

void PlaybackController::applySubtitleStyle()
{
    QStringList args;
    if (qEnvironmentVariableIsSet("LUMINA_AUDIO_DUMMY")) args << QStringLiteral("--aout=dummy");
    args += subtitleStyleArgs();
    if (m_settings->getBool(QStringLiteral("performance/verboseEngineLog"))) args << QStringLiteral("--verbose=2");
    if (args == m_engine->instanceArgs()) return;                       // nothing changed

    const bool hadMedia = isActive() && m_queue.currentIndex() >= 0 && !m_url.isEmpty();
    const qint64 pos = m_positionMs;
    const bool wasPaused = m_engine->state() == VlcEngine::State::Paused;
    const qint64 delay = m_engine->subtitleDelayMs();
    const int audio = m_engine->currentAudioTrack();
    const int sub = m_engine->currentSubtitleTrack();
    const QString file = m_userSubtitleFile;
    const float rate = m_engine->rate();

    if (!m_engine->restartInstance(args)) {
        emit message(tr("Could not apply the subtitle style."), QStringLiteral("error"));
        return;
    }
    reapplyEngineState();
    if (!hadMedia) return;

    QStringList options = mediaOptionsFor(m_url);
    options << QStringLiteral(":start-time=%1").arg(double(pos) / 1000.0, 0, 'f', 3);
    m_awaitResumeChoice = false;
    m_tracksRestored = true;                 // do not re-apply stored preferences over the user's live choices
    m_engine->load(m_url, options);
    m_engine->setRate(rate);
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(m_engine, &VlcEngine::stateChanged, this, [this, conn, audio, sub, file, delay, wasPaused](VlcEngine::State s) {
        if (s != VlcEngine::State::Playing) return;
        QObject::disconnect(*conn);                      // single shot; onState stays connected
        if (audio >= 0) m_engine->setAudioTrack(audio);
        if (!file.isEmpty()) m_engine->addSubtitleFile(file, true); else if (sub >= 0) m_engine->setSubtitleTrack(sub);
        m_engine->setSubtitleDelayMs(delay);
        if (wasPaused) m_engine->pause();
        emit tracksChanged();
    });
}

void PlaybackController::reapplyEngineState()
{
    const QString dev = m_settings->getString(QStringLiteral("audio/device"));
    if (!dev.isEmpty()) m_engine->setAudioDevice(dev);
    if (m_stereoMode != QLatin1String("default")) setStereoMode(m_stereoMode);
    applyEq();
    m_engine->setDeinterlace(m_deinterlace);
    for (auto it = m_adjust.constBegin(); it != m_adjust.constEnd(); ++it) m_engine->setAdjust(it.key(), float(it.value().toDouble()));
}

// ===================================================================================== video

void PlaybackController::setVideoSuspended(bool s)
{
    if (s == m_videoSuspendWanted) return;
    m_videoSuspendWanted = s;
    m_engine->setVideoSuspended(s && m_settings->getBool(QStringLiteral("performance/suspendVideoWhenHidden")));
    emit videoSuspendedChanged();
}
void PlaybackController::setAspectMode(const QString &m) { if (m == m_aspect) return; m_aspect = m; emit videoLayoutChanged(); scheduleSavePrefs(); }
void PlaybackController::setZoom(double z) { z = qBound(0.25, z, 8.0); if (qFuzzyCompare(z, m_zoom)) return; m_zoom = z; emit videoLayoutChanged(); scheduleSavePrefs(); }
void PlaybackController::setVideoRotation(int r) { r = (((r % 360) + 360) % 360) / 90 * 90; if (r == m_rotation) return; m_rotation = r; emit videoLayoutChanged(); scheduleSavePrefs(); }
void PlaybackController::resetVideoLayout()
{
    m_aspect = QStringLiteral("auto"); m_zoom = 1.0; m_rotation = 0;
    emit videoLayoutChanged();
    scheduleSavePrefs();
}
void PlaybackController::setAdjustValue(const QString &name, double v)
{
    if (!m_adjust.contains(name)) return;
    m_adjust[name] = v;
    m_engine->setAdjust(name, float(v));
    emit adjustChanged();
}
void PlaybackController::resetAdjust()
{
    m_adjust = {{QStringLiteral("brightness"), 1.0}, {QStringLiteral("contrast"), 1.0}, {QStringLiteral("saturation"), 1.0},
                {QStringLiteral("gamma"), 1.0}, {QStringLiteral("hue"), 0.0}};
    m_engine->resetAdjust();
    emit adjustChanged();
}
void PlaybackController::setDeinterlace(const QString &m)
{
    static const QStringList ok = {QString(), QStringLiteral("blend"), QStringLiteral("bob"), QStringLiteral("linear"), QStringLiteral("mean"),
                                   QStringLiteral("x"), QStringLiteral("yadif"), QStringLiteral("yadif2x"), QStringLiteral("phosphor"), QStringLiteral("ivtc")};
    if (!ok.contains(m)) return;
    m_deinterlace = m;
    m_engine->setDeinterlace(m);
    emit adjustChanged();
}

// ===================================================================================== bookmarks

void PlaybackController::refreshBookmarks()
{
    m_bookmarks = m_mediaId > 0 ? m_library->bookmarks(m_mediaId) : QVariantList();
    emit bookmarksChanged();
}
void PlaybackController::addBookmark(const QString &note)
{
    if (m_mediaId <= 0 || m_isStream) { emit message(tr("Bookmarks are available for files in your library."), QStringLiteral("info")); return; }
    m_library->addBookmark(m_mediaId, m_positionMs, note);
    refreshBookmarks();
    emit message(tr("Bookmark added"), QStringLiteral("success"));
}
void PlaybackController::removeBookmark(qint64 id) { m_library->removeBookmark(id); refreshBookmarks(); }
void PlaybackController::updateBookmark(qint64 id, const QString &note) { m_library->updateBookmarkNote(id, note); refreshBookmarks(); }
void PlaybackController::seekToBookmark(qint64 id)
{
    for (const QVariant &v : m_bookmarks) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("id")).toLongLong() == id) { seekTo(m.value(QStringLiteral("positionMs")).toLongLong()); return; }
    }
}

// ===================================================================================== preferences & persistence

void PlaybackController::scheduleSavePrefs() { if (m_tracksRestored || m_engine->state() == VlcEngine::State::Playing) m_prefsTimer.start(); }

void PlaybackController::applyMediaPrefs()
{
    // Called once per media, as soon as the tracks are known.
    if (m_engine->audioTracks().isEmpty() && m_engine->subtitleTracks().isEmpty() && m_engine->state() != VlcEngine::State::Playing)
        return;
    m_tracksRestored = true;
    const QVariantMap p = m_prefsLoaded;
    if (p.contains(QStringLiteral("audioDelayMs"))) m_engine->setAudioDelayMs(p.value(QStringLiteral("audioDelayMs")).toLongLong());
    else if (const int d = m_settings->getInt(QStringLiteral("audio/delayMs"))) m_engine->setAudioDelayMs(d);
    if (p.contains(QStringLiteral("subDelayMs"))) m_engine->setSubtitleDelayMs(p.value(QStringLiteral("subDelayMs")).toLongLong());
    if (p.contains(QStringLiteral("audio"))) {
        const int id = p.value(QStringLiteral("audio")).toInt();
        for (const auto &t : m_engine->audioTracks()) if (t.id == id) { m_engine->setAudioTrack(id); break; }
    }
    // subtitles: stored choice wins; otherwise auto-load the best external file when enabled
    if (p.contains(QStringLiteral("subtitleFile")) && QFileInfo::exists(p.value(QStringLiteral("subtitleFile")).toString())) {
        m_userSubtitleFile = p.value(QStringLiteral("subtitleFile")).toString();
        m_engine->addSubtitleFile(m_userSubtitleFile, true);
    } else if (p.contains(QStringLiteral("subtitle"))) {
        m_engine->setSubtitleTrack(p.value(QStringLiteral("subtitle")).toInt());
    } else if (m_settings->getBool(QStringLiteral("subtitles/autoEnable")) && !m_externalSubs.isEmpty()) {
        m_engine->addSubtitleFile(m_externalSubs.first().toMap().value(QStringLiteral("path")).toString(), true);
    }
    if (p.contains(QStringLiteral("deinterlace"))) { m_deinterlace = p.value(QStringLiteral("deinterlace")).toString(); m_engine->setDeinterlace(m_deinterlace); }
    m_adjust = {{QStringLiteral("brightness"), p.value(QStringLiteral("brightness"), 1.0)}, {QStringLiteral("contrast"), p.value(QStringLiteral("contrast"), 1.0)},
                {QStringLiteral("saturation"), p.value(QStringLiteral("saturation"), 1.0)}, {QStringLiteral("gamma"), p.value(QStringLiteral("gamma"), 1.0)},
                {QStringLiteral("hue"), p.value(QStringLiteral("hue"), 0.0)}};
    for (auto it = m_adjust.cbegin(); it != m_adjust.cend(); ++it)
        if (std::abs(it.value().toDouble() - (it.key() == QLatin1String("hue") ? 0.0 : 1.0)) > 1e-6) m_engine->setAdjust(it.key(), it.value().toFloat());
    emit adjustChanged();
    emit delaysChanged();
    emit tracksChanged();
}

void PlaybackController::saveMediaPrefs()
{
    if (!m_settings->getBool(QStringLiteral("playback/rememberPerMedia")) || m_mediaId <= 0 || m_isStream || !isActive()) return;
    QVariantMap p;
    p[QStringLiteral("speed")] = rate();
    p[QStringLiteral("aspect")] = m_aspect;
    p[QStringLiteral("zoom")] = m_zoom;
    p[QStringLiteral("rotation")] = m_rotation;
    p[QStringLiteral("audioDelayMs")] = m_engine->audioDelayMs();
    p[QStringLiteral("subDelayMs")] = m_engine->subtitleDelayMs();
    if (m_engine->currentAudioTrack() >= 0) p[QStringLiteral("audio")] = m_engine->currentAudioTrack();
    if (m_subtitleChosenByUser) {
        p[QStringLiteral("subtitle")] = m_engine->currentSubtitleTrack();
        if (!m_userSubtitleFile.isEmpty()) p[QStringLiteral("subtitleFile")] = m_userSubtitleFile;
    }
    if (!m_deinterlace.isEmpty()) p[QStringLiteral("deinterlace")] = m_deinterlace;
    for (auto it = m_adjust.cbegin(); it != m_adjust.cend(); ++it) p[it.key()] = it.value();
    m_library->savePrefs(m_mediaId, p);
}

void PlaybackController::savePosition()
{
    if (m_mediaId <= 0 || m_isStream || m_durationMs <= 0 || m_awaitResumeChoice) return;
    const auto s = m_engine->state();
    if (s == VlcEngine::State::Idle || s == VlcEngine::State::Opening) return;
    if (s == VlcEngine::State::Ended) return;          // onEnd() already recorded completion
    if (std::llabs(m_positionMs - m_lastSavedMs) < 1000) return;   // avoids needless database writes
    m_lastSavedMs = m_positionMs;
    m_library->saveProgress(m_mediaId, m_positionMs, m_durationMs, false);
}

void PlaybackController::shutdown()
{
    m_saveTimer.stop(); m_loopTimer.stop(); m_prefsTimer.stop();
    savePosition();
    saveMediaPrefs();
    if (m_engine) m_engine->stop();
}

// ===================================================================================== screenshots & info

QString PlaybackController::screenshotFileName(const QString &title, qint64 posMs, const QString &ext)
{
    QString t = title.isEmpty() ? QStringLiteral("Lumina") : title;
    static const QRegularExpression bad(QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1f]"));
    t.replace(bad, QStringLiteral("_"));
    t = t.simplified().left(80);
    const qint64 s = posMs / 1000;
    return QStringLiteral("%1 - %2-%3-%4.%5").arg(t).arg(s / 3600, 2, 10, QLatin1Char('0')).arg((s / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(s % 60, 2, 10, QLatin1Char('0')).arg(ext);
}

QString PlaybackController::takeScreenshot()
{
    VideoFramePtr f = frameSource()->latest();
    if (!f || f->width <= 0) { emit message(tr("There is no video frame to capture."), QStringLiteral("warning")); return {}; }
    // The frame is the decoded picture (subtitles included, UI overlays never): copy it before encoding.
    const QImage img = f->asImage().copy();
    QString dir = m_settings->getString(QStringLiteral("screenshots/dir"));
    if (dir.isEmpty()) dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + QStringLiteral("/Lumina");
    if (!QDir().mkpath(dir)) { emit message(tr("Cannot create the screenshot folder: %1").arg(dir), QStringLiteral("error")); return {}; }
    const QString ext = m_settings->getString(QStringLiteral("screenshots/format")) == QLatin1String("jpg") ? QStringLiteral("jpg") : QStringLiteral("png");
    const QString name = screenshotFileName(m_title, m_positionMs, ext);
    QString path = QDir(dir).filePath(name);
    for (int n = 2; QFileInfo::exists(path); ++n)        // never overwrite an existing file
        path = QDir(dir).filePath(QFileInfo(name).completeBaseName() + QStringLiteral(" (%1).").arg(n) + ext);
    if (!img.save(path, ext == QLatin1String("jpg") ? "JPG" : "PNG", ext == QLatin1String("jpg") ? 95 : -1)) {
        emit message(tr("Could not save the screenshot to %1").arg(path), QStringLiteral("error"));
        return {};
    }
    if (m_settings->getBool(QStringLiteral("screenshots/clipboard"))) copyImageToClipboard(path);
    emit screenshotSaved(path);
    emit message(tr("Screenshot saved: %1").arg(QFileInfo(path).fileName()), QStringLiteral("success"));
    return path;
}

bool PlaybackController::copyImageToClipboard(const QString &path)
{
    QClipboard *cb = QGuiApplication::clipboard();
    const QImage img(path);
    if (!cb || img.isNull()) return false;
    cb->setImage(img);
    return true;
}

QVariantList PlaybackController::mediaInfoRows() const
{
    QVariantList rows;
    auto add = [&](const QString &section, const QString &label, const QString &value) {
        rows << QVariantMap{{QStringLiteral("section"), section}, {QStringLiteral("label"), label},
                            {QStringLiteral("value"), value.isEmpty() ? QStringLiteral("—") : value}};
    };
    const QVariantMap info = m_mediaId > 0 ? m_library->mediaInfo(m_mediaId) : QVariantMap();
    const QString general = tr("General");
    add(general, tr("Title"), m_title);
    if (m_url.isLocalFile()) {
        const QFileInfo fi(m_url.toLocalFile());
        add(general, tr("File name"), fi.fileName());
        add(general, tr("Location"), fi.absolutePath());
        add(general, tr("File size"), fi.exists() ? QLocale().formattedDataSize(fi.size()) : QString());
        add(general, tr("Container"), fi.suffix().toUpper());      // by extension: libVLC 3 does not report the demuxer
    } else {
        QUrl c = m_url; c.setUserInfo(QString());
        add(general, tr("Location"), redactUrl(c.toString()));
    }
    const qint64 d = m_durationMs;
    add(general, tr("Duration"), d > 0 ? QStringLiteral("%1:%2:%3").arg(d / 3600000).arg((d / 60000) % 60, 2, 10, QLatin1Char('0')).arg((d / 1000) % 60, 2, 10, QLatin1Char('0')) : QString());
    const QString video = tr("Video");
    add(video, tr("Codec"), m_details.value(QStringLiteral("videoCodec")).toString());
    const int w = m_details.value(QStringLiteral("width")).toInt(), h = m_details.value(QStringLiteral("height")).toInt();
    add(video, tr("Resolution"), w > 0 ? QStringLiteral("%1 × %2").arg(w).arg(h) : QString());
    const double fps = m_details.value(QStringLiteral("fps")).toDouble();
    add(video, tr("Frame rate"), fps > 0 ? QStringLiteral("%1 fps").arg(fps, 0, 'f', 3) : QString());
    const int vb = m_details.value(QStringLiteral("videoBitrate")).toInt();
    add(video, tr("Bitrate"), vb > 0 ? QStringLiteral("%1 kb/s").arg(vb / 1000) : QString());
    const QString audio = tr("Audio");
    add(audio, tr("Codec"), m_details.value(QStringLiteral("audioCodec")).toString());
    const int ch = m_details.value(QStringLiteral("channels")).toInt(), sr = m_details.value(QStringLiteral("sampleRate")).toInt();
    add(audio, tr("Channels"), ch > 0 ? QString::number(ch) : QString());
    add(audio, tr("Sample rate"), sr > 0 ? QStringLiteral("%1 Hz").arg(sr) : QString());
    const int ab = m_details.value(QStringLiteral("audioBitrate")).toInt();
    add(audio, tr("Bitrate"), ab > 0 ? QStringLiteral("%1 kb/s").arg(ab / 1000) : QString());
    add(audio, tr("Tracks"), QString::number(m_engine->audioTracks().size()));
    const QString subs = tr("Subtitles");
    QStringList sn;
    for (const auto &t : m_engine->subtitleTracks()) if (t.id >= 0) sn << t.name;
    add(subs, tr("Tracks"), sn.join(QStringLiteral(", ")));
    add(subs, tr("External files"), QString::number(m_externalSubs.size()));
    const QString chap = tr("Chapters");
    if (m_chapters.isEmpty()) add(chap, tr("Chapters"), QString());
    else for (int i = 0; i < m_chapters.size(); ++i) {
        const QVariantMap c = m_chapters.at(i).toMap();
        const qint64 s = c.value(QStringLiteral("startMs")).toLongLong() / 1000;
        add(chap, QString::number(i + 1), QStringLiteral("%1:%2:%3  %4").arg(s / 3600).arg((s / 60) % 60, 2, 10, QLatin1Char('0')).arg(s % 60, 2, 10, QLatin1Char('0')).arg(c.value(QStringLiteral("name")).toString()));
    }
    const QString eng = tr("Playback engine");
    add(eng, tr("Engine"), VlcEngine::versionString());
    add(eng, tr("Video output"), tr("Software frames (libVLC video callbacks) rendered by Qt Quick"));
    add(eng, tr("Hardware decoding"), m_settings->getString(QStringLiteral("performance/hwDecoding")) == QLatin1String("none")
        ? tr("Disabled") : tr("Allowed (libVLC decides; frames are copied back for display)"));
    return rows;
}

} // namespace lumina
