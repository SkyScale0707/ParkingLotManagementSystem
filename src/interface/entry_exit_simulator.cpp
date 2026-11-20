#include "entry_exit_simulator.h"

#include <QTimer>

#include <algorithm>

namespace pl {
namespace devices {

EntryExitSimulator::EntryExitSimulator(QObject* parent)
    : EntryExitSimulator(Options{}, parent)
{
}

EntryExitSimulator::EntryExitSimulator(const Options& options, QObject* parent)
    : QObject(parent)
    , m_options(options)
{
    registerDeviceMetaTypes();
    m_clock = &m_defaultClock;
    build(options);
}

EntryExitSimulator::~EntryExitSimulator() = default;

void EntryExitSimulator::build(const Options& options)
{
    // ---- 设备 ----
    m_counter = new CounterDevice(this);

    m_entryLoop = new SimulatedLoopDetector(this);
    m_entryCamera = new SimulatedPlateRecognizer(options.entryProfile, this);
    m_entryCamera->setSeed(options.seed);
    m_entryBarrier = new SimulatedBarrier(options.entryProfile, this);
    m_entryBarrier->setSeed(options.seed + 1u);
    m_entryDisplay = new SimulatedDisplayBoard(this);
    m_entryPrinter = new SimulatedTicketPrinter(this);

    m_exitLoop = new SimulatedLoopDetector(this);
    m_exitCamera = new SimulatedPlateRecognizer(options.exitProfile, this);
    m_exitCamera->setSeed(options.seed + 10u);
    m_exitBarrier = new SimulatedBarrier(options.exitProfile, this);
    m_exitBarrier->setSeed(options.seed + 11u);
    m_exitDisplay = new SimulatedDisplayBoard(this);

    // ---- 监控器 ----
    LaneController::DeviceSet entryDevices;
    entryDevices.loop = m_entryLoop;
    entryDevices.camera = m_entryCamera;
    entryDevices.barrier = m_entryBarrier;
    entryDevices.display = m_entryDisplay;
    entryDevices.printer = m_entryPrinter;
    m_entryMonitor = new StdCarInMonitor(entryDevices, m_counter, this);
    m_entryMonitor->controller()->setMinimumConfidence(options.entryProfile.minimumConfidence);

    LaneController::DeviceSet exitDevices;
    exitDevices.loop = m_exitLoop;
    exitDevices.camera = m_exitCamera;
    exitDevices.barrier = m_exitBarrier;
    exitDevices.display = m_exitDisplay;
    m_exitMonitor = new StdCarOutMonitor(exitDevices, m_counter, options.requireExitApproval, this);
    m_exitMonitor->controller()->setMinimumConfidence(options.exitProfile.minimumConfidence);

    // ---- 信号连线 ----
    connect(m_entryMonitor, &StdCarInMonitor::carIn, this, [this](const CarIOLog&) {
        m_entryBusy = false;
        emit entered(m_entryMonitor->controller()->lastPassage());
    });
    connect(m_exitMonitor, &StdCarOutMonitor::carOut, this, [this](const CarIOLog&) {
        m_exitBusy = false;
        emit exited(m_exitMonitor->controller()->lastPassage());
    });

    connect(m_entryMonitor, &StdCarInMonitor::errorOccurred, this, [this](const CarIOError& error) {
        handleMonitorError(LaneKind::Entry, error);
    });
    connect(m_exitMonitor, &StdCarOutMonitor::errorOccurred, this, [this](const CarIOError& error) {
        handleMonitorError(LaneKind::Exit, error);
    });

    connect(m_exitMonitor, &StdCarOutMonitor::waitingForApproval, this,
            &EntryExitSimulator::waitingForExitApproval);

    connect(m_entryDisplay, &IDisplayBoard::messageChanged, this, [this](const QString& text) {
        emit displayChanged(LaneKind::Entry, text);
    });
    connect(m_exitDisplay, &IDisplayBoard::messageChanged, this, [this](const QString& text) {
        emit displayChanged(LaneKind::Exit, text);
    });

    connect(m_entryMonitor->controller(), &LaneController::stateChanged, this,
            [this](LaneState state) {
                if (state == LaneState::VehiclePassing)
                    schedulePassThrough(LaneKind::Entry);
            });
    connect(m_exitMonitor->controller(), &LaneController::stateChanged, this,
            [this](LaneState state) {
                if (state == LaneState::VehiclePassing)
                    schedulePassThrough(LaneKind::Exit);
            });

    // ---- 时钟与启动 ----
    setClock(m_clock);
    m_entryMonitor->start();
    m_exitMonitor->start();
}

void EntryExitSimulator::setClock(IClock* clock)
{
    m_clock = clock ? clock : &m_defaultClock;
    if (m_entryCamera)
        m_entryCamera->setClock(m_clock);
    if (m_exitCamera)
        m_exitCamera->setClock(m_clock);
    if (m_entryMonitor)
        m_entryMonitor->controller()->setClock(m_clock);
    if (m_exitMonitor)
        m_exitMonitor->controller()->setClock(m_clock);
}

LaneController* EntryExitSimulator::entryController() const
{
    return m_entryMonitor ? m_entryMonitor->controller() : nullptr;
}

LaneController* EntryExitSimulator::exitController() const
{
    return m_exitMonitor ? m_exitMonitor->controller() : nullptr;
}

void EntryExitSimulator::simulateEntry(const QString& plate, double confidence,
                                       int vehicleCategory)
{
    if (m_entryBusy) {
        emit deviceError(DeviceError::Busy, QStringLiteral("入口通道正在处理上一辆车"));
        return;
    }

    m_entryBusy = true;
    ++m_entryGeneration;

    if (m_entryLoop->vehiclePresent())
        m_entryLoop->setVehiclePresent(false);

    m_entryCamera->enqueueRecognition(plate, confidence, vehicleCategory);
    m_entryLoop->setVehiclePresent(true);
}

void EntryExitSimulator::simulateExit(const QString& plate, double confidence,
                                      int vehicleCategory)
{
    if (m_exitBusy) {
        emit deviceError(DeviceError::Busy, QStringLiteral("出口通道正在处理上一辆车"));
        return;
    }

    m_exitBusy = true;
    ++m_exitGeneration;

    if (m_exitLoop->vehiclePresent())
        m_exitLoop->setVehiclePresent(false);

    m_exitCamera->enqueueRecognition(plate, confidence, vehicleCategory);
    m_exitLoop->setVehiclePresent(true);
}

bool EntryExitSimulator::approveExit()
{
    return m_exitMonitor && m_exitMonitor->approveExit();
}

bool EntryExitSimulator::rejectExit(const QString& reason)
{
    return m_exitMonitor && m_exitMonitor->rejectExit(reason);
}

void EntryExitSimulator::schedulePassThrough(LaneKind lane)
{
    const int delayMs = lane == LaneKind::Entry ? m_options.entryProfile.vehiclePassMs
                                                : m_options.exitProfile.vehiclePassMs;
    const int generation = lane == LaneKind::Entry ? m_entryGeneration : m_exitGeneration;

    QTimer::singleShot(std::max(0, delayMs), this, [this, lane, generation]() {
        if (lane == LaneKind::Entry) {
            if (generation != m_entryGeneration || !m_entryBusy)
                return;
            m_entryLoop->setVehiclePresent(false);
        } else {
            if (generation != m_exitGeneration || !m_exitBusy)
                return;
            m_exitLoop->setVehiclePresent(false);
        }
    });
}

void EntryExitSimulator::handleMonitorError(LaneKind lane, const CarIOError& error)
{
    emit deviceError(error.code(), error.message());

    if (lane == LaneKind::Entry) {
        m_entryBusy = false;
        ++m_entryGeneration;   // 作废尚未触发的通过定时器
        QTimer::singleShot(250, this, [this]() {
            if (m_entryLoop && m_entryLoop->vehiclePresent())
                m_entryLoop->setVehiclePresent(false);
        });
    } else {
        m_exitBusy = false;
        ++m_exitGeneration;
        QTimer::singleShot(250, this, [this]() {
            if (m_exitLoop && m_exitLoop->vehiclePresent())
                m_exitLoop->setVehiclePresent(false);
        });
    }
}

} // namespace devices
} // namespace pl
