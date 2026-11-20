#include "clock.h"

#include <QDateTime>

namespace pl {
namespace devices {

qint64 SystemClock::nowMs() const
{
    return QDateTime::currentMSecsSinceEpoch();
}

ManualClock::ManualClock(qint64 startMs)
    : m_nowMs(startMs)
{
}

qint64 ManualClock::nowMs() const
{
    return m_nowMs;
}

void ManualClock::setNowMs(qint64 nowMs)
{
    m_nowMs = nowMs;
}

void ManualClock::advanceMs(qint64 deltaMs)
{
    m_nowMs += deltaMs;
}

} // namespace devices
} // namespace pl
