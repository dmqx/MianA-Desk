#pragma once

#include <QDate>
#include <QMetaType>
#include <QString>
#include <QVector>

struct QuoteRequest {
  QString positionId;
  QString symbol;
};

struct QuoteResult {
  QString positionId;
  QString symbol;
  bool success = false;
  double price = 0.0;
  QString currency;
  QString name;
  QString source;
  int precision = 2;
  qint64 marketTimestamp = 0;
  double changePercent = 0.0;
  double previousClose = 0.0;
  bool hasChange = false;
  QString error;
};

struct IntradayResult {
  QString positionId;
  QString symbol;
  bool success = false;
  QDate tradingDate;
  QVector<double> prices;
  QVector<qint64> minutes;
  QString error;
};

Q_DECLARE_METATYPE(IntradayResult)
