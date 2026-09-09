#pragma once

#include "MianA.Core/AppSettings.h"
#include "MianA.Core/Position.h"

#include <QJsonObject>
#include <QString>

class AppStore final {
public:
  AppStore();
  bool load(QVector<Position> &positions, AppSettings &settings);
  bool save(const QVector<Position> &positions, const AppSettings &settings);
  [[nodiscard]] QString loadError() const;

private:
  static QString dataPath();
  bool backupInvalidData(const QString &reason);
  static Position parsePosition(const QJsonObject &object, bool &valid);
  QString m_path;
  QString m_loadError;
  bool m_safeToOverwrite = true;
};
