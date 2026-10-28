#include "Redact.h"
#include <QRegularExpression>
#include <QUrlQuery>
#include <QUrl>
#include <QList>
#include <QStringList>
#include <QString>

namespace lumina {

static bool isSecretKey(const QString &k)
{
    static const QStringList keys = {
        QStringLiteral("token"),   QStringLiteral("key"),      QStringLiteral("apikey"),
        QStringLiteral("api_key"), QStringLiteral("password"), QStringLiteral("passwd"),
        QStringLiteral("pwd"),     QStringLiteral("secret"),   QStringLiteral("sig"),
        QStringLiteral("signature"), QStringLiteral("auth"),   QStringLiteral("access_token")};
    return keys.contains(k.toLower());
}

QString redactUrl(const QString &text)
{
    QUrl url(text, QUrl::TolerantMode);
    if (!url.isValid() || url.scheme().isEmpty() || url.isLocalFile())
        return text;
    if (!url.userInfo().isEmpty())
        url.setUserInfo(QStringLiteral("***"));
    if (url.hasQuery()) {
        QUrlQuery q(url);
        QList<QPair<QString, QString>> items = q.queryItems(QUrl::FullyDecoded);
        bool changed = false;
        for (auto &kv : items) {
            if (isSecretKey(kv.first)) {
                kv.second = QStringLiteral("***");
                changed = true;
            }
        }
        if (changed) {
            QUrlQuery nq;
            for (const auto &kv : items)
                nq.addQueryItem(kv.first, kv.second);
            url.setQuery(nq);
        }
    }
    return url.toString(QUrl::FullyEncoded);
}

bool urlHasCredentials(const QString &text)
{
    QUrl url(text, QUrl::TolerantMode);
    return url.isValid() && !url.scheme().isEmpty() && !url.userInfo().isEmpty();
}

QString redactLogLine(const QString &line)
{
    static const QRegularExpression re(QStringLiteral("([a-zA-Z][a-zA-Z0-9+.-]*://[^\\s'\"<>]+)"));
    QString out;
    int last = 0;
    auto it = re.globalMatch(line);
    while (it.hasNext()) {
        auto m = it.next();
        out += line.mid(last, m.capturedStart() - last);
        out += redactUrl(m.captured(1));
        last = m.capturedEnd();
    }
    out += line.mid(last);
    return out;
}

} // namespace lumina
