#pragma once
// ---------------------------------------------------------------------------
// devicebridge.h — 自动识别设备与业务层的桥
//
// 把 EntryExitSimulator（模拟地感/相机/道闸）产生的事件接到 AppContext：
//   * entered           -> AppContext::enterVehicle() -> 打印入口票据
//   * waitingForApproval-> AppContext::leaveVehicle() -> approveExit()
//
// 人工登记（入库/结算对话框）与自动识别共用同一个 AppContext，
// 因此两条通道可以同时工作，界面通过 AppContext::changed() 统一刷新。
// ---------------------------------------------------------------------------
#include "domain/types.h"
#include "interface/entry_exit_simulator.h"

#include <QObject>
#include <QString>

namespace app {

class AppContext;

class DeviceBridge : public QObject
{
    Q_OBJECT

public:
    explicit DeviceBridge(AppContext* context, QObject* parent = nullptr);
    DeviceBridge(AppContext* context, const pl::devices::EntryExitSimulator::Options& options,
                 QObject* parent = nullptr);
    ~DeviceBridge() override;

    bool isListening() const { return m_listening; }
    bool isReadOnly() const;
    pl::devices::EntryExitSimulator* simulator() const { return m_simulator; }

public slots:
    void setListening(bool listening);

    // 入口：plate 为空时自动生成唯一演示车牌
    void requestEntry(const QString& plate = QString(), int vehicleCategory = 0);
    // 出口：plate 为空时从场内随机挑一辆车
    void requestExit(const QString& plate = QString());

signals:
    void autoEntered(const QString& plate, const QString& ticket, const QString& detail);
    void autoExited(const QString& plate, qint64 feeCents, const QString& detail);
    void autoRejected(const QString& plate, const QString& reason);
    void autoError(const QString& message);
    void laneStateChanged(pl::devices::LaneKind lane, pl::devices::LaneState state);
    void displayChanged(pl::devices::LaneKind lane, const QString& text);
    void listeningChanged(bool listening);

private:
    void buildConnections();
    void handleEntered(const pl::devices::PassageEvent& event);
    void handleWaitingExit(const pl::devices::PlateReading& reading);
    void handleDeviceError(pl::devices::DeviceError error, const QString& detail);

    QString nextEntryPlate() const;
    static pl::VehicleType mapCategory(int vehicleCategory);

    AppContext* m_ctx = nullptr;
    pl::devices::EntryExitSimulator* m_simulator = nullptr;
    bool m_listening = false;
    bool m_suppressRejectedError = false;   // 车道拒绝已单独上报，避免重复记一条异常
};

} // namespace app
