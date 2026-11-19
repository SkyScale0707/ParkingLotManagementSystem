#include "domain/billing.h"

#include <QDateTime>

namespace pl {
namespace {
constexpr qint64 kMinuteMs = 60'000;
}

qint64 feeCentsFor(qint64 enteredAtMs, qint64 now, const Config& c, qint64* billableMs)
{
    qint64 stay = now - enteredAtMs;
    if (stay < 0)
        stay = 0;

    const qint64 freeMs = static_cast<qint64>(c.freeMinutes) * kMinuteMs;
    if (stay <= freeMs) {
        if (billableMs)
            *billableMs = 0;
        return 0;
    }

    const qint64 billable = stay - freeMs;
    qint64 unitMs = static_cast<qint64>(c.billingUnitMinutes) * kMinuteMs;
    if (unitMs <= 0)
        unitMs = kMinuteMs;

    const qint64 units = (billable + unitMs - 1) / unitMs;
    if (billableMs)
        *billableMs = billable;
    return units * static_cast<qint64>(c.unitPriceCents);
}

QString formatMoney(qint64 cents)
{
    const bool negative = cents < 0;
    if (negative)
        cents = -cents;
    const qint64 yuan = cents / 100;
    const qint64 fen = cents % 100;
    return QStringLiteral("%1¥%2.%3")
        .arg(negative ? QStringLiteral("-") : QString())
        .arg(yuan)
        .arg(fen, 2, 10, QLatin1Char('0'));
}

QString formatDuration(qint64 ms)
{
    if (ms < 0)
        ms = 0;
    if (ms < 60'000)
        return QStringLiteral("不足 1 分钟");

    const qint64 totalMinutes = ms / 60'000;
    const qint64 hours = totalMinutes / 60;
    const qint64 minutes = totalMinutes % 60;
    if (hours > 0)
        return QStringLiteral("%1 小时 %2 分").arg(hours).arg(minutes, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1 分钟").arg(minutes);
}

QString formatDateTime(qint64 ms)
{
    return QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QString formatClock(qint64 ms)
{
    return QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("HH:mm:ss"));
}

} // namespace pl
