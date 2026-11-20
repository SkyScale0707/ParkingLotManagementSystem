#include "lane_controller.h"

#include <QtGlobal>

#include <algorithm>

namespace pl {
namespace devices {

LaneController::LaneController(LaneKind lane, const DeviceSet& devices, QObject* parent)
    : QObject(parent)
    , m_lane(lane)
    , m_dev(devices)
{
    registerDeviceMetaTypes();
    m_clock = &m_defaultClock;

    m_recognitionTimer.setSingleShot(true);
    m_passageTimer.setSingleShot(true);
    connect(&m_recognitionTimer, &QTimer::timeout, this, &LaneController::onRecognitionTimeout);
    connect(&m_passageTimer, &QTimer::timeout, this, &LaneController::onPassageTimeout);

    if (m_dev.loop) {
        connect(m_dev.loop, &ILoopDetector::vehicleArrived, this, &LaneController::onVehicleArrived);
        connect(m_dev.loop, &ILoopDetector::vehicleLeft, this, &LaneController::onVehicleLeft);
    }
    if (m_dev.camera) {
        connect(m_dev.camera, &IPlateRecognizer::recognized, this, &LaneController::onRecognized);
        connect(m_dev.camera, &IPlateRecognizer::failed, this, &LaneController::onRecognitionFailed);
    }
    if (m_dev.barrier) {
        connect(m_dev.barrier, &IBarrier::opened, this, &LaneController::onBarrierOpened);
        connect(m_dev.barrier, &IBarrier::closed, this, &LaneController::onBarrierClosed);
        connect(m_dev.barrier, &IBarrier::jammed, this, &LaneController::onBarrierJammed);
    }
}

LaneController::~LaneController()
{
    // 设备可能先于本对象销毁（QObject 子对象析构顺序），这里只停自己的定时器，
    // 绝不回访 m_dev 里的指针。
    m_running = false;
    m_recognitionTimer.stop();
    m_passageTimer.stop();
}

void LaneController::setMinimumConfidence(double value)
{
    m_minConfidence = std::clamp(value, 0.0, 1.0);
}

void LaneController::setRecognitionTimeoutMs(int ms)
{
    m_recognitionTimeoutMs = std::max(0, ms);
}

void LaneController::setPassageTimeoutMs(int ms)
{
    m_passageTimeoutMs = std::max(0, ms);
}

void LaneController::setRequiresApproval(bool required)
{
    m_requiresApproval = required;
}

void LaneController::setClock(IClock* clock)
{
    m_clock = clock ? clock : &m_defaultClock;
}

void LaneController::start()
{
    if (m_running)
        return;
    m_running = true;
    resetToIdle();

    // 启动时若地感上已经有车，补一次到达事件
    if (m_dev.loop && m_dev.loop->vehiclePresent()) {
        QTimer::singleShot(0, this, [this]() {
            if (m_running && m_state == LaneState::Idle)
                onVehicleArrived();
        });
    }
}

void LaneController::stop()
{
    m_running = false;
    m_recognitionTimer.stop();
    m_passageTimer.stop();
    if (m_dev.camera)
        m_dev.camera->cancel();
    if (m_dev.barrier)
        m_dev.barrier->close();
    if (m_dev.display)
        m_dev.display->clear();
    setState(LaneState::Idle);
}

QString LaneController::idleMessage() const
{
    return m_lane == LaneKind::Entry ? QStringLiteral("入口空闲，等待车辆")
                                     : QStringLiteral("出口空闲，等待车辆");
}

void LaneController::setState(LaneState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(m_state);
}

void LaneController::showMessage(const QString& text)
{
    if (m_dev.display)
        m_dev.display->showMessage(text);
    emit displayChanged(text);
}

void LaneController::resetToIdle()
{
    setState(LaneState::Idle);
    showMessage(idleMessage());
}

void LaneController::onVehicleArrived()
{
    if (!m_running)
        return;
    if (m_state == LaneState::VehiclePassing || m_state == LaneState::OpeningBarrier
        || m_state == LaneState::ClosingBarrier || m_state == LaneState::AwaitingApproval)
        return;
    if (m_state != LaneState::Idle && m_state != LaneState::Rejected
        && m_state != LaneState::Fault)
        return;

    setState(LaneState::VehicleDetected);
    showMessage(QStringLiteral("检测到车辆，正在识别车牌…"));

    if (!m_dev.camera) {
        onRecognitionFailed(DeviceError::Offline, QStringLiteral("相机未接入"));
        return;
    }

    m_dev.camera->requestRecognition(m_lane);
    setState(LaneState::Recognizing);
    m_recognitionTimer.start(m_recognitionTimeoutMs);
}

void LaneController::onRecognized(const PlateReading& reading)
{
    if (!m_running)
        return;
    if (m_state != LaneState::VehicleDetected && m_state != LaneState::Recognizing)
        return;

    m_recognitionTimer.stop();

    PlateReading fixed = reading;
    fixed.lane = m_lane;
    if (fixed.recognizedAtMs <= 0)
        fixed.recognizedAtMs = m_clock->nowMs();

    emit plateRecognized(fixed);

    if (!fixed.isValid() || fixed.confidence < m_minConfidence) {
        const QString detail = QStringLiteral("%1（置信度 %2%）")
                                   .arg(fixed.plate)
                                   .arg(fixed.confidence * 100.0, 0, 'f', 1);
        emit rejected(DeviceError::LowConfidence, detail);
        showMessage(QStringLiteral("车牌识别失败，请重试"));
        setState(LaneState::Rejected);
        return;
    }

    m_reading = fixed;

    if (m_lane == LaneKind::Exit && m_requiresApproval) {
        setState(LaneState::AwaitingApproval);
        showMessage(QStringLiteral("车牌 %1，请缴费确认").arg(fixed.plate));
        emit waitingForApproval(fixed);
        return;
    }

    beginBarrierOpen();
}

void LaneController::onRecognitionFailed(DeviceError error, const QString& detail)
{
    if (!m_running)
        return;

    m_recognitionTimer.stop();
    emit rejected(error, detail.isEmpty() ? deviceErrorText(error) : detail);
    showMessage(QStringLiteral("%1，请重试").arg(deviceErrorText(error)));
    setState(LaneState::Fault);
}

void LaneController::beginBarrierOpen()
{
    if (!m_dev.barrier) {
        // 没有道闸（例如纯识别调试）：直接进入通行状态，等车辆离开地感
        setState(LaneState::VehiclePassing);
        showMessage(QStringLiteral("%1，请通行").arg(m_reading.plate));
        m_passageTimer.start(m_passageTimeoutMs);
        return;
    }

    setState(LaneState::OpeningBarrier);
    showMessage(QStringLiteral("%1 识别成功，道闸开启中…").arg(m_reading.plate));
    m_dev.barrier->open();
}

void LaneController::onBarrierOpened()
{
    if (!m_running || m_state != LaneState::OpeningBarrier)
        return;

    setState(LaneState::VehiclePassing);
    showMessage(QStringLiteral("%1，请通行").arg(m_reading.plate));
    m_passageTimer.start(m_passageTimeoutMs);
}

void LaneController::completePassage()
{
    PassageEvent event;
    event.lane = m_lane;
    event.plate = m_reading.plate;
    event.recognizedAtMs = m_reading.recognizedAtMs;
    event.passedAtMs = m_clock->nowMs();
    if (event.passedAtMs < event.recognizedAtMs)
        event.passedAtMs = event.recognizedAtMs;
    event.confidence = m_reading.confidence;
    event.vehicleCategory = m_reading.vehicleCategory;

    m_lastPassage = event;
    if (m_lane == LaneKind::Entry)
        emit entered(event);
    else
        emit exited(event);
}

void LaneController::onVehicleLeft()
{
    if (!m_running)
        return;

    switch (m_state) {
    case LaneState::VehiclePassing:
        m_passageTimer.stop();
        completePassage();
        setState(LaneState::ClosingBarrier);
        if (m_dev.barrier) {
            showMessage(QStringLiteral("车辆已通过，道闸关闭中…"));
            m_dev.barrier->close();
        } else {
            resetToIdle();
        }
        break;

    case LaneState::OpeningBarrier:
        // 车辆在抬杆过程中倒出
        if (m_dev.barrier)
            m_dev.barrier->close();
        resetToIdle();
        break;

    case LaneState::AwaitingApproval:
    case LaneState::Rejected:
    case LaneState::Fault:
        resetToIdle();
        break;

    case LaneState::VehicleDetected:
    case LaneState::Recognizing:
        m_recognitionTimer.stop();
        if (m_dev.camera)
            m_dev.camera->cancel();
        resetToIdle();
        break;

    case LaneState::Idle:
    case LaneState::ClosingBarrier:
        break;
    }
}

void LaneController::onBarrierClosed()
{
    if (!m_running || m_state != LaneState::ClosingBarrier)
        return;

    if (m_dev.loop && m_dev.loop->vehiclePresent()) {
        emit rejected(DeviceError::BarrierJam, QStringLiteral("道闸关闭时车辆仍在杆下"));
        setState(LaneState::Fault);
        return;
    }
    resetToIdle();
}

void LaneController::onBarrierJammed(const QString& reason)
{
    if (!m_running)
        return;
    emit rejected(DeviceError::BarrierJam,
                  reason.isEmpty() ? deviceErrorText(DeviceError::BarrierJam) : reason);
    showMessage(QStringLiteral("道闸故障，请联系管理员"));
    setState(LaneState::Fault);
}

void LaneController::onRecognitionTimeout()
{
    if (!m_running)
        return;
    if (m_dev.camera)
        m_dev.camera->cancel();
    emit rejected(DeviceError::CameraTimeout, QStringLiteral("车牌识别超时"));
    showMessage(QStringLiteral("车牌识别超时，请重试"));
    setState(LaneState::Fault);
}

void LaneController::onPassageTimeout()
{
    if (!m_running)
        return;
    emit rejected(DeviceError::PassageTimeout, QStringLiteral("车辆通过超时"));
    if (m_dev.barrier)
        m_dev.barrier->close();
    setState(LaneState::ClosingBarrier);
}

bool LaneController::approveExit()
{
    if (m_lane != LaneKind::Exit || m_state != LaneState::AwaitingApproval)
        return false;
    beginBarrierOpen();
    return true;
}

bool LaneController::rejectExit(const QString& reason)
{
    if (m_lane != LaneKind::Exit || m_state != LaneState::AwaitingApproval)
        return false;

    emit rejected(DeviceError::Rejected,
                  reason.isEmpty() ? QStringLiteral("已取消放行") : reason);
    showMessage(QStringLiteral("已取消放行，请倒车"));
    setState(LaneState::Rejected);
    return true;
}

bool LaneController::printTicket(const QString& ticket)
{
    if (!m_dev.printer)
        return false;

    const QString plate =
        m_lastPassage.plate.isEmpty() ? m_reading.plate : m_lastPassage.plate;
    if (plate.isEmpty())
        return false;

    const qint64 atMs = m_lastPassage.passedAtMs > 0 ? m_lastPassage.passedAtMs : m_clock->nowMs();
    m_dev.printer->print(ticket, plate, atMs);

    m_lastPassage.ticket = ticket;
    emit ticketIssued(ticket, m_lastPassage);
    return true;
}

} // namespace devices
} // namespace pl
