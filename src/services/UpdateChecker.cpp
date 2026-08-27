#include "UpdateChecker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace {
const QUrl LatestReleaseUrl(QStringLiteral(
    "https://api.github.com/repos/dmqx/MianA-Desk/releases/latest"));

bool tagIsNewer(const QString &tag, const QString &current) {
  auto numbers = [](QString value) {
    value = value.trimmed();
    if (value.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
      value.remove(0, 1);
    QStringList list = value.split(QLatin1Char('.'));
    while (list.size() < 3)
      list << QStringLiteral("0");
    return list;
  };
  const QStringList latest = numbers(tag);
  const QStringList ours = numbers(current);
  for (int i = 0; i < 3; ++i) {
    bool latestOk = false;
    bool oursOk = false;
    const int latestValue = latest.at(i).toInt(&latestOk);
    const int oursValue = ours.at(i).toInt(&oursOk);
    if (!latestOk || !oursOk)
      return false;
    if (latestValue != oursValue)
      return latestValue > oursValue;
  }
  return false;
}
} // namespace

UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent) {}

void UpdateChecker::check() {
  QNetworkRequest request(LatestReleaseUrl);
  request.setRawHeader(QByteArrayLiteral("User-Agent"),
                       QByteArrayLiteral("MianADesk/" MIANA_VERSION_STRING));
  request.setRawHeader(QByteArrayLiteral("Accept"),
                       QByteArrayLiteral("application/vnd.github+json"));
  request.setTransferTimeout(8000);
  QNetworkReply *reply = m_network.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      emit checkFailed();
      return;
    }
    const QJsonObject object =
        QJsonDocument::fromJson(reply->readAll()).object();
    const QString tag = object.value(QStringLiteral("tag_name")).toString();
    const QUrl url(object.value(QStringLiteral("html_url")).toString());
    if (tag.isEmpty() || url.isEmpty()) {
      emit checkFailed();
      return;
    }
    if (tagIsNewer(tag, QStringLiteral(MIANA_VERSION_STRING)))
      emit updateAvailable(tag, url);
    else
      emit upToDate();
  });
}