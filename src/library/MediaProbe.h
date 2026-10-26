#pragma once

#include <QString>
#include <memory>

struct libvlc_instance_t;

namespace lumina {

struct ProbeResult {
    bool ok = false;
    qint64 durationMs = 0;
    int width = 0, height = 0;
    double fps = 0;
    QString videoCodec, audioCodec;
    int audioTracks = 0, subtitleTracks = 0;
    int channels = 0, sampleRate = 0, videoBitrate = 0, audioBitrate = 0;
};

/// Reads container/stream metadata with libVLC's parser. One probe owns one (headless) libVLC
/// instance, so use one probe per thread; probe() itself is not re-entrant.
class MediaProbe {
public:
    MediaProbe();
    ~MediaProbe();
    bool isValid() const { return m_instance != nullptr; }
    ProbeResult probe(const QString &path, int timeoutMs = 10000);

private:
    libvlc_instance_t *m_instance = nullptr;
};

} // namespace lumina
