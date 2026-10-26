#include "VlcEngine.h"
#include "VideoFrameSource.h"

#include <vlc/vlc.h>

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QRegularExpression>

#include <cstdarg>
#include <cstdio>

Q_LOGGING_CATEGORY(lcEngine, "lumina.engine")

namespace lumina {

namespace {

// Strip "user:password@" from anything that looks like a URL before it can reach a log.
QString redact(QString s)
{
    static const QRegularExpression re(QStringLiteral("(://)[^/@\\s]+:[^/@\\s]*@"));
    s.replace(re, QStringLiteral("\\1***@"));
    return s;
}

void vlcLogCallback(void *data, int level, const libvlc_log_t *, const char *fmt, va_list args)
{
    auto *engine = static_cast<VlcEngine *>(data);
    char buf[1024];
    std::vsnprintf(buf, sizeof buf, fmt, args);
    const char *tag = level == LIBVLC_ERROR ? "E" : level == LIBVLC_WARNING ? "W" : level == LIBVLC_NOTICE ? "N" : "D";
    engine->appendLog(QStringLiteral("[%1] %2").arg(QLatin1String(tag), redact(QString::fromUtf8(buf))));
}

QVector<TrackInfo> fromDescriptions(libvlc_track_description_t *head)
{
    QVector<TrackInfo> out;
    for (auto *t = head; t; t = t->p_next)
        out.push_back({t->i_id, QString::fromUtf8(t->psz_name ? t->psz_name : "")});
    if (head)
        libvlc_track_description_list_release(head);
    return out;
}

QString fourccToString(uint32_t cc)
{
    char s[5] = {char(cc & 0xff), char((cc >> 8) & 0xff), char((cc >> 16) & 0xff), char((cc >> 24) & 0xff), 0};
    return QString::fromLatin1(s).trimmed();
}

} // namespace

// Bridges C callbacks (VLC threads) to the engine without exposing private members.
struct EventBridge {
    static void callback(const libvlc_event_t *e, void *data)
    {
        auto *self = static_cast<VlcEngine *>(data);
        const quint64 epoch = self->m_epoch.load();
        float f = 0.f;
        qint64 i = 0;
        switch (e->type) {
        case libvlc_MediaPlayerTimeChanged:
            self->m_lastTimeMs = e->u.media_player_time_changed.new_time;
            if (self->m_timePending.exchange(true))
                return; // coalesce: GUI will read the latest value when it gets to it
            break;
        case libvlc_MediaPlayerBuffering: f = e->u.media_player_buffering.new_cache; break;
        case libvlc_MediaPlayerLengthChanged: i = e->u.media_player_length_changed.new_length; break;
        case libvlc_MediaPlayerSeekableChanged: i = e->u.media_player_seekable_changed.new_seekable; break;
        case libvlc_MediaPlayerChapterChanged: i = e->u.media_player_chapter_changed.new_chapter; break;
        default: break;
        }
        const int type = e->type;
        QMetaObject::invokeMethod(
            self, [self, type, f, i, epoch] { self->handleEvent(type, f, i, epoch); }, Qt::QueuedConnection);
    }
};

// ---------------------------------------------------------------------------

VlcEngine::VlcEngine(const QStringList &extraArgs, QObject *parent) : QObject(parent)
{
    m_frames = new VideoFrameSource(this);
    createInstance(extraArgs);
}

bool VlcEngine::createInstance(const QStringList &extraArgs)
{
    m_extraArgs = extraArgs;
    QList<QByteArray> storage;
    storage << "--no-xlib"                 // we never ask VLC to open an X11 window
            << "--no-video-title-show"
            << "--no-osd"
            << "--no-snapshot-preview"
            << "--no-stats"
            << "--verbose=1";
    for (const auto &a : extraArgs)
        storage << a.toUtf8();
    std::vector<const char *> argv;
    for (const auto &a : storage)
        argv.push_back(a.constData());

    m_instance = libvlc_new(static_cast<int>(argv.size()), argv.data());
    if (!m_instance) {
        qCCritical(lcEngine) << "libvlc_new failed (plugins not found?)";
        return false;
    }
    libvlc_set_user_agent(m_instance, "Lumina", "Lumina/0.1");
    libvlc_log_set(m_instance, vlcLogCallback, this);

    m_player = libvlc_media_player_new(m_instance);
    if (!m_player) {
        qCCritical(lcEngine) << "libvlc_media_player_new failed";
        return false;
    }
    libvlc_video_set_format_callbacks(m_player, &VideoFrameSource::formatSetup, &VideoFrameSource::formatCleanup);
    libvlc_video_set_callbacks(m_player, &VideoFrameSource::lockCb, &VideoFrameSource::unlockCb,
                               &VideoFrameSource::displayCb, m_frames);
    // Keyboard/mouse input is handled by Qt, never by VLC.
    libvlc_video_set_key_input(m_player, false);
    libvlc_video_set_mouse_input(m_player, false);
    attachEvents();
    return isValid();
}

VlcEngine::~VlcEngine() { destroyInstance(); }

void VlcEngine::destroyInstance()
{
    // Order matters: invalidate queued events, stop callbacks, then release in dependency order.
    ++m_epoch;
    if (m_player) {
        detachEvents();
        libvlc_media_player_stop(m_player);   // blocks until the video thread has finished
    }
    if (m_player) {
        libvlc_media_player_release(m_player);
        m_player = nullptr;
    }
    releaseMedia();
    if (m_instance) {
        libvlc_log_unset(m_instance);
        libvlc_release(m_instance);
        m_instance = nullptr;
    }
}

bool VlcEngine::restartInstance(const QStringList &extraArgs)
{
    const int vol = m_volume;
    const bool muted = m_muted;
    destroyInstance();
    m_state = State::Idle;
    m_frames->clear();
    const bool ok = createInstance(extraArgs);
    if (ok) {
        setVolume(vol);
        setMuted(muted);
    }
    emit stateChanged(m_state);
    return ok;
}

QString VlcEngine::versionString() { return QString::fromUtf8(libvlc_get_version()); }
QString VlcEngine::compilerString() { return QString::fromUtf8(libvlc_get_compiler()); }

void VlcEngine::attachEvents()
{
    m_events = libvlc_media_player_event_manager(m_player);
    static const libvlc_event_type_t types[] = {
        libvlc_MediaPlayerOpening,       libvlc_MediaPlayerBuffering,    libvlc_MediaPlayerPlaying,
        libvlc_MediaPlayerPaused,        libvlc_MediaPlayerStopped,      libvlc_MediaPlayerEndReached,
        libvlc_MediaPlayerEncounteredError, libvlc_MediaPlayerTimeChanged, libvlc_MediaPlayerLengthChanged,
        libvlc_MediaPlayerSeekableChanged, libvlc_MediaPlayerESAdded,    libvlc_MediaPlayerESDeleted,
        libvlc_MediaPlayerESSelected,    libvlc_MediaPlayerChapterChanged,
    };
    for (auto t : types)
        libvlc_event_attach(m_events, t, &EventBridge::callback, this);
}

void VlcEngine::detachEvents()
{
    if (!m_events)
        return;
    static const libvlc_event_type_t types[] = {
        libvlc_MediaPlayerOpening,       libvlc_MediaPlayerBuffering,    libvlc_MediaPlayerPlaying,
        libvlc_MediaPlayerPaused,        libvlc_MediaPlayerStopped,      libvlc_MediaPlayerEndReached,
        libvlc_MediaPlayerEncounteredError, libvlc_MediaPlayerTimeChanged, libvlc_MediaPlayerLengthChanged,
        libvlc_MediaPlayerSeekableChanged, libvlc_MediaPlayerESAdded,    libvlc_MediaPlayerESDeleted,
        libvlc_MediaPlayerESSelected,    libvlc_MediaPlayerChapterChanged,
    };
    for (auto t : types)
        libvlc_event_detach(m_events, t, &EventBridge::callback, this);
    m_events = nullptr;
}

void VlcEngine::releaseMedia()
{
    if (m_media) {
        libvlc_media_release(m_media);
        m_media = nullptr;
    }
}

void VlcEngine::appendLog(const QString &line)
{
    std::lock_guard<std::mutex> lock(m_logMutex);
    m_log.push_back(line);
    if (m_log.size() > 1500)
        m_log.erase(m_log.begin(), m_log.begin() + 300);
}

QStringList VlcEngine::recentLog() const
{
    std::lock_guard<std::mutex> lock(m_logMutex);
    return m_log;
}

void VlcEngine::setState(State s)
{
    if (s == m_state)
        return;
    m_state = s;
    emit stateChanged(s);
}

// ---------------------------------------------------------------------------
// Event handling (runs on the engine thread)

void VlcEngine::handleEvent(int type, float fArg, qint64 iArg, quint64 epoch)
{
    if (epoch != m_epoch.load())
        return; // belongs to a media that has since been replaced

    switch (type) {
    case libvlc_MediaPlayerOpening:
        setState(State::Opening);
        break;
    case libvlc_MediaPlayerBuffering:
        emit bufferingChanged(fArg);
        if (fArg < 100.f) {
            if (m_state == State::Opening || m_state == State::Playing)
                setState(State::Buffering);
        } else if (m_state == State::Buffering) {
            setState(State::Playing);
        }
        break;
    case libvlc_MediaPlayerPlaying:
        applyPlayerSettings();
        if (m_frames->naturalAspect() <= 0.0) {
            const StreamDetails d = streamDetails();
            if (d.displayAspect() > 0.0)
                m_frames->setNaturalAspect(d.displayAspect());
        }
        setState(State::Playing);
        emit lengthChanged(length());
        break;
    case libvlc_MediaPlayerPaused:
        setState(State::Paused);
        break;
    case libvlc_MediaPlayerStopped:
        if (m_state != State::Error && m_state != State::Ended)
            setState(State::Stopped);
        break;
    case libvlc_MediaPlayerEndReached:
        // libVLC 3 reports an unreadable/unsupported file as Playing -> EndReached, with no error
        // event. Nothing was ever decoded (no time, no length, no tracks) means it failed to open.
        if (m_lastTimeMs.load() <= 0 && length() <= 0 && !streamDetails().valid) {
            m_lastError = tr("This file is not a supported media format, or it is damaged.");
            setState(State::Error);
            emit errorOccurred(m_lastError);
            break;
        }
        setState(State::Ended);
        emit endReached();
        break;
    case libvlc_MediaPlayerEncounteredError: {
        const char *msg = libvlc_errmsg();
        m_lastError = msg ? QString::fromUtf8(msg) : QString();
        setState(State::Error);
        emit errorOccurred(m_lastError.isEmpty()
                               ? tr("This media could not be opened or decoded.")
                               : redact(m_lastError));
        break;
    }
    case libvlc_MediaPlayerTimeChanged:
        m_timePending = false;
        emit timeChanged(m_lastTimeMs.load());
        break;
    case libvlc_MediaPlayerLengthChanged:
        emit lengthChanged(iArg);
        break;
    case libvlc_MediaPlayerSeekableChanged:
        m_seekable = iArg != 0;
        emit seekableChanged(m_seekable);
        break;
    case libvlc_MediaPlayerESAdded:
    case libvlc_MediaPlayerESDeleted:
    case libvlc_MediaPlayerESSelected:
        emit tracksChanged();
        break;
    case libvlc_MediaPlayerChapterChanged:
        emit chapterChanged(static_cast<int>(iArg));
        break;
    default:
        break;
    }
}

void VlcEngine::applyPlayerSettings()
{
    // libVLC drops some settings issued before the audio/video outputs exist.
    libvlc_audio_set_volume(m_player, m_volume);
    libvlc_audio_set_mute(m_player, m_muted ? 1 : 0);
    if (m_rate != 1.0f)
        libvlc_media_player_set_rate(m_player, m_rate);
    if (m_audioDelayMs)
        libvlc_audio_set_delay(m_player, m_audioDelayMs * 1000);
    if (m_subDelayMs)
        libvlc_video_set_spu_delay(m_player, m_subDelayMs * 1000);
}

// ---------------------------------------------------------------------------
// Transport

bool VlcEngine::load(const QUrl &url, const QStringList &mediaOptions)
{
    if (!isValid())
        return false;

    // Stop the previous media first (synchronous), then invalidate in-flight events.
    libvlc_media_player_stop(m_player);
    ++m_epoch;
    m_timePending = false;
    m_lastTimeMs = 0;
    m_seekable = false;
    m_lastError.clear();
    m_frames->clear();

    releaseMedia();
    if (url.isLocalFile()) {
        const QString path = QDir::toNativeSeparators(url.toLocalFile());
        m_media = libvlc_media_new_path(m_instance, path.toUtf8().constData());
    } else {
        m_media = libvlc_media_new_location(m_instance, url.toEncoded().constData());
    }
    if (!m_media) {
        setState(State::Error);
        emit errorOccurred(tr("The location is not valid."));
        return false;
    }
    for (const auto &opt : mediaOptions)
        libvlc_media_add_option(m_media, opt.toUtf8().constData());

    // Local files: parse first (fast, synchronous) so the renderer knows the exact visible size and
    // pixel aspect before the first frame. Network sources are never parsed synchronously.
    m_frames->setSizeHint(QSize());
    m_frames->setNaturalAspect(0.0);
    if (url.isLocalFile()) {
        QT_WARNING_PUSH
        QT_WARNING_DISABLE_DEPRECATED
        libvlc_media_parse(m_media);
        QT_WARNING_POP
        const StreamDetails d = streamDetails();
        if (d.width > 0 && d.height > 0) {
            m_frames->setSizeHint(QSize(d.width, d.height));
            m_frames->setNaturalAspect(d.displayAspect());
        }
    }

    libvlc_media_player_set_media(m_player, m_media);
    m_currentUrl = url;
    setState(State::Opening);
    emit mediaLoaded(url);
    return libvlc_media_player_play(m_player) == 0;
}

void VlcEngine::play()
{
    if (!isValid() || !m_media)
        return;
    if (m_state == State::Paused)
        libvlc_media_player_set_pause(m_player, 0);
    else if (m_state != State::Playing && m_state != State::Buffering && m_state != State::Opening) {
        ++m_epoch;   // restart (stopped/ended): ignore leftovers from the previous run
        libvlc_media_player_play(m_player);
    }
}

void VlcEngine::pause()
{
    if (isValid() && (m_state == State::Playing || m_state == State::Buffering))
        libvlc_media_player_set_pause(m_player, 1);
}

void VlcEngine::togglePause()
{
    if (m_state == State::Playing || m_state == State::Buffering)
        pause();
    else
        play();
}

void VlcEngine::stop()
{
    if (!isValid())
        return;
    libvlc_media_player_stop(m_player);
    ++m_epoch;
    m_timePending = false;
    m_frames->clear();
    setState(State::Stopped);
    emit timeChanged(0);
}

void VlcEngine::nextFrame()
{
    if (!isValid())
        return;
    if (m_state == State::Playing)
        pause();
    libvlc_media_player_next_frame(m_player);
}

qint64 VlcEngine::time() const { return isValid() ? libvlc_media_player_get_time(m_player) : 0; }
qint64 VlcEngine::length() const { return isValid() ? libvlc_media_player_get_length(m_player) : 0; }
double VlcEngine::position() const { return isValid() ? libvlc_media_player_get_position(m_player) : 0.0; }

void VlcEngine::setTime(qint64 ms)
{
    if (isValid() && m_seekable)
        libvlc_media_player_set_time(m_player, qMax<qint64>(0, ms));
}

void VlcEngine::setPosition(double fraction)
{
    if (isValid() && m_seekable)
        libvlc_media_player_set_position(m_player, static_cast<float>(qBound(0.0, fraction, 1.0)));
}

// ---------------------------------------------------------------------------
// Audio

void VlcEngine::setVolume(int percent)
{
    m_volume = qBound(0, percent, 200);
    if (isValid())
        libvlc_audio_set_volume(m_player, m_volume);
}

void VlcEngine::setMuted(bool muted)
{
    m_muted = muted;
    if (isValid())
        libvlc_audio_set_mute(m_player, muted ? 1 : 0);
}

bool VlcEngine::setRate(float rate)
{
    rate = qBound(0.25f, rate, 8.0f);
    m_rate = rate;
    return isValid() && libvlc_media_player_set_rate(m_player, rate) == 0;
}

QVector<TrackInfo> VlcEngine::audioTracks() const
{
    return isValid() ? fromDescriptions(libvlc_audio_get_track_description(m_player)) : QVector<TrackInfo>{};
}
int VlcEngine::currentAudioTrack() const { return isValid() ? libvlc_audio_get_track(m_player) : -1; }
bool VlcEngine::setAudioTrack(int id) { return isValid() && libvlc_audio_set_track(m_player, id) == 0; }

bool VlcEngine::setAudioDelayMs(qint64 ms)
{
    m_audioDelayMs = ms;
    return isValid() && libvlc_audio_set_delay(m_player, ms * 1000) == 0;
}

QVector<TrackInfo> VlcEngine::audioDevices() const
{
    QVector<TrackInfo> out; // id is meaningless for devices; name carries "id\tdescription"
    if (!isValid())
        return out;
    auto *list = libvlc_audio_output_device_enum(m_player);
    for (auto *d = list; d; d = d->p_next)
        out.push_back({0, QStringLiteral("%1\t%2").arg(QString::fromUtf8(d->psz_device ? d->psz_device : ""),
                                                      QString::fromUtf8(d->psz_description ? d->psz_description : ""))});
    if (list)
        libvlc_audio_output_device_list_release(list);
    return out;
}

bool VlcEngine::setAudioDevice(const QString &deviceId)
{
    if (!isValid())
        return false;
    const QByteArray id = deviceId.toUtf8();
    libvlc_audio_output_device_set(m_player, nullptr, deviceId.isEmpty() ? nullptr : id.constData()); // void in libVLC 3
    return true;
}

bool VlcEngine::setAudioChannelMode(int mode) { return isValid() && libvlc_audio_set_channel(m_player, mode) == 0; }

QStringList VlcEngine::equalizerPresetNames()
{
    QStringList out;
    for (unsigned i = 0, n = libvlc_audio_equalizer_get_preset_count(); i < n; ++i)
        out << QString::fromUtf8(libvlc_audio_equalizer_get_preset_name(i));
    return out;
}

QVector<float> VlcEngine::equalizerBandFrequencies()
{
    QVector<float> out;
    for (unsigned i = 0, n = libvlc_audio_equalizer_get_band_count(); i < n; ++i)
        out << libvlc_audio_equalizer_get_band_frequency(i);
    return out;
}

bool VlcEngine::setEqualizer(bool enabled, float preamp, const QVector<float> &bands)
{
    if (!isValid())
        return false;
    if (!enabled)
        return libvlc_media_player_set_equalizer(m_player, nullptr) == 0;
    libvlc_equalizer_t *eq = libvlc_audio_equalizer_new();
    if (!eq)
        return false;
    libvlc_audio_equalizer_set_preamp(eq, qBound(-20.f, preamp, 20.f));
    for (int i = 0; i < bands.size(); ++i)
        libvlc_audio_equalizer_set_amp_at_index(eq, qBound(-20.f, bands[i], 20.f), static_cast<unsigned>(i));
    const bool ok = libvlc_media_player_set_equalizer(m_player, eq) == 0;
    libvlc_audio_equalizer_release(eq);
    return ok;
}

bool VlcEngine::setEqualizerPreset(int presetIndex, float *preampOut, QVector<float> *bandsOut)
{
    if (!isValid() || presetIndex < 0 || presetIndex >= int(libvlc_audio_equalizer_get_preset_count()))
        return false;
    libvlc_equalizer_t *eq = libvlc_audio_equalizer_new_from_preset(static_cast<unsigned>(presetIndex));
    if (!eq)
        return false;
    if (preampOut)
        *preampOut = libvlc_audio_equalizer_get_preamp(eq);
    if (bandsOut) {
        bandsOut->clear();
        for (unsigned i = 0, n = libvlc_audio_equalizer_get_band_count(); i < n; ++i)
            bandsOut->push_back(libvlc_audio_equalizer_get_amp_at_index(eq, i));
    }
    const bool ok = libvlc_media_player_set_equalizer(m_player, eq) == 0;
    libvlc_audio_equalizer_release(eq);
    return ok;
}

// ---------------------------------------------------------------------------
// Subtitles

QVector<TrackInfo> VlcEngine::subtitleTracks() const
{
    return isValid() ? fromDescriptions(libvlc_video_get_spu_description(m_player)) : QVector<TrackInfo>{};
}
int VlcEngine::currentSubtitleTrack() const { return isValid() ? libvlc_video_get_spu(m_player) : -1; }
bool VlcEngine::setSubtitleTrack(int id) { return isValid() && libvlc_video_set_spu(m_player, id) == 0; }

bool VlcEngine::addSubtitleFile(const QString &path, bool select)
{
    if (!isValid())
        return false;
    const QByteArray uri = QUrl::fromLocalFile(path).toEncoded();
    return libvlc_media_player_add_slave(m_player, libvlc_media_slave_type_subtitle, uri.constData(), select) == 0;
}

bool VlcEngine::setSubtitleDelayMs(qint64 ms)
{
    m_subDelayMs = ms;
    return isValid() && libvlc_video_set_spu_delay(m_player, ms * 1000) == 0;
}

// ---------------------------------------------------------------------------
// Video

bool VlcEngine::setAdjust(const QString &name, float value)
{
    if (!isValid())
        return false;
    unsigned option;
    if (name == QLatin1String("contrast")) option = libvlc_adjust_Contrast;
    else if (name == QLatin1String("brightness")) option = libvlc_adjust_Brightness;
    else if (name == QLatin1String("hue")) option = libvlc_adjust_Hue;
    else if (name == QLatin1String("saturation")) option = libvlc_adjust_Saturation;
    else if (name == QLatin1String("gamma")) option = libvlc_adjust_Gamma;
    else return false;
    libvlc_video_set_adjust_int(m_player, libvlc_adjust_Enable, 1);
    libvlc_video_set_adjust_float(m_player, option, value);
    return true;
}

void VlcEngine::resetAdjust()
{
    if (!isValid())
        return;
    libvlc_video_set_adjust_float(m_player, libvlc_adjust_Contrast, 1.f);
    libvlc_video_set_adjust_float(m_player, libvlc_adjust_Brightness, 1.f);
    libvlc_video_set_adjust_float(m_player, libvlc_adjust_Saturation, 1.f);
    libvlc_video_set_adjust_float(m_player, libvlc_adjust_Gamma, 1.f);
    libvlc_video_set_adjust_float(m_player, libvlc_adjust_Hue, 0.f);
    libvlc_video_set_adjust_int(m_player, libvlc_adjust_Enable, 0);
}

bool VlcEngine::setDeinterlace(const QString &mode)
{
    if (!isValid())
        return false;
    libvlc_video_set_deinterlace(m_player, mode.isEmpty() ? nullptr : mode.toUtf8().constData());
    return true;
}

int VlcEngine::chapterCount() const { return isValid() ? libvlc_media_player_get_chapter_count(m_player) : 0; }
int VlcEngine::currentChapter() const { return isValid() ? libvlc_media_player_get_chapter(m_player) : -1; }

QVector<ChapterInfo> VlcEngine::chapters() const
{
    QVector<ChapterInfo> out;
    if (!isValid())
        return out;
    libvlc_chapter_description_t **arr = nullptr;
    const int n = libvlc_media_player_get_full_chapter_descriptions(m_player, -1, &arr);
    for (int i = 0; i < n; ++i)
        out.push_back({arr[i]->i_time_offset, arr[i]->i_duration,
                       QString::fromUtf8(arr[i]->psz_name ? arr[i]->psz_name : "")});
    if (arr)
        libvlc_chapter_descriptions_release(arr, static_cast<unsigned>(qMax(n, 0)));
    return out;
}

bool VlcEngine::setChapter(int index)
{
    if (!isValid())
        return false;
    libvlc_media_player_set_chapter(m_player, index);
    return true;
}

StreamDetails VlcEngine::streamDetails() const
{
    StreamDetails d;
    if (!m_media)
        return d;
    libvlc_media_track_t **tracks = nullptr;
    const unsigned n = libvlc_media_tracks_get(m_media, &tracks);
    for (unsigned i = 0; i < n; ++i) {
        const auto *t = tracks[i];
        switch (t->i_type) {
        case libvlc_track_video:
            if (d.videoCodec.isEmpty()) {
                d.videoCodec = fourccToString(t->i_codec);
                d.width = int(t->video->i_width);
                d.height = int(t->video->i_height);
                if (t->video->i_frame_rate_den)
                    d.fps = double(t->video->i_frame_rate_num) / double(t->video->i_frame_rate_den);
                d.videoBitrate = int(t->i_bitrate);
                d.sarNum = t->video->i_sar_num;
                d.sarDen = t->video->i_sar_den;
            }
            break;
        case libvlc_track_audio:
            if (d.audioCodec.isEmpty()) {
                d.audioCodec = fourccToString(t->i_codec);
                d.channels = int(t->audio->i_channels);
                d.sampleRate = int(t->audio->i_rate);
                d.audioBitrate = int(t->i_bitrate);
            }
            break;
        case libvlc_track_text:
            d.subtitleTracks << QStringLiteral("%1 (%2)")
                                    .arg(t->psz_description ? QString::fromUtf8(t->psz_description) : tr("Subtitle"),
                                         t->psz_language ? QString::fromUtf8(t->psz_language) : tr("und"));
            break;
        default: break;
        }
    }
    if (tracks)
        libvlc_media_tracks_release(tracks, n);
    d.valid = n > 0;
    return d;
}

} // namespace lumina
