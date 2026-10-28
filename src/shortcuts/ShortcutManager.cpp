#include "ShortcutManager.h"
#include "../settings/AppSettings.h"
#include "../utilities/Logger.h"
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QKeyEvent>
#include <QKeySequence>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSet>
#include <QJsonValue>
#include <QHash>
#include <QList>
#include <QStringList>
#include <QObject>
#include <QString>

namespace lumina {

namespace {
ShortcutManager::Action A(const char *id, const char *title, const char *cat, std::initializer_list<const char *> keyList, bool repeat = false, bool global = false)
{
    QStringList keys;
    for (const char *k : keyList) keys << QString::fromLatin1(k);
    ShortcutManager::Action a;
    a.id = QString::fromLatin1(id); a.title = QString::fromLatin1(title); a.category = QString::fromLatin1(cat);
    a.defaults = keys; a.repeatable = repeat; a.global = global;
    return a;
}
} // namespace

const QList<ShortcutManager::Action> &ShortcutManager::actions()
{
    static const QList<Action> list = {
        // Playback
        A("playPause", "Play / Pause", "Playback", {"Space"}),
        A("stop", "Stop", "Playback", {"X"}),
        A("restart", "Restart", "Playback", {"R"}),
        A("seekBack", "Seek backward", "Playback", {"Left"}, true),
        A("seekForward", "Seek forward", "Playback", {"Right"}, true),
        A("seekBackLarge", "Seek backward (large step)", "Playback", {"Shift+Left"}, true),
        A("seekForwardLarge", "Seek forward (large step)", "Playback", {"Shift+Right"}, true),
        A("frameStep", "Step one frame", "Playback", {"E"}, true),
        A("abLoop", "A–B loop (set A, set B, clear)", "Playback", {"L"}),
        A("next", "Next item", "Playback", {"N"}),
        A("previous", "Previous item", "Playback", {"P"}),
        A("chapterNext", "Next chapter", "Playback", {"PgDown"}),
        A("chapterPrevious", "Previous chapter", "Playback", {"PgUp"}),
        A("speedUp", "Faster", "Playback", {"]"}, true),
        A("speedDown", "Slower", "Playback", {"["}, true),
        A("speedReset", "Normal speed", "Playback", {"Backspace"}),
        A("goToTime", "Go to time…", "Playback", {"G"}),
        A("seekPercent0", "Jump to 0%", "Playback", {"0"}),
        A("seekPercent1", "Jump to 10%", "Playback", {"1"}),
        A("seekPercent2", "Jump to 20%", "Playback", {"2"}),
        A("seekPercent3", "Jump to 30%", "Playback", {"3"}),
        A("seekPercent4", "Jump to 40%", "Playback", {"4"}),
        A("seekPercent5", "Jump to 50%", "Playback", {"5"}),
        A("seekPercent6", "Jump to 60%", "Playback", {"6"}),
        A("seekPercent7", "Jump to 70%", "Playback", {"7"}),
        A("seekPercent8", "Jump to 80%", "Playback", {"8"}),
        A("seekPercent9", "Jump to 90%", "Playback", {"9"}),
        // Audio & video
        A("volumeUp", "Volume up", "Audio", {"Up"}, true),
        A("volumeDown", "Volume down", "Audio", {"Down"}, true),
        A("mute", "Mute", "Audio", {"M"}),
        A("cycleAudio", "Next audio track", "Audio", {"A"}),
        A("audioDelayMinus", "Audio delay −", "Audio", {"Ctrl+J"}, true),
        A("audioDelayPlus", "Audio delay +", "Audio", {"Ctrl+K"}, true),
        A("cycleSubtitle", "Next subtitle track", "Subtitles", {"S"}),
        A("subDelayMinus", "Subtitles earlier", "Subtitles", {"J"}, true),
        A("subDelayPlus", "Subtitles later", "Subtitles", {"K"}, true),
        A("fullscreen", "Fullscreen", "Window", {"F", "Alt+Return"}),
        A("escape", "Exit fullscreen / close overlay", "Window", {"Escape"}),
        A("alwaysOnTop", "Always on top", "Window", {"T"}),
        A("pip", "Picture-in-picture", "Window", {"Ctrl+P"}),
        A("screenshot", "Take screenshot", "Tools", {"C"}),
        A("mediaInfo", "Media information", "Tools", {"I"}),
        A("addBookmark", "Add bookmark", "Tools", {"B"}),
        A("toggleQueue", "Show / hide queue", "Panels", {"Q"}),
        A("toggleSubtitlePanel", "Subtitle panel", "Panels", {"Shift+S"}),
        A("toggleAudioPanel", "Audio panel", "Panels", {"Shift+A"}),
        A("toggleVideoPanel", "Video adjustments panel", "Panels", {"Shift+V"}),
        A("toggleBookmarkPanel", "Bookmarks panel", "Panels", {"Shift+B"}),
        // Global
        A("openMenu", "Open…", "Application", {"O"}, false, true),
        A("openFile", "Open file…", "Application", {"Ctrl+O"}, false, true),
        A("openFolder", "Open folder…", "Application", {"Ctrl+Shift+O"}, false, true),
        A("openUrl", "Open network stream…", "Application", {"Ctrl+U"}, false, true),
        A("search", "Search library", "Application", {"Ctrl+L", "Ctrl+F"}, false, true),
        A("toggleSidebar", "Show / hide sidebar", "Application", {"Ctrl+B"}, false, true),
        A("openSettings", "Settings", "Application", {"Ctrl+,"}, false, true),
        A("quit", "Quit", "Application", {"Ctrl+Q"}, false, true),
    };
    return list;
}

const ShortcutManager::Action *ShortcutManager::find(const QString &id)
{
    for (const Action &a : actions()) if (a.id == id) return &a;
    return nullptr;
}

QString ShortcutManager::normalize(const QString &seq)
{
    const QKeySequence ks(seq.trimmed(), QKeySequence::PortableText);
    if (ks.isEmpty() || ks.count() != 1) return {};
    return ks.toString(QKeySequence::PortableText);
}

ShortcutManager::ShortcutManager(AppSettings *settings, QObject *parent) : QObject(parent), m_settings(settings)
{
    rebuild();
}

void ShortcutManager::rebuild()
{
    m_bindings.clear();
    m_lookup.clear();
    QJsonObject overrides;
    if (m_settings) {
        const auto doc = QJsonDocument::fromJson(m_settings->get(QStringLiteral("keyboard/overrides")).toString().toUtf8());
        if (doc.isObject()) overrides = doc.object();
    }
    for (const Action &a : actions()) {
        QStringList keys;
        if (overrides.contains(a.id)) {
            for (const QJsonValue &v : overrides.value(a.id).toArray()) {
                const QString n = normalize(v.toString());
                if (!n.isEmpty()) keys << n;
            }
        } else {
            for (const QString &k : a.defaults) { const QString n = normalize(k); if (!n.isEmpty()) keys << n; }
        }
        m_bindings.insert(a.id, keys);
        for (const QString &k : keys)
            if (!m_lookup.contains(k)) m_lookup.insert(k, a.id);     // first definition wins if the user file is inconsistent
    }
}

void ShortcutManager::persist()
{
    if (!m_settings) return;
    QJsonObject o;
    for (const Action &a : actions()) {
        QStringList def;
        for (const QString &k : a.defaults) def << normalize(k);
        if (m_bindings.value(a.id) != def) {
            QJsonArray arr;
            for (const QString &k : m_bindings.value(a.id)) arr << k;
            o.insert(a.id, arr);
        }
    }
    m_settings->set(QStringLiteral("keyboard/overrides"), QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
}

QVariantList ShortcutManager::actionList() const
{
    QVariantList out;
    for (const Action &a : actions()) {
        QStringList def;
        for (const QString &k : a.defaults) def << normalize(k);
        QVariantMap m;
        m[QStringLiteral("id")] = a.id; m[QStringLiteral("title")] = a.title; m[QStringLiteral("category")] = a.category;
        m[QStringLiteral("keys")] = m_bindings.value(a.id);
        m[QStringLiteral("defaults")] = def;
        m[QStringLiteral("customized")] = m_bindings.value(a.id) != def;
        out << m;
    }
    return out;
}

QString ShortcutManager::keysText(const QString &id) const
{
    const QStringList k = m_bindings.value(id);
    return k.isEmpty() ? QString() : QKeySequence(k.first(), QKeySequence::PortableText).toString(QKeySequence::NativeText);
}

QString ShortcutManager::conflictFor(const QString &sequence, const QString &except) const
{
    const QString n = normalize(sequence);
    if (n.isEmpty()) return QStringLiteral("invalid");
    for (auto it = m_bindings.cbegin(); it != m_bindings.cend(); ++it) {
        if (it.key() == except) continue;
        if (it.value().contains(n)) {
            const Action *a = find(it.key());
            return a ? a->title : it.key();
        }
    }
    return {};
}

QString ShortcutManager::setBinding(const QString &id, const QString &sequence)
{
    if (!find(id)) return QStringLiteral("invalid");
    const QString n = normalize(sequence);
    if (n.isEmpty()) return QStringLiteral("invalid");
    const QString c = conflictFor(n, id);
    if (!c.isEmpty()) return c;
    m_bindings[id] = QStringList{n};
    m_lookup.clear();
    for (auto it = m_bindings.cbegin(); it != m_bindings.cend(); ++it)
        for (const QString &k : it.value()) if (!m_lookup.contains(k)) m_lookup.insert(k, it.key());
    persist();
    emit bindingsChanged();
    return {};
}

void ShortcutManager::resetBinding(const QString &id)
{
    const Action *a = find(id);
    if (!a) return;
    QStringList keys;
    for (const QString &k : a->defaults) keys << normalize(k);
    // taking the defaults back must not steal a key that the user has since bound elsewhere
    for (const QString &k : keys) {
        const QString owner = m_lookup.value(k);
        if (!owner.isEmpty() && owner != id) {
            QStringList ok = m_bindings.value(owner);
            ok.removeAll(k);
            m_bindings[owner] = ok;
        }
    }
    m_bindings[id] = keys;
    m_lookup.clear();
    for (auto it = m_bindings.cbegin(); it != m_bindings.cend(); ++it)
        for (const QString &k : it.value()) if (!m_lookup.contains(k)) m_lookup.insert(k, it.key());
    persist();
    emit bindingsChanged();
}

void ShortcutManager::resetAll()
{
    if (m_settings) m_settings->set(QStringLiteral("keyboard/overrides"), QStringLiteral("{}"));
    rebuild();
    emit bindingsChanged();
}

QString ShortcutManager::sequenceFromKey(int key, int mods) const
{
    switch (key) {
    case Qt::Key_Shift: case Qt::Key_Control: case Qt::Key_Alt: case Qt::Key_Meta: case Qt::Key_AltGr: case 0:
        return {};
    }
    return QKeySequence(QKeyCombination(Qt::KeyboardModifiers(mods) & ~Qt::KeypadModifier, Qt::Key(key))).toString(QKeySequence::PortableText);
}

QString ShortcutManager::actionFor(QKeyCombination kc) const
{
    const Qt::KeyboardModifiers mods = kc.keyboardModifiers() & ~Qt::KeypadModifier;
    const Qt::Key key = kc.key();
    QString s = QKeySequence(QKeyCombination(mods, key)).toString(QKeySequence::PortableText);
    auto it = m_lookup.constFind(s);
    if (it != m_lookup.constEnd()) return it.value();
    // symbols typed with Shift (e.g. "?" = Shift+/) are reported with the Shift flag; try without it
    const bool isLetter = key >= Qt::Key_A && key <= Qt::Key_Z;
    if (!isLetter && (mods & Qt::ShiftModifier)) {
        s = QKeySequence(QKeyCombination(mods & ~Qt::ShiftModifier, key)).toString(QKeySequence::PortableText);
        it = m_lookup.constFind(s);
        if (it != m_lookup.constEnd()) return it.value();
    }
    return {};
}

QStringList ShortcutManager::defaultConflicts()
{
    QHash<QString, QString> seen;
    QStringList out;
    for (const Action &a : actions())
        for (const QString &k : a.defaults) {
            const QString n = normalize(k);
            if (n.isEmpty()) { out << QStringLiteral("%1: unparsable default '%2'").arg(a.id, k); continue; }
            if (seen.contains(n)) out << QStringLiteral("%1 and %2 both use %3").arg(seen.value(n), a.id, n);
            else seen.insert(n, a.id);
        }
    return out;
}

void ShortcutManager::setPlayerActive(bool v) { if (v != m_playerActive) { m_playerActive = v; emit playerActiveChanged(); } }
void ShortcutManager::setBlocked(bool v) { if (v != m_blocked) { m_blocked = v; emit blockedChanged(); } }

bool ShortcutManager::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::KeyPress) return QObject::eventFilter(watched, event);
    if (m_blocked) return false;
    auto *ke = static_cast<QKeyEvent *>(event);

    if (auto *win = qobject_cast<QQuickWindow *>(watched)) {
        if (QQuickItem *fi = win->activeFocusItem())
            if (fi->flags() & QQuickItem::ItemAcceptsInputMethod) return false;   // typing into a text field
    } else {
        return false;
    }
    const QString id = actionFor(ke->keyCombination());
    if (id.isEmpty()) return false;
    const Action *a = find(id);
    if (!a) return false;
    if (!a->global && !m_playerActive) return false;
    if (ke->isAutoRepeat() && !a->repeatable) return true;
    emit triggered(id);
    return true;
}

} // namespace lumina
