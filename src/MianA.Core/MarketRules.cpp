#include "MianA.Core/MarketRules.h"

#include <QRegularExpression>

namespace MarketRules {

QString normalizeSymbol(const QString &raw) {
  QString value = raw.trimmed().toUpper();
  value.remove(QLatin1Char(' '));
  const QList<QPair<QString, QString>> suffixes{
      {QStringLiteral(".XSHG"), QStringLiteral(".SS")},
      {QStringLiteral(".SH"), QStringLiteral(".SS")},
      {QStringLiteral(".XSHE"), QStringLiteral(".SZ")},
      {QStringLiteral(".SHE"), QStringLiteral(".SZ")}};
  for (const auto &[from, to] : suffixes) {
    if (value.endsWith(from))
      return value.first(value.size() - from.size()) + to;
  }
  if (value.endsWith(QStringLiteral(".SS")) ||
      value.endsWith(QStringLiteral(".SZ")) ||
      value.endsWith(QStringLiteral(".BJ")) ||
      value.endsWith(QStringLiteral(".HK")) ||
      value.endsWith(QStringLiteral(".US")))
    return value;

  static const QRegularExpression usTicker(
      QStringLiteral("^[A-Z][A-Z0-9]*([.-][A-Z0-9]+)?$"));
  if (usTicker.match(value).hasMatch()) {
    value.replace(QLatin1Char('-'), QLatin1Char('.'));
    return value + QStringLiteral(".US");
  }
  if (value.contains(QLatin1Char('.')) || value.contains(QLatin1Char('=')) ||
      value.contains(QLatin1Char('^')))
    return value;

  static const QRegularExpression sixDigits(QStringLiteral("^[0-9]{6}$"));
  if (sixDigits.match(value).hasMatch()) {
    if (QStringLiteral("659").contains(value.front()))
      return value + QStringLiteral(".SS");
    if (QStringLiteral("0123").contains(value.front()))
      return value + QStringLiteral(".SZ");
    if (QStringLiteral("48").contains(value.front()))
      return value + QStringLiteral(".BJ");
  }
  return value;
}

QString marketKey(const QString &symbol) {
  const QString upper = normalizeSymbol(symbol);
  if (upper.endsWith(QStringLiteral(".SS")) ||
      upper.endsWith(QStringLiteral(".SZ")) ||
      upper.endsWith(QStringLiteral(".BJ")))
    return QStringLiteral("CN");
  if (upper.endsWith(QStringLiteral(".HK")))
    return QStringLiteral("HK");
  if (upper.endsWith(QStringLiteral(".US")))
    return QStringLiteral("US");
  return QStringLiteral("CN");
}

QString currency(const QString &symbol) {
  const QString market = marketKey(symbol);
  return market == QStringLiteral("CN")   ? QStringLiteral("CNY")
         : market == QStringLiteral("HK") ? QStringLiteral("HKD")
                                           : QStringLiteral("USD");
}

int pricePrecision(const QString &symbol) {
  const QString value = normalizeSymbol(symbol);
  const QString code = value.section(QLatin1Char('.'), 0, 0);
  if (code.isEmpty())
    return 2;
  if (value.endsWith(QStringLiteral(".SS")))
    return code.startsWith(QLatin1Char('6')) ? 2 : 3;
  if (value.endsWith(QStringLiteral(".SZ")))
    return QStringLiteral("023").contains(code.front()) ? 2 : 3;
  return value.endsWith(QStringLiteral(".HK")) ? 3 : 2;
}

const QTimeZone &timeZone(const QString &market) {
  static const QTimeZone newYork(QByteArrayLiteral("America/New_York"));
  static const QTimeZone hongKong(QByteArrayLiteral("Asia/Hong_Kong"));
  static const QTimeZone shanghai(QByteArrayLiteral("Asia/Shanghai"));
  return market == QStringLiteral("US")   ? newYork
         : market == QStringLiteral("HK") ? hongKong
                                           : shanghai;
}

} // namespace MarketRules
