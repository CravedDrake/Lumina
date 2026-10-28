#include "MediaProbe.h"

#include <vlc/vlc.h>

#include <QDir>
#include <QString>

#include <condition_variable>
#include <mutex>

namespace lumina {

namespace {
struct Waiter {
    std::mutex m;
    std::condition_variable cv;
    bool done = false;
};

void onParsed(const libvlc_event_t *, void *data)
{
    auto *w = static_cast<Waiter *>(data);
    std::lock_guard<std::mutex> lock(w->m);
    w->done = true;
    w->cv.notify_all();
}

QString fourcc(uint32_t cc)
{
    char s[5] = {char(cc & 0xff), char((cc >> 8) & 0xff), char((cc >> 16) & 0xff), char((cc >> 24) & 0xff), 0};
    return QString::fromLatin1(s).trimmed();
}
} // namespace

MediaProbe::MediaProbe()
{
    const char *args[] = {"--no-video", "--no-audio", "--no-xlib", "--quiet", "--no-stats", "--no-osd"};
    m_instance = libvlc_new(int(sizeof(args) / sizeof(args[0])), args);
}

MediaProbe::~MediaProbe()
{
    if (m_instance)
        libvlc_release(m_instance);
}

ProbeResult MediaProbe::probe(const QString &path, int timeoutMs)
{
    ProbeResult r;
    if (!m_instance)
        return r;
    libvlc_media_t *media = libvlc_media_new_path(m_instance, QDir::toNativeSeparators(path).toUtf8().constData());
    if (!media)
        return r;

    Waiter waiter;
    libvlc_event_manager_t *em = libvlc_media_event_manager(media);
    libvlc_event_attach(em, libvlc_MediaParsedChanged, &onParsed, &waiter);
    if (libvlc_media_parse_with_options(media, libvlc_media_parse_local, timeoutMs) == 0) {
        std::unique_lock<std::mutex> lock(waiter.m);
        waiter.cv.wait_for(lock, std::chrono::milliseconds(timeoutMs + 2000), [&] { return waiter.done; });
    }
    libvlc_event_detach(em, libvlc_MediaParsedChanged, &onParsed, &waiter);

    if (libvlc_media_get_parsed_status(media) == libvlc_media_parsed_status_done) {
        r.durationMs = libvlc_media_get_duration(media);
        libvlc_media_track_t **tracks = nullptr;
        const unsigned n = libvlc_media_tracks_get(media, &tracks);
        for (unsigned i = 0; i < n; ++i) {
            const auto *t = tracks[i];
            if (t->i_type == libvlc_track_video && r.videoCodec.isEmpty()) {
                r.videoCodec = fourcc(t->i_codec);
                r.width = int(t->video->i_width);
                r.height = int(t->video->i_height);
                if (t->video->i_frame_rate_den)
                    r.fps = double(t->video->i_frame_rate_num) / double(t->video->i_frame_rate_den);
                r.videoBitrate = int(t->i_bitrate);
            } else if (t->i_type == libvlc_track_audio) {
                if (r.audioCodec.isEmpty()) {
                    r.audioCodec = fourcc(t->i_codec);
                    r.channels = int(t->audio->i_channels);
                    r.sampleRate = int(t->audio->i_rate);
                    r.audioBitrate = int(t->i_bitrate);
                }
                ++r.audioTracks;
            } else if (t->i_type == libvlc_track_text) {
                ++r.subtitleTracks;
            }
        }
        if (tracks)
            libvlc_media_tracks_release(tracks, n);
        r.ok = n > 0 || r.durationMs > 0;
    }
    libvlc_media_release(media);
    return r;
}

} // namespace lumina
