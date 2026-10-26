#pragma once

#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include "QueueModel.h"
#include "VlcEngine.h"
#include "VideoFrameSource.h"

namespace lumina {

class AppSettings;
class LibraryManager;

/**
 * QML-facing playback hub ("Player"). Owns the engine and the queue and implements everything that is
 * policy rather than mechanism: auto-advance, repeat/shuffle, resume prompts, A-B loop, per-media
 * preferences, subtitle discovery/styling, progress saving and screenshots.
 *
 * Threading: GUI thread only (libVLC events are already marshalled onto it by VlcEngine).
 */
class PlaybackController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ stateName NOTIFY stateChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY stateChanged)
    Q_PROPERTY(bool active READ isActive NOTIFY stateChanged)            // something is loaded (any state but idle)
    Q_PROPERTY(bool busy READ isBusy NOTIFY stateChanged)                // opening / buffering
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY positionChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY durationChanged)
    Q_PROPERTY(bool seekable READ seekable NOTIFY seekableChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY volumeChanged)
    Q_PROPERTY(double rate READ rate WRITE setRate NOTIFY rateChanged)
    Q_PROPERTY(double bufferingPercent READ bufferingPercent NOTIFY bufferingChanged)
    Q_PROPERTY(QString title READ title NOTIFY mediaChanged)
    Q_PROPERTY(QUrl currentUrl READ currentUrl NOTIFY mediaChanged)
    Q_PROPERTY(qint64 currentMediaId READ currentMediaId NOTIFY mediaChanged)
    Q_PROPERTY(bool isStream READ isStream NOTIFY mediaChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorChanged)
    Q_PROPERTY(lumina::QueueModel *queue READ queue CONSTANT)
    Q_PROPERTY(lumina::VideoFrameSource *frameSource READ frameSource CONSTANT)
    Q_PROPERTY(QString repeatMode READ repeatMode WRITE setRepeatMode NOTIFY modeChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY modeChanged)
    Q_PROPERTY(bool canNext READ canNext NOTIFY queueStateChanged)
    Q_PROPERTY(bool canPrevious READ canPrevious NOTIFY queueStateChanged)
    Q_PROPERTY(qint64 loopA READ loopA NOTIFY loopChanged)
    Q_PROPERTY(qint64 loopB READ loopB NOTIFY loopChanged)
    Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY tracksChanged)
    Q_PROPERTY(QVariantList subtitleTracks READ subtitleTracks NOTIFY tracksChanged)
    Q_PROPERTY(int audioTrack READ audioTrack NOTIFY tracksChanged)
    Q_PROPERTY(int subtitleTrack READ subtitleTrack NOTIFY tracksChanged)
    Q_PROPERTY(QVariantList externalSubtitles READ externalSubtitles NOTIFY tracksChanged)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY chaptersChanged)
    Q_PROPERTY(int currentChapter READ currentChapter NOTIFY chaptersChanged)
    Q_PROPERTY(qint64 subtitleDelayMs READ subtitleDelayMs NOTIFY delaysChanged)
    Q_PROPERTY(qint64 audioDelayMs READ audioDelayMs NOTIFY delaysChanged)
    Q_PROPERTY(QString aspectMode READ aspectMode WRITE setAspectMode NOTIFY videoLayoutChanged)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY videoLayoutChanged)
    Q_PROPERTY(int videoRotation READ videoRotation WRITE setVideoRotation NOTIFY videoLayoutChanged)
    Q_PROPERTY(QVariantMap adjust READ adjust NOTIFY adjustChanged)
    Q_PROPERTY(QString deinterlace READ deinterlace WRITE setDeinterlace NOTIFY adjustChanged)
    Q_PROPERTY(QVariantMap resumePrompt READ resumePrompt NOTIFY resumePromptChanged)
    Q_PROPERTY(QVariantMap details READ details NOTIFY detailsChanged)
    Q_PROPERTY(bool eqEnabled READ eqEnabled NOTIFY eqChanged)
    Q_PROPERTY(int eqPreset READ eqPreset NOTIFY eqChanged)
    Q_PROPERTY(double eqPreamp READ eqPreamp NOTIFY eqChanged)
    Q_PROPERTY(QVariantList eqBands READ eqBands NOTIFY eqChanged)
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY bookmarksChanged)
    Q_PROPERTY(QString engineVersion READ engineVersion CONSTANT)

