#pragma once
#include <QHash>
#include <QKeyCombination>
#include <QObject>
#include <QVariantList>

namespace lumina {

class AppSettings;

// Customisable keyboard shortcuts.
//  * Installed as an application-level event filter on the main window, so shortcuts work
//    regardless of which QML item has focus — except while a text field has focus or a modal
//    dialog sets `blocked`.
//  * Player-scope actions only fire while `playerActive` is true; global actions always fire.
class ShortcutManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool playerActive READ playerActive WRITE setPlayerActive NOTIFY playerActiveChanged)
    Q_PROPERTY(bool blocked READ blocked WRITE setBlocked NOTIFY blockedChanged)
public:
    struct Action {
        QString id, title, category;
        QStringList defaults;
        bool repeatable = false;
        bool global = false;
    };

    explicit ShortcutManager(AppSettings *settings, QObject *parent = nullptr);

    static const QList<Action> &actions();

    bool playerActive() const { return m_playerActive; }
    void setPlayerActive(bool v);
    bool blocked() const { return m_blocked; }
    void setBlocked(bool v);

    Q_INVOKABLE QVariantList actionList() const;                 // [{id,title,category,keys,defaults,customized}]
    Q_INVOKABLE QString keysText(const QString &actionId) const; // human readable first binding, "" if none
    // Returns "" on success, "invalid" for unparsable input, otherwise the title of the conflicting action.
    Q_INVOKABLE QString setBinding(const QString &actionId, const QString &sequence);
    Q_INVOKABLE QString conflictFor(const QString &sequence, const QString &exceptActionId = QString()) const;
    Q_INVOKABLE void resetBinding(const QString &actionId);
    Q_INVOKABLE void resetAll();
    Q_INVOKABLE QString sequenceFromKey(int key, int modifiers) const;   // for the "press a key" capture UI

    // Pure lookup (also used by tests): which action would this key press trigger?
    QString actionFor(QKeyCombination kc) const;
    // Find duplicate default bindings (should be empty; used by tests).
    static QStringList defaultConflicts();

    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void triggered(const QString &actionId);
    void bindingsChanged();
    void playerActiveChanged();
    void blockedChanged();

private:
    void rebuild();
    void persist();
    static QString normalize(const QString &seq);
    static const Action *find(const QString &id);

    AppSettings *m_settings;
    QHash<QString, QStringList> m_bindings;   // actionId -> normalised sequences
    QHash<QString, QString> m_lookup;         // normalised sequence -> actionId
    bool m_playerActive = false;
    bool m_blocked = false;
};

} // namespace lumina
