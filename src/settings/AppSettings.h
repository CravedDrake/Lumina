#pragma once

#include <QObject>
#include <QSettings>
#include <QVariantMap>

namespace lumina {

/**
 * Typed, defaulted settings backed by QSettings.
 *
 * QML reads `Settings.values["group/key"]`: the whole map is one notifying property, so any binding
 * re-evaluates when a setting changes (settings change rarely, so this is cheap and keeps the
 * QML free of per-key boilerplate). Writes go through set(), which validates against the defaults
 * table (unknown keys and wrong types are rejected).
 */
class AppSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap values READ values NOTIFY valuesChanged)
public:
    explicit AppSettings(QObject *parent = nullptr);
    /// Uses an explicit ini file (tests).
    explicit AppSettings(const QString &iniPath, QObject *parent = nullptr);

    QVariantMap values() const { return m_values; }
    static const QVariantMap &defaults();

    Q_INVOKABLE QVariant get(const QString &key) const;
    Q_INVOKABLE bool set(const QString &key, const QVariant &value);
    Q_INVOKABLE void reset(const QString &key);
    Q_INVOKABLE void resetAll();
    Q_INVOKABLE QVariantMap defaultValues() const { return defaults(); }

    int getInt(const QString &key) const { return get(key).toInt(); }
    bool getBool(const QString &key) const { return get(key).toBool(); }
    double getDouble(const QString &key) const { return get(key).toDouble(); }
    QString getString(const QString &key) const { return get(key).toString(); }
    QStringList getList(const QString &key) const { return get(key).toStringList(); }

    // Window state (kept apart from `values` so geometry writes do not re-evaluate UI bindings).
    Q_INVOKABLE QVariantMap windowState() const;
    Q_INVOKABLE void saveWindowState(int x, int y, int width, int height, bool maximized);

    QString fileName() const { return m_settings.fileName(); }

signals:
    void valuesChanged();
    void changed(const QString &key);

private:
    void load();
    QSettings m_settings;
    QVariantMap m_values;
};

} // namespace lumina
