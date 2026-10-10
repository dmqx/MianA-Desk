#pragma once

#include "MianA.Core/Quote.h"

#include <QDate>
#include <QString>
#include <QVector>

struct Position {
  QString id;
  QString symbol;
  QString name;
  double price = 0.0;
  QString currency = QStringLiteral("CNY");
  int precision = 2;
  bool customName = false;
  double updatedAt = 0.0;
  QString error;
  QString source;
  int failureCount = 0;
  double retryAfter = 0.0;
  double changePercent = 0.0;
  double previousClose = 0.0;
  bool hasChange = false;
  QVector<BookLevel> bids;
  QVector<BookLevel> asks;
  QVector<double> intraday;
  qint64 intradayMinute = 0;
  QVector<qint64> intradayMinutes;
  QDate intradayDate;
};