public:
    PlaybackController(AppSettings *settings, LibraryManager *library, QObject *parent = nullptr);
    ~PlaybackController() override;

    VlcEngine *engine() const { return m_engine; }
    VideoFrameSource *frameSource() const { return m_engine->frameSource(); }
    QueueModel *queue() { return &m_queue; }

    QString stateName() const;
    bool isPlaying() const { return m_engine->state() == VlcEngine::State::Playing; }
    bool isActive() const { return m_engine->state() != VlcEngine::State::Idle; }
    bool isBusy() const;
    qint64 positionMs() const { return m_positionMs; }
    qint64 durationMs() const { return m_durationMs; }
    bool seekable() const { return m_engine->isSeekable(); }
    int volume() const { return m_engine->volume(); }
    void setVolume(int v);
    bool muted() const { return m_engine->isMuted(); }
    void setMuted(bool m);
    double rate() const { return double(m_engine->rate()); }
    void setRate(double r);
    double bufferingPercent() const { return m_buffering; }
    QString title() const { return m_title; }
    QUrl currentUrl() const { return m_url; }
    qint64 currentMediaId() const { return m_mediaId; }
    bool isStream() const { return m_isStream; }
    QString errorText() const { return m_error; }
    QString repeatMode() const { return m_repeat; }
    void setRepeatMode(const QString &m);
    bool shuffle() const { return m_shuffle; }
    void setShuffle(bool s);
    bool canNext() const;
    bool canPrevious() const;
    qint64 loopA() const { return m_loopA; }
    qint64 loopB() const { return m_loopB; }
    QVariantList audioTracks() const;
    QVariantList subtitleTracks() const;
    int audioTrack() const { return m_engine->currentAudioTrack(); }
    int subtitleTrack() const { return m_engine->currentSubtitleTrack(); }
    QVariantList externalSubtitles() const { return m_externalSubs; }
    QVariantList chapters() const;
    int currentChapter() const { return m_engine->currentChapter(); }
    qint64 subtitleDelayMs() const { return m_engine->subtitleDelayMs(); }
    qint64 audioDelayMs() const { return m_engine->audioDelayMs(); }
    QString aspectMode() const { return m_aspect; }
    void setAspectMode(const QString &m);
    double zoom() const { return m_zoom; }
    void setZoom(double z);
    int videoRotation() const { return m_rotation; }
    void setVideoRotation(int r);
    QVariantMap adjust() const { return m_adjust; }
    QString deinterlace() const { return m_deinterlace; }
    void setDeinterlace(const QString &m);
    QVariantMap resumePrompt() const { return m_resumePrompt; }
    QVariantMap details() const { return m_details; }
    bool eqEnabled() const { return m_eqEnabled; }
    int eqPreset() const { return m_eqPreset; }
    double eqPreamp() const { return m_eqPreamp; }
    QVariantList eqBands() const;
    QVariantList bookmarks() const { return m_bookmarks; }
    QString engineVersion() const { return VlcEngine::versionString(); }

    // ---------------------------------------------------------------- opening media
    /// Replaces the queue with these locations (paths, file:// or network URLs) and starts at `startIndex`.
    Q_INVOKABLE bool open(const QVariantList &locations, int startIndex = 0);
    Q_INVOKABLE bool openFile(const QString &pathOrUrl) { return open({pathOrUrl}, 0); }
    /// Opens a stream; credentials (if given) are used for this session only and never stored or logged.
    Q_INVOKABLE bool openStream(const QString &url, const QString &user = QString(), const QString &password = QString());
    /// Replaces the queue with library items and plays `startIndex`.
    Q_INVOKABLE bool openMediaIds(const QVariantList &ids, int startIndex = 0);
    Q_INVOKABLE bool openFolder(const QString &folder);                  // every media file inside, natural order
    Q_INVOKABLE bool openPlaylistFile(const QString &path);              // M3U / M3U8
    Q_INVOKABLE int enqueue(const QVariantList &locations, bool playNext = false);   // returns number added
    Q_INVOKABLE int enqueueMediaIds(const QVariantList &ids, bool playNext = false);
    Q_INVOKABLE bool saveQueueAsPlaylistFile(const QString &path);
    Q_INVOKABLE bool playIndex(int index);
    Q_INVOKABLE void closeMedia();                                       // stop and return to "nothing loaded"

    // ---------------------------------------------------------------- transport
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void restart();
    Q_INVOKABLE bool next();
    Q_INVOKABLE bool previous();
    Q_INVOKABLE void seekTo(qint64 ms);
    Q_INVOKABLE void seekFraction(double f);
    Q_INVOKABLE void seekRelative(qint64 deltaMs);
    Q_INVOKABLE void seekSmall(int direction) { seekRelative(direction * smallStepMs()); }
    Q_INVOKABLE void seekLarge(int direction) { seekRelative(direction * largeStepMs()); }
    Q_INVOKABLE void frameStep();
    Q_INVOKABLE void volumeStep(int delta);
    Q_INVOKABLE void toggleMute() { setMuted(!muted()); }
    Q_INVOKABLE void speedStep(int direction);                           // walks the speed table
    Q_INVOKABLE void resetSpeed() { setRate(1.0); }
    Q_INVOKABLE QVariantList speedChoices() const;
    Q_INVOKABLE void nextChapter();
    Q_INVOKABLE void previousChapter();
    Q_INVOKABLE void setChapter(int index);
    Q_INVOKABLE void cycleRepeat();

    // ---------------------------------------------------------------- A-B loop
    Q_INVOKABLE void cycleAbLoop();              // set A -> set B -> clear
    Q_INVOKABLE void setLoop(qint64 a, qint64 b);
    Q_INVOKABLE void clearLoop();

    // ---------------------------------------------------------------- resume prompt
    Q_INVOKABLE void acceptResume();
    Q_INVOKABLE void declineResume();

    // ---------------------------------------------------------------- audio
    Q_INVOKABLE void setAudioTrack(int id);
    Q_INVOKABLE void cycleAudioTrack();
    Q_INVOKABLE void setAudioDelayMs(qint64 ms);
    Q_INVOKABLE void adjustAudioDelay(qint64 deltaMs) { setAudioDelayMs(audioDelayMs() + deltaMs); }
    Q_INVOKABLE QVariantList audioDevices() const;
    Q_INVOKABLE bool setAudioDevice(const QString &id);
    Q_INVOKABLE bool setStereoMode(const QString &mode);   // default|stereo|reverse|left|right|dolby
    Q_INVOKABLE QStringList eqPresetNames() const { return VlcEngine::equalizerPresetNames(); }
    Q_INVOKABLE QVariantList eqFrequencies() const;
    Q_INVOKABLE void setEqEnabled(bool on);
    Q_INVOKABLE void applyEqPreset(int index);
    Q_INVOKABLE void setEqPreamp(double db);
    Q_INVOKABLE void setEqBand(int index, double db);
    Q_INVOKABLE QStringList eqProfileNames() const;
    Q_INVOKABLE bool saveEqProfile(const QString &name);
    Q_INVOKABLE bool loadEqProfile(const QString &name);
    Q_INVOKABLE bool deleteEqProfile(const QString &name);

    // ---------------------------------------------------------------- subtitles
    Q_INVOKABLE void setSubtitleTrack(int id);             // -1 = off
    Q_INVOKABLE void cycleSubtitleTrack();
    Q_INVOKABLE bool loadSubtitleFile(const QString &pathOrUrl);
    Q_INVOKABLE void setSubtitleDelayMs(qint64 ms);
    Q_INVOKABLE void adjustSubtitleDelay(qint64 deltaMs) { setSubtitleDelayMs(subtitleDelayMs() + deltaMs); }
    /// Subtitle style is applied through libVLC text-renderer options, which libVLC reads when the media
    /// starts: applying re-opens the current media at the current position.
    Q_INVOKABLE void applySubtitleStyle();

    // ---------------------------------------------------------------- video
    Q_INVOKABLE void setAdjustValue(const QString &name, double value);  // brightness|contrast|saturation|gamma|hue
    Q_INVOKABLE void resetAdjust();
    Q_INVOKABLE void resetVideoLayout();

    // ---------------------------------------------------------------- bookmarks / screenshots / misc
    Q_INVOKABLE void addBookmark(const QString &note = QString());
    Q_INVOKABLE void removeBookmark(qint64 id);
    Q_INVOKABLE void updateBookmark(qint64 id, const QString &note);
    Q_INVOKABLE void seekToBookmark(qint64 id);
    /// Saves the current decoded frame (exactly what is on screen, without UI overlays). Returns the path or "".
    Q_INVOKABLE QString takeScreenshot();
    Q_INVOKABLE bool copyImageToClipboard(const QString &path);
    Q_INVOKABLE void savePosition();                                      // flush progress now (pause, stop, quit)
    Q_INVOKABLE void shutdown();                                          // stop playback and persist everything
    Q_INVOKABLE QVariantList mediaInfoRows() const;                       // for the media-information dialog

    qint64 smallStepMs() const;
    qint64 largeStepMs() const;

    // test hooks / helpers (also useful for the UI)
    static QString screenshotFileName(const QString &title, qint64 positionMs, const QString &ext);

