#pragma once

#include <QString>
#include <QTimeZone>

namespace MarketRules {

[[nodiscard]] QString normalizeSymbol(const QString &raw);
[[nodiscard]] QString marketKey(const QString &symbol);
[[nodiscard]] QString currency(const QString &symbol);
[[nodiscard]] int pricePrecision(const QString &symbol);
[[nodiscard]] const QTimeZone &timeZone(const QString &market);

} // namespace MarketRules
