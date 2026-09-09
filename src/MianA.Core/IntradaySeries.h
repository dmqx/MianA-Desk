#pragma once

#include "MianA.Core/Position.h"
#include "MianA.Core/Quote.h"

namespace IntradaySeries {
[[nodiscard]] int chinaSlot(int minuteOfDay);
void appendLiveQuote(Position &position, const QuoteResult &quote,
                     bool tradingDay);
[[nodiscard]] bool mergeHistory(Position &position,
                                const IntradayResult &history);
} // namespace IntradaySeries
