#pragma once

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
