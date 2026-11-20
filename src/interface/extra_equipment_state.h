#pragma once
// ---------------------------------------------------------------------------
// extra_equipment_state.h — 附加外设：通过计数器
//
// 对应原草稿 extra_equipment_state.cpp：
//   * counterState  -> CounterState（Fine / Error / Disconnected）
//   * carIOState    -> CarIOState（In / Out）
//   * counterMake   -> CounterDevice（记录每次通过的车牌、方向与时间）
//
// 计数器只负责「数数」与状态上报，不参与计费/放行判断。
// ---------------------------------------------------------------------------
#include <QObject>
#include <QString>
#include <QtGlobal>

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

class CounterDevice : public QObject
{
    Q_OBJECT

public:
    explicit CounterDevice(QObject* parent = nullptr);

    CounterState state() const { return m_state; }
    void setState(CounterState state);

    qint64 inCount() const { return m_inCount; }
    qint64 outCount() const { return m_outCount; }
    qint64 netCount() const { return m_inCount - m_outCount; }   // 当前在场

    // 记录一次车辆通过；计数器故障/离线时返回 false
    bool record(const QString& plate, CarIOState state, qint64 atMs);
    void reset();

signals:
    void counted(const QString& plate, pl::devices::CarIOState state, qint64 atMs);
    void stateChanged(pl::devices::CounterState state);

private:
    CounterState m_state = CounterState::Fine;
    qint64 m_inCount = 0;
    qint64 m_outCount = 0;
};

} // namespace devices
} // namespace pl
