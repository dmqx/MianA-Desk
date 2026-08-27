#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

class UpdateChecker final : public QObject {
  Q_OBJECT
public:
  explicit UpdateChecker(QObject *parent = nullptr);
  void check();

signals:
  void updateAvailable(const QString &tagName, const QUrl &url);
  void upToDate();
  void checkFailed();

private:
  QNetworkAccessManager m_network;
};