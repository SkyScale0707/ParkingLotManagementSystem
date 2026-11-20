#pragma once
// ---------------------------------------------------------------------------
// entry_exit_simulator.h — 一进一出两条车道的模拟器
//
// 把「模拟设备 + 车道状态机 + 进出口监控」组装成一个可直接使用的对象：
//
//   EntryExitSimulator sim;
//   connect(&sim, &EntryExitSimulator::entered, ...);   // 入口识别+通过
//   connect(&sim, &EntryExitSimulator::exited, ...);    // 出口识别+通过
//   sim.simulateEntry("鄂A12345");
//   sim.simulateExit("鄂A12345");                       // 需先 approveExit()
//
// 车辆通过由模拟器自动驱动：道闸打开后等待 vehiclePassMs，再让地感复位。
// ---------------------------------------------------------------------------
#include "clock.h"
#include "extra_equipment_state.h"
#include "simulated_devices.h"
#include "std_car_io.h"

#include <QObject>

namespace pl {
namespace devices {

class EntryExitSimulator : public QObject
{
    Q_OBJECT

public:
    struct Options {
        SimulationProfile entryProfile;
        SimulationProfile exitProfile;
        bool requireExitApproval = false;   // 出口是否需要业务确认后才抬杆
        quint32 seed = 20251120;
    };

    explicit EntryExitSimulator(QObject* parent = nullptr);
    explicit EntryExitSimulator(const Options& options, QObject* parent = nullptr);
    ~EntryExitSimulator() override;

    void setClock(IClock* clock);   // 非拥有，null 回退到系统时钟
    IClock* clock() const { return m_clock; }

    const Options& options() const { return m_options; }

    StdCarInMonitor* entryMonitor() const { return m_entryMonitor; }
    StdCarOutMonitor* exitMonitor() const { return m_exitMonitor; }
    LaneController* entryController() const;
    LaneController* exitController() const;
    CounterDevice* counter() const { return m_counter; }

    SimulatedLoopDetector* entryLoop() const { return m_entryLoop; }
    SimulatedPlateRecognizer* entryCamera() const { return m_entryCamera; }
    SimulatedBarrier* entryBarrier() const { return m_entryBarrier; }
    SimulatedDisplayBoard* entryDisplay() const { return m_entryDisplay; }
    SimulatedTicketPrinter* entryPrinter() const { return m_entryPrinter; }

    SimulatedLoopDetector* exitLoop() const { return m_exitLoop; }
    SimulatedPlateRecognizer* exitCamera() const { return m_exitCamera; }
    SimulatedBarrier* exitBarrier() const { return m_exitBarrier; }
    SimulatedDisplayBoard* exitDisplay() const { return m_exitDisplay; }

    bool isEntryBusy() const { return m_entryBusy; }
    bool isExitBusy() const { return m_exitBusy; }

    bool approveExit();
    bool rejectExit(const QString& reason = QString());

public slots:
    // plate 为空时由相机随机生成演示车牌；confidence < 0 表示随机
    void simulateEntry(const QString& plate = QString(), double confidence = -1.0,
                       int vehicleCategory = -1);
    void simulateExit(const QString& plate = QString(), double confidence = -1.0,
                      int vehicleCategory = -1);

signals:
    void entered(const pl::devices::PassageEvent& event);
    void exited(const pl::devices::PassageEvent& event);
    void waitingForExitApproval(const pl::devices::PlateReading& reading);
    void deviceError(pl::devices::DeviceError error, const QString& detail);
    void displayChanged(pl::devices::LaneKind lane, const QString& text);

private:
    void build(const Options& options);
    void schedulePassThrough(LaneKind lane);
    void handleMonitorError(LaneKind lane, const CarIOError& error);

    Options m_options;
    SystemClock m_defaultClock;
    IClock* m_clock = nullptr;
    CounterDevice* m_counter = nullptr;

    SimulatedLoopDetector* m_entryLoop = nullptr;
    SimulatedPlateRecognizer* m_entryCamera = nullptr;
    SimulatedBarrier* m_entryBarrier = nullptr;
    SimulatedDisplayBoard* m_entryDisplay = nullptr;
    SimulatedTicketPrinter* m_entryPrinter = nullptr;
    StdCarInMonitor* m_entryMonitor = nullptr;

    SimulatedLoopDetector* m_exitLoop = nullptr;
    SimulatedPlateRecognizer* m_exitCamera = nullptr;
    SimulatedBarrier* m_exitBarrier = nullptr;
    SimulatedDisplayBoard* m_exitDisplay = nullptr;
    StdCarOutMonitor* m_exitMonitor = nullptr;

    bool m_entryBusy = false;
    bool m_exitBusy = false;
    int m_entryGeneration = 0;
    int m_exitGeneration = 0;
};

} // namespace devices
} // namespace pl
