#pragma once

#include <QtGlobal>

namespace lumina::resume {

constexpr qint64 kMinResumeMs = 5000;       // watched less than this: just start over
constexpr double kCompletedFraction = 0.95; // beyond this fraction the item counts as finished
constexpr qint64 kCompletedTailMs = 20000;  // ...or within this many ms of the end

inline bool isCompleted(qint64 positionMs, qint64 durationMs)
{
    if (durationMs <= 0)
        return false;
    return positionMs >= durationMs * kCompletedFraction || positionMs >= durationMs - kCompletedTailMs;
}

/// A saved position is only worth offering if it is meaningful and not at the very end.
inline bool canResume(qint64 positionMs, qint64 durationMs)
{
    if (positionMs < kMinResumeMs)
        return false;
    if (durationMs > 0 && isCompleted(positionMs, durationMs))
        return false;
    return true;
}

} // namespace lumina::resume
