#pragma once

#include "MianA.Core/Quote.h"

#include <QByteArray>
#include <QString>

namespace QuoteProviders {
enum class Provider { Tencent, Sina, EastMoney, Yahoo };
[[nodiscard]] QString name(Provider provider);
[[nodiscard]] QString queryKey(Provider provider, const QString &symbol);
[[nodiscard]] QString decodeChinesePayload(const QByteArray &body);
[[nodiscard]] QuoteResult parseReply(Provider provider,
                                     const QString &positionId,
                                     const QString &symbol,
                                     const QByteArray &body);
[[nodiscard]] QuoteResult parseReplyText(Provider provider,
                                         const QString &positionId,
                                         const QString &symbol,
                                         const QString &text);
} // namespace QuoteProviders
