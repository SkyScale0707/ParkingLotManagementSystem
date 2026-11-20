#pragma once
// ---------------------------------------------------------------------------
// extra_equipment_state.h — 附加外设：通过计数器（空置占位）
//
// 对应原草稿 extra_equipment_state.cpp 中的：
//   * counterState  -> CounterState（Fine / Error / Disconnected）
//   * carIOState    -> CarIOState（In / Out）
//   * counterMake   -> CounterDevice
//
// 注意：CounterDevice 目前**保持空置**，不产生、不累计任何数据。
// 真实计数器外设接入后，再在这里补充数据接收接口与实现，
// 避免模拟数据与真实外设数据混在一起。
// ---------------------------------------------------------------------------
#include <QObject>
#include <QString>

namespace pl {
namespace devices {

enum class CounterState : int {
    Fine = 0,
    Error = 1,
    Disconnected = 2,
};

QString counterStateText(CounterState state);

enum class CarIOState : int {
    In = 0,
    Out = 1,
};

QString carIOStateText(CarIOState state);

// 空置占位：等待真实外设类传入数据。
class CounterDevice : public QObject
{
    Q_OBJECT

public:
    explicit CounterDevice(QObject* parent = nullptr);
};

} // namespace devices
} // namespace pl
