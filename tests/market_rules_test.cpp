#include "MianA.Core/MarketRules.h"
#include "MianA.Market/QuoteProviders.h"

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  int failures = 0;
  const auto check = [&](const QString &actual, const QString &expected) {
    if (actual != expected) {
      qCritical().noquote() << "Expected" << expected << "but got" << actual;
      ++failures;
    }
  };
  using QuoteProviders::Provider;

  const QString beijing = MarketRules::normalizeSymbol(QStringLiteral("920171"));
  check(beijing, QStringLiteral("920171.BJ"));
  check(QuoteProviders::queryKey(Provider::Tencent, beijing),
        QStringLiteral("bj920171"));
  check(MarketRules::normalizeSymbol(QStringLiteral("900901")),
        QStringLiteral("900901.SS"));
  check(MarketRules::normalizeSymbol(QStringLiteral("600000")),
        QStringLiteral("600000.SS"));
  check(MarketRules::normalizeSymbol(QStringLiteral("920171.BJ")), beijing);

  for (const auto &symbol : {QStringLiteral("700.HK"), QStringLiteral("0700.HK"),
                             QStringLiteral("00700.HK")}) {
    check(MarketRules::normalizeSymbol(symbol), symbol);
    check(QuoteProviders::queryKey(Provider::Yahoo, symbol),
          QStringLiteral("0700.HK"));
    check(QuoteProviders::queryKey(Provider::Tencent, symbol),
          QStringLiteral("r_hk00700"));
  }
  check(QuoteProviders::queryKey(Provider::Yahoo, QStringLiteral("80700.HK")),
        QStringLiteral("80700.HK"));
  check(QuoteProviders::queryKey(Provider::Tencent, QStringLiteral("80700.HK")),
        QStringLiteral("r_hk80700"));
  check(QuoteProviders::queryKey(Provider::Yahoo, QStringLiteral("ABC.HK")),
        QStringLiteral("ABC.HK"));
  check(QuoteProviders::queryKey(Provider::Yahoo, QStringLiteral("BRK.B.US")),
        QStringLiteral("BRK-B"));
  return failures == 0 ? 0 : 1;
}
