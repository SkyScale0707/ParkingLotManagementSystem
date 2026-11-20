#pragma once
// ---------------------------------------------------------------------------
// lane_controller.h — 单条车道的设备状态机
//
// 入口：地感触发 → 相机识别 → 道闸开 → 车辆通过 → 道闸关 → entered()
// 出口：地感触发 → 相机识别 → （可选）等待业务确认 → 道闸开 → exited()
//
// 控制器只依赖 devices.h 里的抽象接口，不关心设备是模拟的还是真实的。
// 业务层（下一轮）只需要：
//   * 入口监听 entered()，调用 AppContext::enterVehicle，再 printTicket()
//   * 出口监听 waitingForApproval()，算费后 approveExit()/rejectExit()
// ---------------------------------------------------------------------------
#include "clock.h"
#include "devices.h"

#include <QTimer>

namespace pl {
namespace devices {

class LaneController : public QObject
{
    Q_OBJECT

public:
    struct DeviceSet {
        ILoopDetector* loop = nullptr;
        IPlateRecognizer* camera = nullptr;
        IBarrier* barrier = nullptr;
        IDisplayBoard* display = nullptr;
        ITicketPrinter* printer = nullptr;
    };

    explicit LaneController(LaneKind lane, const DeviceSet& devices, QObject* parent = nullptr);
    ~LaneController() override;

    LaneKind lane() const { return m_lane; }
    LaneState state() const { return m_state; }
    QString lastPlate() const { return m_reading.plate; }
    PassageEvent lastPassage() const { return m_lastPassage; }
    DeviceSet devices() const { return m_dev; }

    double minimumConfidence() const { return m_minConfidence; }
    void setMinimumConfidence(double value);

    int recognitionTimeoutMs() const { return m_recognitionTimeoutMs; }
    void setRecognitionTimeoutMs(int ms);

    int passageTimeoutMs() const { return m_passageTimeoutMs; }
    void setPassageTimeoutMs(int ms);

    bool requiresApproval() const { return m_requiresApproval; }
    void setRequiresApproval(bool required);

    void setClock(IClock* clock);   // 非拥有，null 回退到系统时钟
    IClock* clock() const { return m_clock; }

    void start();
    void stop();
    bool isRunning() const { return m_running; }

    // 出口业务确认
    bool approveExit();
    bool rejectExit(const QString& reason = QString());

    // 入口业务处理完后补打票据
    bool printTicket(const QString& ticket);

signals:
    void stateChanged(pl::devices::LaneState state);
    void plateRecognized(const pl::devices::PlateReading& reading);
    void entered(const pl::devices::PassageEvent& event);
    void exited(const pl::devices::PassageEvent& event);
    void rejected(pl::devices::DeviceError error, const QString& detail);
    void waitingForApproval(const pl::devices::PlateReading& reading);
    void displayChanged(const QString& text);
    void ticketIssued(const QString& ticket, const pl::devices::PassageEvent& event);

private:
    void onVehicleArrived();
    void onVehicleLeft();
    void onRecognized(const PlateReading& reading);
    void onRecognitionFailed(DeviceError error, const QString& detail);
    void onBarrierOpened();
    void onBarrierClosed();
    void onBarrierJammed(const QString& reason);
    void onRecognitionTimeout();
    void onPassageTimeout();

    void setState(LaneState state);
    void showMessage(const QString& text);
    void beginBarrierOpen();
    void completePassage();
    void resetToIdle();
    QString idleMessage() const;

    LaneKind m_lane = LaneKind::Entry;
    DeviceSet m_dev;
    LaneState m_state = LaneState::Idle;
    bool m_running = false;
    bool m_requiresApproval = false;
    double m_minConfidence = 0.75;
    int m_recognitionTimeoutMs = 3000;
    int m_passageTimeoutMs = 8000;

    PlateReading m_reading;
    PassageEvent m_lastPassage;

    QTimer m_recognitionTimer;
    QTimer m_passageTimer;
    SystemClock m_defaultClock;
    IClock* m_clock = nullptr;
};

} // namespace devices
} // namespace pl
