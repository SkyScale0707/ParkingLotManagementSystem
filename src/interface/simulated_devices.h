#pragma once
// ---------------------------------------------------------------------------
// simulated_devices.h — 模拟外部设备
//
// 这些类模拟真实硬件的时序与故障，用于在没有硬件的情况下跑通出入口流程，
// 也用于下一轮把 AppContext 接到「自动识别」上做适配实验。
// ---------------------------------------------------------------------------
#include "clock.h"
#include "devices.h"

#include <QQueue>
#include <QRandomGenerator>
#include <QTimer>

namespace pl {
namespace devices {

// 地感线圈：setVehiclePresent(true/false) 触发到达/离开边沿
class SimulatedLoopDetector final : public ILoopDetector
{
    Q_OBJECT

public:
    explicit SimulatedLoopDetector(QObject* parent = nullptr);

    bool vehiclePresent() const override;
    void setVehiclePresent(bool present) override;

private:
    bool m_present = false;
};

// 车牌识别相机：可预置识别结果、注入故障、模拟低置信度
class SimulatedPlateRecognizer final : public IPlateRecognizer
{
    Q_OBJECT

public:
    struct PendingRecognition {
        QString plate;              // 空字符串表示让模拟器随机生成
        double confidence = -1.0;   // < 0 表示随机 0.85~0.99
        int vehicleCategory = -1;
    };

    explicit SimulatedPlateRecognizer(const SimulationProfile& profile = SimulationProfile(),
                                      QObject* parent = nullptr);

    void setProfile(const SimulationProfile& profile);
    const SimulationProfile& profile() const { return m_profile; }

    void setSeed(quint32 seed);
    void setClock(IClock* clock);   // 非拥有，null 回退到系统时钟

    // 预置下一次识别结果；队列为空时自动生成演示车牌
    void enqueueRecognition(const QString& plate = QString(), double confidence = -1.0,
                            int vehicleCategory = -1);
    void clearQueue();
    int pendingCount() const { return m_queue.size(); }

    // 故障注入
    void injectFailure(DeviceError error, const QString& detail = QString());
    void setAlwaysFail(bool always, DeviceError error = DeviceError::CameraTimeout);

    void requestRecognition(LaneKind lane) override;
    void cancel() override;
    bool busy() const override { return m_busy; }

private:
    void finishRecognition();

    SimulationProfile m_profile;
    QTimer* m_timer = nullptr;
    SystemClock m_defaultClock;
    IClock* m_clock = nullptr;
    QQueue<PendingRecognition> m_queue;
    QRandomGenerator m_rng;
    bool m_busy = false;
    LaneKind m_lane = LaneKind::Entry;
    bool m_alwaysFail = false;
    bool m_hasInjectedFailure = false;
    DeviceError m_injectedError = DeviceError::CameraTimeout;
    QString m_injectedDetail;
};

// 道闸：打开/关闭带延时，可注入卡死
class SimulatedBarrier final : public IBarrier
{
    Q_OBJECT

public:
    explicit SimulatedBarrier(const SimulationProfile& profile = SimulationProfile(),
                              QObject* parent = nullptr);

    void setProfile(const SimulationProfile& profile);
    const SimulationProfile& profile() const { return m_profile; }

    State state() const override { return m_state; }
    void open() override;
    void close() override;

    void setJamProbability(double probability);   // 0.0 ~ 1.0
    void injectJam(bool jam);
    void setSeed(quint32 seed);

private:
    void finishOpen();
    void finishClose();

    SimulationProfile m_profile;
    QTimer* m_openTimer = nullptr;
    QTimer* m_closeTimer = nullptr;
    State m_state = State::Closed;
    double m_jamProbability = 0.0;
    bool m_jamInjected = false;
    QRandomGenerator m_rng;
};

// LED 显示屏：记录最后一条消息，供 UI/日志读取
class SimulatedDisplayBoard final : public IDisplayBoard
{
    Q_OBJECT

public:
    explicit SimulatedDisplayBoard(QObject* parent = nullptr);

    QString message() const override { return m_message; }
    void showMessage(const QString& text) override;
    void clear() override;

private:
    QString m_message;
};

// 票据打印机：同步打印，可注入一次失败
class SimulatedTicketPrinter final : public ITicketPrinter
{
    Q_OBJECT

public:
    explicit SimulatedTicketPrinter(QObject* parent = nullptr);

    void print(const QString& ticket, const QString& plate, qint64 atMs) override;

    QString lastTicket() const { return m_lastTicket; }
    QString lastPlate() const { return m_lastPlate; }
    qint64 lastPrintedAtMs() const { return m_lastPrintedAtMs; }
    int printCount() const { return m_printCount; }

    void failNextPrint(bool fail = true) { m_failNext = fail; }

private:
    QString m_lastTicket;
    QString m_lastPlate;
    qint64 m_lastPrintedAtMs = 0;
    int m_printCount = 0;
    bool m_failNext = false;
};

} // namespace devices
} // namespace pl
