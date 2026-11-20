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

// 空置：真实外设计数器接入后再实现数据接收。
CounterDevice::CounterDevice(QObject* parent)
    : QObject(parent)
{
}

} // namespace devices
} // namespace pl
