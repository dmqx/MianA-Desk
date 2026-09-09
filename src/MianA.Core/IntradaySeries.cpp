#include "MianA.Core/IntradaySeries.h"

#include "MianA.Core/MarketRules.h"

#include <QDateTime>
#include <QMap>
#include <QTimeZone>
#include <algorithm>
#include <cmath>
#include <limits>

namespace IntradaySeries {

int chinaSlot(int minuteOfDay) {
  if (minuteOfDay >= 570 && minuteOfDay <= 690)
    return std::min(119, minuteOfDay - 570);
  if (minuteOfDay >= 780 && minuteOfDay <= 900)
    return std::min(239, 120 + minuteOfDay - 780);
  return -1;
}

void appendLiveQuote(Position &position, const QuoteResult &quote,
                     bool tradingDay) {
  const QString market = MarketRules::marketKey(position.symbol);
  const auto quoteTime =
      (quote.marketTimestamp > 0
           ? QDateTime::fromSecsSinceEpoch(quote.marketTimestamp,
                                           QTimeZone::UTC)
           : QDateTime::currentDateTimeUtc())
          .toTimeZone(MarketRules::timeZone(market));
  const QDate quoteDate = quoteTime.date();
  const qint64 minute = quoteTime.toSecsSinceEpoch() / 60;
  const int minuteOfDay = quoteTime.time().hour() * 60 +
                          quoteTime.time().minute();

  if (quoteDate > position.intradayDate) {
    position.intraday.clear();
    position.intradayMinutes.clear();
    position.intradayMinute = 0;
    position.intradayDate = quoteDate;
  }
  if (quoteDate != position.intradayDate || minute < position.intradayMinute ||
      (quote.marketTimestamp <= 0 && !tradingDay))
    return;

  if (market == QStringLiteral("CN")) {
    const int slot = chinaSlot(minuteOfDay);
    if (slot < 0)
      return;
    if (position.intraday.size() != 240)
      position.intraday = QVector<double>(
          240, std::numeric_limits<double>::quiet_NaN());
    position.intraday[slot] = quote.price;
    position.intradayMinute = minute;
    return;
  }

  const bool inSession =
      (market == QStringLiteral("HK") &&
       ((minuteOfDay >= 570 && minuteOfDay <= 720) ||
        (minuteOfDay >= 780 && minuteOfDay <= 960))) ||
      (market == QStringLiteral("US") && minuteOfDay >= 570 &&
       minuteOfDay <= 960);
  if (!inSession)
    return;

  if (!position.intradayMinutes.isEmpty() &&
      position.intradayMinutes.last() == minute) {
    position.intraday.last() = quote.price;
  } else {
    position.intraday.append(quote.price);
    position.intradayMinutes.append(minute);
    if (position.intraday.size() > 480) {
      position.intraday.removeFirst();
      position.intradayMinutes.removeFirst();
    }
  }
  position.intradayMinute = minute;
}

bool mergeHistory(Position &position, const IntradayResult &history) {
  auto prices = history.prices;
  auto minutes = history.minutes;
  const bool sameDay = position.intradayDate == history.tradingDate;

  if (MarketRules::marketKey(position.symbol) == QStringLiteral("CN")) {
    int lastHistorySlot = -1;
    for (qsizetype i = 0; i < prices.size(); ++i) {
      if (std::isfinite(prices[i]) && prices[i] > 0)
        lastHistorySlot = int(i);
    }
    if (sameDay) {
      const auto count = std::min(prices.size(), position.intraday.size());
      for (qsizetype i = 0; i < count; ++i) {
        if (std::isfinite(position.intraday[i]) && position.intraday[i] > 0 &&
            (i >= lastHistorySlot || !std::isfinite(prices[i]) ||
             prices[i] <= 0))
          prices[i] = position.intraday[i];
      }
    }
  } else {
    if (prices.size() != minutes.size())
      return false;
    QMap<qint64, double> merged;
    for (qsizetype i = 0; i < prices.size(); ++i)
      merged.insert(minutes[i], prices[i]);
    const qint64 lastHistoryMinute = merged.isEmpty() ? 0 : merged.lastKey();
    if (sameDay) {
      const auto count =
          std::min(position.intraday.size(), position.intradayMinutes.size());
      for (qsizetype i = 0; i < count; ++i) {
        const qint64 liveMinute = position.intradayMinutes[i];
        if (liveMinute >= lastHistoryMinute || !merged.contains(liveMinute))
          merged.insert(liveMinute, position.intraday[i]);
      }
    }
    while (merged.size() > 480)
      merged.erase(merged.begin());
    minutes = merged.keys();
    prices = merged.values();
  }

  position.intraday = std::move(prices);
  position.intradayMinutes = std::move(minutes);
  position.intradayDate = history.tradingDate;
  if (!sameDay)
    position.intradayMinute = 0;
  if (!position.intradayMinutes.isEmpty())
    position.intradayMinute = position.intradayMinutes.last();
  return true;
}

} // namespace IntradaySeries
