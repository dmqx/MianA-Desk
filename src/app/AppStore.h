#pragma once

#include "models/PositionModel.h"

#include <QJsonObject>
#include <QString>

struct AppSettings {
  int x = 80;
  int y = 80;
  bool topmost = true;
  bool locked = false;
  bool compact = false;
  bool floating = false;
  bool showIntraday = true;
  bool hideStockCode = false;
  QString focusId;
  bool paused = false;
  bool minimizeToTray = true;
  bool autoTheme = false;
  QString themeColor = QStringLiteral("#FFFFFF");
  int frameOpacity = 95;
  int textOpacity = 95;
};

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
