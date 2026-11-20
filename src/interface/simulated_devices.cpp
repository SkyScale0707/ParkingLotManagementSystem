#include "simulated_devices.h"

#include <QtGlobal>

#include <algorithm>

namespace pl {
namespace devices {

// ---------------------------------------------------------------------------
// 地感线圈
// ---------------------------------------------------------------------------
SimulatedLoopDetector::SimulatedLoopDetector(QObject* parent)
    : ILoopDetector(parent)
{
}

bool SimulatedLoopDetector::vehiclePresent() const
{
    return m_present;
}

void SimulatedLoopDetector::setVehiclePresent(bool present)
{
    if (m_present == present)
        return;
    m_present = present;
    if (m_present)
        emit vehicleArrived();
    else
        emit vehicleLeft();
}

// ---------------------------------------------------------------------------
// 车牌识别相机
// ---------------------------------------------------------------------------
SimulatedPlateRecognizer::SimulatedPlateRecognizer(const SimulationProfile& profile, QObject* parent)
    : IPlateRecognizer(parent)
    , m_profile(profile)
    , m_timer(new QTimer(this))
    , m_clock(&m_defaultClock)
    , m_rng(1u)
{
    registerDeviceMetaTypes();
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &SimulatedPlateRecognizer::finishRecognition);
}

void SimulatedPlateRecognizer::setProfile(const SimulationProfile& profile)
{
    m_profile = profile;
}

void SimulatedPlateRecognizer::setSeed(quint32 seed)
{
    m_rng.seed(seed);
}

void SimulatedPlateRecognizer::setClock(IClock* clock)
{
    m_clock = clock ? clock : &m_defaultClock;
}

void SimulatedPlateRecognizer::enqueueRecognition(const QString& plate, double confidence,
                                                  int vehicleCategory)
{
    PendingRecognition pending;
    pending.plate = plate;
    pending.confidence = confidence;
    pending.vehicleCategory = vehicleCategory;
    m_queue.enqueue(pending);
}

void SimulatedPlateRecognizer::clearQueue()
{
    m_queue.clear();
}

void SimulatedPlateRecognizer::injectFailure(DeviceError error, const QString& detail)
{
    m_hasInjectedFailure = true;
    m_injectedError = error;
    m_injectedDetail = detail;
}

void SimulatedPlateRecognizer::setAlwaysFail(bool always, DeviceError error)
{
    m_alwaysFail = always;
    m_injectedError = error;
}

void SimulatedPlateRecognizer::requestRecognition(LaneKind lane)
{
    if (m_busy)
        return;

    m_busy = true;
    m_lane = lane;

    int delay = std::max(0, m_profile.recognitionDelayMs);
    if (m_profile.recognitionJitterMs > 0)
        delay += static_cast<int>(m_rng.bounded(static_cast<quint32>(m_profile.recognitionJitterMs) + 1u));
    m_timer->start(delay);
}

void SimulatedPlateRecognizer::cancel()
{
    m_timer->stop();
    m_busy = false;
}

void SimulatedPlateRecognizer::finishRecognition()
{
    m_busy = false;

    if (m_alwaysFail) {
        emit failed(m_injectedError,
                    m_injectedDetail.isEmpty() ? deviceErrorText(m_injectedError)
                                               : m_injectedDetail);
        return;
    }

    if (m_hasInjectedFailure) {
        m_hasInjectedFailure = false;
        emit failed(m_injectedError,
                    m_injectedDetail.isEmpty() ? deviceErrorText(m_injectedError)
                                               : m_injectedDetail);
        return;
    }

    if (m_profile.recognitionFailureRate > 0.0
        && m_rng.generateDouble() < m_profile.recognitionFailureRate) {
        emit failed(DeviceError::CameraTimeout, QStringLiteral("识别服务无响应"));
        return;
    }

    PendingRecognition pending;
    if (!m_queue.isEmpty()) {
        pending = m_queue.dequeue();
    } else {
        pending.plate = makeDemoPlate(m_rng.generate());
    }

    const QString plate = normalizePlate(pending.plate);
    if (!isPlausiblePlate(plate)) {
        emit failed(DeviceError::InvalidPlate, plate);
        return;
    }

    double confidence = pending.confidence;
    if (confidence < 0.0)
        confidence = 0.85 + 0.14 * m_rng.generateDouble();
    confidence = std::clamp(confidence, 0.0, 1.0);

    PlateReading reading;
    reading.plate = plate;
    reading.confidence = confidence;
    reading.recognizedAtMs = m_clock->nowMs();
    reading.lane = m_lane;
    reading.vehicleCategory = pending.vehicleCategory;
    emit recognized(reading);
}

// ---------------------------------------------------------------------------
// 道闸
// ---------------------------------------------------------------------------
SimulatedBarrier::SimulatedBarrier(const SimulationProfile& profile, QObject* parent)
    : IBarrier(parent)
    , m_profile(profile)
    , m_openTimer(new QTimer(this))
    , m_closeTimer(new QTimer(this))
    , m_rng(2u)
{
    m_openTimer->setSingleShot(true);
    m_closeTimer->setSingleShot(true);
    connect(m_openTimer, &QTimer::timeout, this, &SimulatedBarrier::finishOpen);
    connect(m_closeTimer, &QTimer::timeout, this, &SimulatedBarrier::finishClose);
}

void SimulatedBarrier::setProfile(const SimulationProfile& profile)
{
    m_profile = profile;
}

void SimulatedBarrier::setJamProbability(double probability)
{
    m_jamProbability = std::clamp(probability, 0.0, 1.0);
}

void SimulatedBarrier::injectJam(bool jam)
{
    m_jamInjected = jam;
}

void SimulatedBarrier::setSeed(quint32 seed)
{
    m_rng.seed(seed);
}

void SimulatedBarrier::open()
{
    if (m_state == State::Open)
        return;
    m_closeTimer->stop();
    m_state = State::Opening;

    const int delay = std::max(0, m_profile.barrierOpenMs);
    if (delay == 0) {
        finishOpen();
        return;
    }
    m_openTimer->start(delay);
}

void SimulatedBarrier::finishOpen()
{
    const bool jam = m_jamInjected
        || (m_jamProbability > 0.0 && m_rng.generateDouble() < m_jamProbability);
    m_jamInjected = false;

    if (jam) {
        m_state = State::Jammed;
        emit jammed(QStringLiteral("道闸抬杆卡死"));
        return;
    }

    m_state = State::Open;
    emit opened();
}

void SimulatedBarrier::close()
{
    if (m_state == State::Closed)
        return;
    m_openTimer->stop();
    m_state = State::Closing;

    const int delay = std::max(0, m_profile.barrierCloseMs);
    if (delay == 0) {
        finishClose();
        return;
    }
    m_closeTimer->start(delay);
}

void SimulatedBarrier::finishClose()
{
    m_state = State::Closed;
    emit closed();
}

// ---------------------------------------------------------------------------
// LED 显示屏
// ---------------------------------------------------------------------------
SimulatedDisplayBoard::SimulatedDisplayBoard(QObject* parent)
    : IDisplayBoard(parent)
{
}

void SimulatedDisplayBoard::showMessage(const QString& text)
{
    if (m_message == text)
        return;
    m_message = text;
    emit messageChanged(m_message);
}

void SimulatedDisplayBoard::clear()
{
    showMessage(QString());
}

// ---------------------------------------------------------------------------
// 票据打印机
// ---------------------------------------------------------------------------
SimulatedTicketPrinter::SimulatedTicketPrinter(QObject* parent)
    : ITicketPrinter(parent)
{
}

void SimulatedTicketPrinter::print(const QString& ticket, const QString& plate, qint64 atMs)
{
    if (m_failNext) {
        m_failNext = false;
        emit failed(QStringLiteral("打印机缺纸"));
        return;
    }

    m_lastTicket = ticket;
    m_lastPlate = plate;
    m_lastPrintedAtMs = atMs;
    ++m_printCount;
    emit printed(ticket, plate);
}

} // namespace devices
} // namespace pl
