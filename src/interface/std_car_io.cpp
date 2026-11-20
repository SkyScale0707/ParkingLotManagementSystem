#include "std_car_io.h"

#include <QMetaType>

#include <utility>

namespace pl {
namespace devices {

// ---------------------------------------------------------------------------
// CarIOError
// ---------------------------------------------------------------------------
CarIOError::CarIOError(DeviceError code, QString message, qint64 atMs)
    : m_code(code)
    , m_message(std::move(message))
    , m_atMs(atMs)
{
}

QList<int> CarIOError::timeLog() const
{
    const QDateTime dt = time();
    const QDate date = dt.date();
    const QTime clock = dt.time();
    return {date.year(), date.month(), date.day(), clock.hour(), clock.minute(), clock.second()};
}

QString CarIOError::toString() const
{
    if (!hasError())
        return QStringLiteral("无错误");
    return QStringLiteral("%1（%2）%3")
        .arg(deviceErrorText(m_code), m_message,
             time().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
}

// ---------------------------------------------------------------------------
// CarIOLog
// ---------------------------------------------------------------------------
QString CarIOLog::toString() const
{
    return QStringLiteral("%1 %2 %3 %4 置信度 %5%")
        .arg(time().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
             laneKindText(lane), carIOStateText(state), plate)
        .arg(confidence * 100.0, 0, 'f', 1);
}

// ---------------------------------------------------------------------------
// 入口监控
// ---------------------------------------------------------------------------
StdCarInMonitor::StdCarInMonitor(const LaneController::DeviceSet& devices,
                                 CounterDevice* counter, QObject* parent)
    : QObject(parent)
    , m_counter(counter)
{
    registerDeviceMetaTypes();
    qRegisterMetaType<pl::devices::CarIOLog>("pl::devices::CarIOLog");
    qRegisterMetaType<pl::devices::CarIOError>("pl::devices::CarIOError");

    m_controller = new LaneController(LaneKind::Entry, devices, this);
    m_controller->setRequiresApproval(false);

    connect(m_controller, &LaneController::entered, this, &StdCarInMonitor::handleEntered);
    connect(m_controller, &LaneController::rejected, this, &StdCarInMonitor::handleRejected);
    connect(m_controller, &LaneController::ticketIssued, this,
            [this](const QString& ticket, const PassageEvent&) { emit ticketIssued(ticket); });
}

void StdCarInMonitor::start()
{
    m_controller->start();
}

void StdCarInMonitor::stop()
{
    m_controller->stop();
}

bool StdCarInMonitor::printTicket(const QString& ticket)
{
    const bool ok = m_controller->printTicket(ticket);
    if (ok && !m_logs.isEmpty())
        m_logs.last().ticket = ticket;
    return ok;
}

void StdCarInMonitor::handleEntered(const PassageEvent& event)
{
    CarIOLog log;
    log.plate = event.plate;
    log.lane = LaneKind::Entry;
    log.state = CarIOState::In;
    log.atMs = event.passedAtMs;
    log.confidence = event.confidence;
    log.vehicleCategory = event.vehicleCategory;
    log.ticket = event.ticket;

    m_logs.append(log);
    emit carIn(log);
}

void StdCarInMonitor::handleRejected(DeviceError code, const QString& detail)
{
    emit errorOccurred(CarIOError(code, detail, m_controller->clock()->nowMs()));
}

// ---------------------------------------------------------------------------
// 出口监控
// ---------------------------------------------------------------------------
StdCarOutMonitor::StdCarOutMonitor(const LaneController::DeviceSet& devices,
                                   CounterDevice* counter, bool requireApproval, QObject* parent)
    : QObject(parent)
    , m_counter(counter)
{
    registerDeviceMetaTypes();
    qRegisterMetaType<pl::devices::CarIOLog>("pl::devices::CarIOLog");
    qRegisterMetaType<pl::devices::CarIOError>("pl::devices::CarIOError");

    m_controller = new LaneController(LaneKind::Exit, devices, this);
    m_controller->setRequiresApproval(requireApproval);

    connect(m_controller, &LaneController::exited, this, &StdCarOutMonitor::handleExited);
    connect(m_controller, &LaneController::rejected, this, &StdCarOutMonitor::handleRejected);
    connect(m_controller, &LaneController::waitingForApproval, this,
            &StdCarOutMonitor::waitingForApproval);
}

void StdCarOutMonitor::start()
{
    m_controller->start();
}

void StdCarOutMonitor::stop()
{
    m_controller->stop();
}

bool StdCarOutMonitor::approveExit()
{
    return m_controller->approveExit();
}

bool StdCarOutMonitor::rejectExit(const QString& reason)
{
    return m_controller->rejectExit(reason);
}

void StdCarOutMonitor::handleExited(const PassageEvent& event)
{
    CarIOLog log;
    log.plate = event.plate;
    log.lane = LaneKind::Exit;
    log.state = CarIOState::Out;
    log.atMs = event.passedAtMs;
    log.confidence = event.confidence;
    log.vehicleCategory = event.vehicleCategory;
    log.ticket = event.ticket;

    m_logs.append(log);
    emit carOut(log);
}

void StdCarOutMonitor::handleRejected(DeviceError code, const QString& detail)
{
    emit errorOccurred(CarIOError(code, detail, m_controller->clock()->nowMs()));
}

} // namespace devices
} // namespace pl
