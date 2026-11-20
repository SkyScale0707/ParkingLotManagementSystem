#include "app/devicebridge.h"

#include "app/appcontext.h"
#include "domain/billing.h"

#include <QRandomGenerator>
#include <QVector>

namespace app {
namespace {

// 出口必须先结算再抬杆
pl::devices::EntryExitSimulator::Options forceExitApproval(
    pl::devices::EntryExitSimulator::Options options)
{
    options.requireExitApproval = true;
    return options;
}

QString enterStatusText(pl::EnterStatus status)
{
    switch (status) {
    case pl::EnterStatus::Parked:           return QStringLiteral("已入位");
    case pl::EnterStatus::Queued:           return QStringLiteral("已排队");
    case pl::EnterStatus::DuplicateActive:  return QStringLiteral("车牌已在场内");
    case pl::EnterStatus::DuplicateWaiting: return QStringLiteral("车牌已在队列中");
    case pl::EnterStatus::QueueFull:        return QStringLiteral("等待队列已满");
    case pl::EnterStatus::InvalidPlate:     return QStringLiteral("车牌无效");
    }
    return QStringLiteral("入场失败");
}

// 默认参数：识别/道闸保留真实感延时，但不做随机失败，避免演示时偶发拒绝
pl::devices::EntryExitSimulator::Options defaultOptions()
{
    pl::devices::EntryExitSimulator::Options options;
    options.entryProfile.recognitionFailureRate = 0.0;
    options.exitProfile.recognitionFailureRate = 0.0;
    return options;
}

} // namespace

DeviceBridge::DeviceBridge(AppContext* context, QObject* parent)
    : DeviceBridge(context, defaultOptions(), parent)
{
}

DeviceBridge::DeviceBridge(AppContext* context,
                           const pl::devices::EntryExitSimulator::Options& options,
                           QObject* parent)
    : QObject(parent)
    , m_ctx(context)
{
    m_simulator = new pl::devices::EntryExitSimulator(forceExitApproval(options), this);
    m_listening = true;
    buildConnections();
}

DeviceBridge::~DeviceBridge() = default;

void DeviceBridge::buildConnections()
{
    using namespace pl::devices;

    connect(m_simulator, &EntryExitSimulator::entered, this, &DeviceBridge::handleEntered);
    connect(m_simulator, &EntryExitSimulator::waitingForExitApproval, this,
            &DeviceBridge::handleWaitingExit);
    connect(m_simulator, &EntryExitSimulator::deviceError, this, &DeviceBridge::handleDeviceError);
    connect(m_simulator, &EntryExitSimulator::displayChanged, this, &DeviceBridge::displayChanged);

    if (auto* entry = m_simulator->entryController()) {
        connect(entry, &LaneController::stateChanged, this, [this](LaneState state) {
            emit laneStateChanged(LaneKind::Entry, state);
        });
    }
    if (auto* exit = m_simulator->exitController()) {
        connect(exit, &LaneController::stateChanged, this, [this](LaneState state) {
            emit laneStateChanged(LaneKind::Exit, state);
        });
    }
}

bool DeviceBridge::isReadOnly() const
{
    return !m_ctx || m_ctx->isReadOnly();
}

void DeviceBridge::setListening(bool listening)
{
    if (m_listening == listening)
        return;
    m_listening = listening;

    if (auto* entry = m_simulator->entryMonitor()) {
        if (m_listening)
            entry->start();
        else
            entry->stop();
    }
    if (auto* exit = m_simulator->exitMonitor()) {
        if (m_listening)
            exit->start();
        else
            exit->stop();
    }
    emit listeningChanged(m_listening);
}

QString DeviceBridge::nextEntryPlate() const
{
    const pl::Lot& lot = m_ctx->lot();
    for (int attempt = 0; attempt < 50; ++attempt) {
        const QString plate =
            pl::devices::makeDemoPlate(QRandomGenerator::global()->generate());
        if (!lot.hasPlate(plate) && lot.waitingIndex(plate) < 0)
            return plate;
    }
    return pl::devices::makeDemoPlate(QRandomGenerator::global()->generate());
}

pl::VehicleType DeviceBridge::mapCategory(int vehicleCategory)
{
    if (vehicleCategory >= 0 && vehicleCategory <= 3)
        return static_cast<pl::VehicleType>(vehicleCategory);
    return pl::VehicleType::CarSmall;
}

void DeviceBridge::requestEntry(const QString& plate, int vehicleCategory)
{
    if (!m_listening) {
        emit autoError(QStringLiteral("自动识别已停止，无法模拟入口来车"));
        return;
    }
    if (isReadOnly()) {
        emit autoError(QStringLiteral("只读模式，无法自动入场"));
        return;
    }

    QString normalized = plate.trimmed();
    normalized = normalized.isEmpty() ? nextEntryPlate()
                                      : pl::devices::normalizePlate(normalized);
    if (!pl::devices::isPlausiblePlate(normalized)) {
        emit autoRejected(normalized, QStringLiteral("车牌格式无效"));
        return;
    }
    if (m_ctx->lot().hasPlate(normalized) || m_ctx->lot().waitingIndex(normalized) >= 0) {
        emit autoRejected(normalized, QStringLiteral("车牌已在场内或队列中"));
        return;
    }
    if (m_ctx->lot().freeSlots.empty()
        && static_cast<int>(m_ctx->lot().waiting.size()) >= m_ctx->lot().cfg.maxWaiting) {
        emit autoRejected(normalized, QStringLiteral("等待队列已满"));
        return;
    }

    m_simulator->simulateEntry(normalized, -1.0, vehicleCategory);
}

void DeviceBridge::requestExit(const QString& plate)
{
    if (!m_listening) {
        emit autoError(QStringLiteral("自动识别已停止，无法模拟出口来车"));
        return;
    }
    if (isReadOnly()) {
        emit autoError(QStringLiteral("只读模式，无法自动出场"));
        return;
    }

    QString normalized = pl::devices::normalizePlate(plate);
    if (normalized.isEmpty()) {
        const QList<pl::ActiveEntry> parked = m_ctx->lot().parkedBySlot();
        if (parked.isEmpty()) {
            emit autoError(QStringLiteral("场内没有可出场的车辆"));
            return;
        }
        const int index =
            static_cast<int>(QRandomGenerator::global()->bounded(static_cast<int>(parked.size())));
        normalized = parked.at(index).plate;
    }
    if (!pl::devices::isPlausiblePlate(normalized)) {
        emit autoRejected(normalized, QStringLiteral("车牌格式无效"));
        return;
    }

    m_simulator->simulateExit(normalized, -1.0, -1);
}

void DeviceBridge::handleEntered(const pl::devices::PassageEvent& event)
{
    if (isReadOnly()) {
        emit autoError(QStringLiteral("只读模式，自动入场被忽略"));
        return;
    }

    QString error;
    const pl::EnterResult result =
        m_ctx->enterVehicle(event.plate, mapCategory(event.vehicleCategory), &error);
    if (!error.isEmpty()) {
        emit autoError(QStringLiteral("入场写盘失败：%1").arg(error));
        return;
    }

    if (result.status == pl::EnterStatus::Parked) {
        if (auto* monitor = m_simulator->entryMonitor())
            monitor->printTicket(result.ticket);
        emit autoEntered(event.plate, result.ticket,
                         QStringLiteral("已入位 %1").arg(m_ctx->lot().slotLabel(result.slot)));
        return;
    }

    if (result.status == pl::EnterStatus::Queued) {
        const int position = m_ctx->lot().waitingIndex(event.plate);
        if (auto* monitor = m_simulator->entryMonitor())
            monitor->printTicket(result.ticket);
        emit autoEntered(event.plate, result.ticket,
                         QStringLiteral("车位已满，排队第 %1 位").arg(position + 1));
        return;
    }

    emit autoRejected(event.plate, enterStatusText(result.status));
}

void DeviceBridge::handleWaitingExit(const pl::devices::PlateReading& reading)
{
    if (isReadOnly()) {
        m_suppressRejectedError = true;
        m_simulator->rejectExit(QStringLiteral("只读模式"));
        emit autoError(QStringLiteral("只读模式，自动出场被拒绝"));
        return;
    }

    if (!m_ctx->lot().hasPlate(reading.plate)) {
        m_suppressRejectedError = true;
        m_simulator->rejectExit(QStringLiteral("车辆不在场内"));
        emit autoRejected(reading.plate, QStringLiteral("车辆不在场内，无法结算"));
        return;
    }

    QString error;
    const pl::ExitResult result = m_ctx->leaveVehicle(reading.plate, &error);
    if (!error.isEmpty()) {
        m_suppressRejectedError = true;
        m_simulator->rejectExit(QStringLiteral("结算写盘失败"));
        emit autoError(QStringLiteral("出场写盘失败：%1").arg(error));
        return;
    }
    if (result.status != pl::ExitStatus::Ok) {
        m_suppressRejectedError = true;
        m_simulator->rejectExit(QStringLiteral("结算失败"));
        emit autoRejected(reading.plate, QStringLiteral("结算失败，无法放行"));
        return;
    }

    QString detail = QStringLiteral("实收 %1").arg(pl::formatMoney(result.feeCents));
    if (result.promoted) {
        detail += QStringLiteral("；队首 %1 已补位到 %2")
                      .arg(result.promotedPlate, m_ctx->lot().slotLabel(result.slot));
    }

    if (!m_simulator->approveExit()) {
        emit autoError(QStringLiteral("出口道闸开启失败"));
        return;
    }

    emit autoExited(reading.plate, result.feeCents, detail);
}

void DeviceBridge::handleDeviceError(pl::devices::DeviceError error, const QString& detail)
{
    if (m_suppressRejectedError && error == pl::devices::DeviceError::Rejected) {
        m_suppressRejectedError = false;
        return;
    }
    m_suppressRejectedError = false;

    emit autoError(QStringLiteral("%1：%2")
                       .arg(pl::devices::deviceErrorText(error),
                            detail.isEmpty() ? QStringLiteral("无详细信息") : detail));
}

} // namespace app
