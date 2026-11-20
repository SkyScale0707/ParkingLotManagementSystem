#include "extra_equipment_state.h"

namespace pl {
namespace devices {

QString counterStateText(CounterState state)
{
    switch (state) {
    case CounterState::Fine:         return QStringLiteral("正常");
    case CounterState::Error:        return QStringLiteral("故障");
    case CounterState::Disconnected: return QStringLiteral("未连接");
    }
    return QStringLiteral("未知");
}

QString carIOStateText(CarIOState state)
{
    return state == CarIOState::In ? QStringLiteral("入场") : QStringLiteral("出场");
}

CounterDevice::CounterDevice(QObject* parent)
    : QObject(parent)
{
}

void CounterDevice::setState(CounterState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(m_state);
}

bool CounterDevice::record(const QString& plate, CarIOState state, qint64 atMs)
{
    if (m_state != CounterState::Fine)
        return false;

    if (state == CarIOState::In)
        ++m_inCount;
    else
        ++m_outCount;

    emit counted(plate, state, atMs);
    return true;
}

void CounterDevice::reset()
{
    m_inCount = 0;
    m_outCount = 0;
}

} // namespace devices
} // namespace pl
