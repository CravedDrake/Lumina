#pragma once

#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVector>

#include <atomic>
#include <memory>
#include <mutex>

struct libvlc_instance_t;
struct libvlc_media_player_t;
struct libvlc_media_t;
struct libvlc_event_manager_t;

namespace lumina {

class VideoFrameSource;

struct TrackInfo {
    int id = -1;
    QString name;
};

struct ChapterInfo {
    qint64 startMs = 0;
    qint64 durationMs = 0;
    QString name;
};

struct StreamDetails {           // Everything libVLC reports about the loaded media.
    QString videoCodec, audioCodec;
    int width = 0, height = 0;
    double fps = 0.0;
    int videoBitrate = 0, audioBitrate = 0;   // bits per second, 0 = unknown
    int channels = 0, sampleRate = 0;
    unsigned sarNum = 1, sarDen = 1;
    double displayAspect() const {
        if (width <= 0 || height <= 0) return 0.0;
        const double sar = (sarNum > 0 && sarDen > 0) ? double(sarNum) / double(sarDen) : 1.0;
        return double(width) * sar / double(height);
    }
    QStringList subtitleTracks;
    bool valid = false;
};

/**
 * Thin, thread-safe-by-construction wrapper around one libvlc instance and one
 * libvlc media player. All public methods must be called from the engine's
 * thread (the GUI thread). libVLC events arrive on VLC threads and are
 * re-posted to the engine thread as queued calls, tagged with an "epoch" so
 * that events belonging to a previously loaded media are discarded. This is what
 * makes rapid media switching safe.
 *
 * Video is delivered through libVLC's software callbacks into a VideoFrameSource.
 */
class VlcEngine : public QObject {
    Q_OBJECT
public:
    enum class State { Idle, Opening, Buffering, Playing, Paused, Stopped, Ended, Error };
    Q_ENUM(State)

    explicit VlcEngine(const QStringList &extraArgs = {}, QObject *parent = nullptr);
    ~VlcEngine() override;

    /// Recreates libVLC with new instance-level options (needed for text-renderer style, which libVLC reads
    /// only when the instance is created). Stops playback; volume/mute are kept, everything else must be re-applied.
    bool restartInstance(const QStringList &extraArgs);
    QStringList instanceArgs() const { return m_extraArgs; }
    bool isValid() const { return m_instance && m_player; }
    VideoFrameSource *frameSource() const { return m_frames; }
    static QString versionString();
    static QString compilerString();

    // ---- transport ---------------------------------------------------------
    bool load(const QUrl &url, const QStringList &mediaOptions = {});
    void play();
    void pause();
    void togglePause();
    /// Stop decoding/converting the video track while no window shows it (minimised, nothing visible).
    /// Audio keeps playing. Re-enabled when `suspended` becomes false; also re-applied to the next media.
    void setVideoSuspended(bool suspended);
    bool videoSuspended() const { return m_videoSuspended; }
    void stop();
    void nextFrame();
    State state() const { return m_state; }

    qint64 time() const;
    qint64 length() const;
    double position() const;
    bool isSeekable() const { return m_seekable; }
    void setTime(qint64 ms);
    void setPosition(double fraction);

    // ---- audio -------------------------------------------------------------
    void setVolume(int percent);          // 0..200
    int volume() const { return m_volume; }
    void setMuted(bool muted);
    bool isMuted() const { return m_muted; }
    bool setRate(float rate);
    float rate() const { return m_rate; }
    QVector<TrackInfo> audioTracks() const;
    int currentAudioTrack() const;
    bool setAudioTrack(int id);
    bool setAudioDelayMs(qint64 ms);
    qint64 audioDelayMs() const { return m_audioDelayMs; }
    QVector<TrackInfo> audioDevices() const;
    bool setAudioDevice(const QString &deviceId);
    bool setAudioChannelMode(int mode);   // 1 stereo, 2 reverse, 3 left, 4 right, 5 dolby
    static QStringList equalizerPresetNames();
    static QVector<float> equalizerBandFrequencies();
    bool setEqualizer(bool enabled, float preamp, const QVector<float> &bands);
    bool setEqualizerPreset(int presetIndex, float *preampOut, QVector<float> *bandsOut);

    // ---- subtitles ---------------------------------------------------------
    QVector<TrackInfo> subtitleTracks() const;
    int currentSubtitleTrack() const;
    bool setSubtitleTrack(int id);        // -1 disables
    bool addSubtitleFile(const QString &path, bool select = true);
    bool setSubtitleDelayMs(qint64 ms);
    qint64 subtitleDelayMs() const { return m_subDelayMs; }

    // ---- video -------------------------------------------------------------
    bool setAdjust(const QString &name, float value); // brightness|contrast|saturation|gamma|hue
    void resetAdjust();
    bool setDeinterlace(const QString &mode);         // "" disables
    int chapterCount() const;
    int currentChapter() const;
    QVector<ChapterInfo> chapters() const;
    bool setChapter(int index);
    StreamDetails streamDetails() const;

    // ---- diagnostics -------------------------------------------------------
    QStringList recentLog() const;
    QString lastErrorMessage() const { return m_lastError; }

signals:
    void stateChanged(lumina::VlcEngine::State state);
    void timeChanged(qint64 ms);
    void lengthChanged(qint64 ms);
    void seekableChanged(bool seekable);
    void bufferingChanged(float percent);
    void tracksChanged();
    void chapterChanged(int index);
    void endReached();
    void errorOccurred(const QString &message);
    void mediaLoaded(const QUrl &url);

private:
    friend struct EventBridge;
    void attachEvents();
    void detachEvents();
    void handleEvent(int type, float fArg, qint64 iArg, quint64 epoch);
    void setState(State s);
    void applyVideoSuspension();
    void applyPlayerSettings();
    void releaseMedia();
public:
    void appendLog(const QString &line);   // used by the libVLC log callback (thread-safe)
private:

    libvlc_instance_t *m_instance = nullptr;
    libvlc_media_player_t *m_player = nullptr;
    libvlc_media_t *m_media = nullptr;
    libvlc_event_manager_t *m_events = nullptr;
    VideoFrameSource *m_frames = nullptr;

    State m_state = State::Idle;
    std::atomic<quint64> m_epoch{0};
    std::atomic<bool> m_timePending{false};
    std::atomic<qint64> m_lastTimeMs{0};
    bool m_seekable = false;
    bool m_suspendRequested = false;   // what the UI wants
    bool m_videoSuspended = false;     // what is currently applied to the player
    int m_savedVideoTrack = -1;
    bool createInstance(const QStringList &extraArgs);
    void destroyInstance();
    QStringList m_extraArgs;
    int m_volume = 100;
    bool m_muted = false;
    float m_rate = 1.0f;
    qint64 m_audioDelayMs = 0;
    qint64 m_subDelayMs = 0;
    QString m_lastError;
    QUrl m_currentUrl;

    mutable std::mutex m_logMutex;
    QStringList m_log;
};

} // namespace lumina