signals:
    void stateChanged();
    void positionChanged();
    void durationChanged();
    void seekableChanged();
    void volumeChanged();
    void rateChanged();
    void bufferingChanged();
    void mediaChanged();
    void errorChanged();
    void modeChanged();
    void queueStateChanged();
    void loopChanged();
    void tracksChanged();
    void chaptersChanged();
    void delaysChanged();
    void videoLayoutChanged();
    void adjustChanged();
    void resumePromptChanged();
    void detailsChanged();
    void eqChanged();
    void bookmarksChanged();
    void screenshotSaved(const QString &path);
    void message(const QString &text, const QString &level);
    void playbackEnded();                       // queue finished, nothing more to play

private slots:
    void onState(lumina::VlcEngine::State s);
    void onTime(qint64 ms);
    void onLength(qint64 ms);
    void onEnd();
    void onError(const QString &msg);
    void onTracks();
    void checkLoop();

private:
    bool startEntry(int index, bool autoAdvance);
    void applyMediaPrefs();
    void saveMediaPrefs();
    void scheduleSavePrefs();
    void discoverSubtitles();
    void refreshChapters();
    void refreshBookmarks();
    void setState();
    QStringList mediaOptionsFor(const QUrl &url) const;
    QStringList subtitleStyleArgs() const;
    void reapplyEngineState();
    QVector<QueueEntry> entriesFor(const QVariantList &locations) const;
    QVector<QueueEntry> entriesForIds(const QVariantList &ids) const;
    int pickNext(bool forward);
    void applyEq();
    void persistEq();
    void setError(const QString &e);

    AppSettings *m_settings;
    LibraryManager *m_library;
    VlcEngine *m_engine;
    QueueModel m_queue;

    QTimer m_saveTimer;        // periodic progress save (coarse, only while playing)
    QTimer m_prefsTimer;       // debounced per-media preference save
    QTimer m_loopTimer;        // fine-grained A-B loop watcher (runs only when a loop is active)

    QUrl m_url;
    QString m_title, m_error;
    qint64 m_mediaId = 0;
    bool m_isStream = false;
    qint64 m_positionMs = 0, m_durationMs = 0;
    qint64 m_lastSavedMs = -100000;
    double m_buffering = 100.0;
    QString m_repeat = QStringLiteral("off");
    bool m_shuffle = false;
    QVector<int> m_shuffleHistory;
    QSet<int> m_shufflePlayed;
    qint64 m_loopA = -1, m_loopB = -1;
    QVariantMap m_resumePrompt;
    qint64 m_pendingSeekMs = -1;
    bool m_awaitResumeChoice = false;
    bool m_autoAdvancing = false;
    QVariantList m_externalSubs;
    QVariantList m_bookmarks;
    QVariantList m_chapters;
    QVariantMap m_details;
    QVariantMap m_adjust;
    QString m_aspect = QStringLiteral("auto");
    double m_zoom = 1.0;
    int m_rotation = 0;
    QString m_deinterlace;
    bool m_eqEnabled = false;
    int m_eqPreset = -1;
    double m_eqPreamp = 0.0;
    QVector<float> m_eqBands;
    QVariantMap m_prefsLoaded;
    bool m_tracksRestored = false;
    qint64 m_prefSubtitle = -2;      // -2 = no stored preference
    bool m_subtitleChosenByUser = false;
    QString m_userSubtitleFile;
    QString m_stereoMode = QStringLiteral("default");
    quint64 m_loadSerial = 0;
};

} // namespace lumina
