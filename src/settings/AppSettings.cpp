#include "AppSettings.h"

#include <QMetaType>

namespace lumina {

const QVariantMap &AppSettings::defaults()
{
    static const QVariantMap d = {
        // Appearance
        {QStringLiteral("appearance/accent"), QStringLiteral("#7C5CFC")},
        {QStringLiteral("appearance/density"), QStringLiteral("comfortable")},   // comfortable | compact
        {QStringLiteral("appearance/animations"), true},
        {QStringLiteral("appearance/sidebarCollapsed"), false},
        {QStringLiteral("appearance/sidebarAutoCollapse"), false},
        {QStringLiteral("appearance/viewMode"), QStringLiteral("grid")},          // grid | list
        // Playback
        {QStringLiteral("playback/defaultSpeed"), 1.0},
        {QStringLiteral("playback/seekSmall"), 10},
        {QStringLiteral("playback/seekLarge"), 30},
        {QStringLiteral("playback/autoPlayNext"), true},
        {QStringLiteral("playback/resume"), QStringLiteral("ask")},               // ask | always | never
        {QStringLiteral("playback/autoHideMs"), 2800},                            // 0 = never hide
        {QStringLiteral("playback/defaultAspect"), QStringLiteral("auto")},
        {QStringLiteral("playback/rememberPerMedia"), true},
        // Audio
        {QStringLiteral("audio/volume"), 80},
        {QStringLiteral("audio/muted"), false},
        {QStringLiteral("audio/device"), QString()},
        {QStringLiteral("audio/delayMs"), 0},
        {QStringLiteral("audio/eqEnabled"), false},
        {QStringLiteral("audio/eqPreset"), -1},
        {QStringLiteral("audio/eqPreamp"), 0.0},
        {QStringLiteral("audio/eqBands"), QVariantList()},
        {QStringLiteral("audio/eqProfiles"), QVariantMap()},
        // Subtitles (style options are applied by libVLC's text renderer when media is (re)loaded)
        {QStringLiteral("subtitles/languages"), QStringList{QStringLiteral("eng")}},
        {QStringLiteral("subtitles/autoEnable"), true},
        {QStringLiteral("subtitles/font"), QString()},
        {QStringLiteral("subtitles/size"), 18},                                    // libVLC relative font size (px/1000 of height)
        {QStringLiteral("subtitles/color"), QStringLiteral("#FFFFFF")},
        {QStringLiteral("subtitles/bold"), false},
        {QStringLiteral("subtitles/outline"), 2},                                  // 0 none,1 thin,2 normal,3 thick
        {QStringLiteral("subtitles/shadow"), true},
        {QStringLiteral("subtitles/bgOpacity"), 0},                                // 0..255
        {QStringLiteral("subtitles/marginBottom"), 10},
        {QStringLiteral("subtitles/delayStep"), 100},
        // Library
        {QStringLiteral("library/autoScan"), true},
        {QStringLiteral("library/excluded"), QStringList()},
        {QStringLiteral("library/removeMissing"), false},
        {QStringLiteral("library/localArtwork"), true},
        // Keyboard & mouse
        {QStringLiteral("keyboard/overrides"), QStringLiteral("{}")},          // JSON: actionId -> [sequences]
        {QStringLiteral("mouse/wheelAction"), QStringLiteral("volume")},         // volume | seek | none
        {QStringLiteral("mouse/gestures"), true},
        {QStringLiteral("mouse/doubleClickFullscreen"), true},
        {QStringLiteral("mouse/clickPause"), true},
        // Performance
        {QStringLiteral("performance/hwDecoding"), QStringLiteral("auto")},      // auto | none
        {QStringLiteral("performance/networkCacheMs"), 1500},
        {QStringLiteral("performance/thumbnails"), true},
        {QStringLiteral("performance/thumbCacheMB"), 256},
        {QStringLiteral("performance/seekPreview"), false},                         // frame preview when hovering the seek bar (local files)
        {QStringLiteral("performance/scanThreads"), 2},
        {QStringLiteral("performance/verboseEngineLog"), false},
        // Privacy
        {QStringLiteral("privacy/historyEnabled"), true},
        {QStringLiteral("privacy/historyDays"), 0},                                // 0 = keep forever
        {QStringLiteral("privacy/recentStreams"), 20},                              // how many stream URLs to remember
        {QStringLiteral("privacy/streamHistory"), QStringList()},                  // remembered URLs (never with credentials)
        // Screenshots
        {QStringLiteral("screenshots/dir"), QString()},
        {QStringLiteral("screenshots/format"), QStringLiteral("png")},
        {QStringLiteral("screenshots/clipboard"), false},
        {QStringLiteral("playback/repeat"), QStringLiteral("off")},
        {QStringLiteral("playback/shuffle"), false},
    };
    return d;
}

AppSettings::AppSettings(QObject *parent)
    : QObject(parent), m_settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Lumina"), QStringLiteral("Lumina"))
{
    load();
}

AppSettings::AppSettings(const QString &iniPath, QObject *parent)
    : QObject(parent), m_settings(iniPath, QSettings::IniFormat)
{
    load();
}

void AppSettings::load()
{
    m_values = defaults();
    const QVariantMap &d = defaults();
    for (auto it = d.constBegin(); it != d.constEnd(); ++it) {
        if (!m_settings.contains(it.key()))
            continue;
        QVariant v = m_settings.value(it.key());
        if (v.canConvert(it.value().metaType()) && v.convert(it.value().metaType()))
            m_values[it.key()] = v;
    }
}

QVariant AppSettings::get(const QString &key) const { return m_values.value(key); }

bool AppSettings::set(const QString &key, const QVariant &value)
{
    const QVariantMap &d = defaults();
    auto it = d.constFind(key);
    if (it == d.constEnd())
        return false;                       // unknown key: reject
    QVariant v = value;
    if (!v.convert(it.value().metaType()))
        return false;                       // wrong type: reject
    if (m_values.value(key) == v)
        return true;
    m_values[key] = v;
    m_settings.setValue(key, v);
    emit changed(key);
    emit valuesChanged();
    return true;
}

void AppSettings::reset(const QString &key)
{
    if (!defaults().contains(key))
        return;
    m_settings.remove(key);
    m_values[key] = defaults().value(key);
    emit changed(key);
    emit valuesChanged();
}

void AppSettings::resetAll()
{
    for (auto it = defaults().constBegin(); it != defaults().constEnd(); ++it)
        m_settings.remove(it.key());
    m_values = defaults();
    emit valuesChanged();
    emit changed(QString());
}

QVariantMap AppSettings::windowState() const
{
    if (!m_settings.contains(QStringLiteral("window/width")))
        return {};
    return {{QStringLiteral("x"), m_settings.value(QStringLiteral("window/x"))},
            {QStringLiteral("y"), m_settings.value(QStringLiteral("window/y"))},
            {QStringLiteral("width"), m_settings.value(QStringLiteral("window/width"))},
            {QStringLiteral("height"), m_settings.value(QStringLiteral("window/height"))},
            {QStringLiteral("maximized"), m_settings.value(QStringLiteral("window/maximized"), false)}};
}

void AppSettings::saveWindowState(int x, int y, int width, int height, bool maximized)
{
    m_settings.setValue(QStringLiteral("window/maximized"), maximized);
    if (!maximized) {   // keep the restored geometry, not the maximised one
        m_settings.setValue(QStringLiteral("window/x"), x);
        m_settings.setValue(QStringLiteral("window/y"), y);
        m_settings.setValue(QStringLiteral("window/width"), width);
        m_settings.setValue(QStringLiteral("window/height"), height);
    }
}

} // namespace lumina
