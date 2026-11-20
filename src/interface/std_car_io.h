#pragma once
// ---------------------------------------------------------------------------
// std_car_io.h — 进出口车辆实时监控（对外接口）
//
// 对应原草稿 std_car_io.cpp 的功能意图：
//   * error      -> CarIOError（错误码 + 消息 + 发生时间，含年/月/日/时/分/秒）
//   * logs       -> CarIOLog（一条出入记录：车牌、方向、时间、置信度、车型）
//   * std_car_in -> StdCarInMonitor（入口车道 + 通过计数器）
//   * std_car_out-> StdCarOutMonitor（出口车道 + 通过计数器，可等待缴费确认）
//
// 这一层是「设备 → 业务」的适配面：下一轮把监控器的 carIn / carOut 信号
// 接到 AppContext 即可，不需要改设备层。
// ---------------------------------------------------------------------------
#include "devices.h"
#include "extra_equipment_state.h"
#include "lane_controller.h"

#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>

namespace pl {
namespace devices {

class CarIOError
{
public:
    CarIOError() = default;
    CarIOError(DeviceError code, QString message, qint64 atMs);

    bool hasError() const { return m_code != DeviceError::None; }
    DeviceError code() const { return m_code; }
    QString message() const { return m_message; }
    qint64 atMs() const { return m_atMs; }

    QDateTime time() const { return QDateTime::fromMSecsSinceEpoch(m_atMs); }
    // 原草稿的 time_log：年、月、日、时、分、秒
    QList<int> timeLog() const;
    QString toString() const;

private:
    DeviceError m_code = DeviceError::None;
    QString m_message;
    qint64 m_atMs = 0;
};

struct CarIOLog {
    QString plate;
    LaneKind lane = LaneKind::Entry;
    CarIOState state = CarIOState::In;
    qint64 atMs = 0;
    double confidence = 0.0;
    int vehicleCategory = -1;
    QString ticket;

    QDateTime time() const { return QDateTime::fromMSecsSinceEpoch(atMs); }
    QString toString() const;
};

// 入口监控：地感 + 相机 + 道闸 + 显示屏（+ 可选打印机、计数器）
class StdCarInMonitor : public QObject
{
    Q_OBJECT

public:
    explicit StdCarInMonitor(const LaneController::DeviceSet& devices,
                             CounterDevice* counter = nullptr, QObject* parent = nullptr);

    LaneController* controller() const { return m_controller; }
    CounterDevice* counter() const { return m_counter; }
    const QList<CarIOLog>& logs() const { return m_logs; }

    void start();
    void stop();

    bool printTicket(const QString& ticket);

signals:
    void carIn(const pl::devices::CarIOLog& log);
    void errorOccurred(const pl::devices::CarIOError& error);
    void ticketIssued(const QString& ticket);

private:
    void handleEntered(const PassageEvent& event);
    void handleRejected(DeviceError code, const QString& detail);

    LaneController* m_controller = nullptr;
    CounterDevice* m_counter = nullptr;
    QList<CarIOLog> m_logs;
};

// 出口监控：识别后默认需要业务确认（缴费）才抬杆
class StdCarOutMonitor : public QObject
{
    Q_OBJECT

public:
    explicit StdCarOutMonitor(const LaneController::DeviceSet& devices,
                              CounterDevice* counter = nullptr,
                              bool requireApproval = true, QObject* parent = nullptr);

    LaneController* controller() const { return m_controller; }
    CounterDevice* counter() const { return m_counter; }
    const QList<CarIOLog>& logs() const { return m_logs; }

    void start();
    void stop();

    bool approveExit();
    bool rejectExit(const QString& reason = QString());

signals:
    void carOut(const pl::devices::CarIOLog& log);
    void waitingForApproval(const pl::devices::PlateReading& reading);
    void errorOccurred(const pl::devices::CarIOError& error);

private:
    void handleExited(const PassageEvent& event);
    void handleRejected(DeviceError code, const QString& detail);

    LaneController* m_controller = nullptr;
    CounterDevice* m_counter = nullptr;
    QList<CarIOLog> m_logs;
};

} // namespace devices
} // namespace pl

Q_DECLARE_METATYPE(pl::devices::CarIOLog)
Q_DECLARE_METATYPE(pl::devices::CarIOError)
