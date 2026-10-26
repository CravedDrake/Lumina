#pragma once
#include <QString>
#include <QUrl>

namespace lumina {

// Removes credentials (user:password@, ?token= style secrets) from anything
// that looks like a URL so that it is safe to write to logs or show in toasts.
QString redactUrl(const QString &text);

// True if the string is a URL that carries embedded credentials.
bool urlHasCredentials(const QString &text);

// Redacts every URL found inside an arbitrary log line.
QString redactLogLine(const QString &line);

} // namespace lumina
