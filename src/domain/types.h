#pragma once
// ---------------------------------------------------------------------------
// types.h — 领域层公共类型
//
// 设计要点（见 docs/feasibility-2d-and-multifile-json.md §2.5）：
//   * 金额一律用 int（分），时间一律用 qint64（毫秒），杜绝浮点误差与 TIME_RATE 魔数
//   * 车位占用状态不单独存储，由 ActiveEntry.slot 推导
// ---------------------------------------------------------------------------
#include <QList>
#include <QString>
#include <QtGlobal>

namespace pl {

enum class VehicleType : int {
    CarSmall = 0,
    TruckSmall = 1,
    TruckMedium = 2,
    TruckLarge = 3,
};

QList<VehicleType> allTypes();
QString typeToken(VehicleType t);   // "car_small" 等，写入 JSON
bool typeFromToken(const QString& s, VehicleType* out);
QString typeLabel(VehicleType t);   // "小汽车" 等，界面显示
QString typeShortLabel(VehicleType t);

struct Config {
    int capacity = 50;
    int unitPriceCents = 500;      // ¥5.00 / 计费单位
    int freeMinutes = 15;          // 免费时长
    int billingUnitMinutes = 60;   // 计费单位
    int maxWaiting = 200;          // 等待队列上限

    bool operator==(const Config&) const = default;
    bool operator!=(const Config&) const = default;
};

struct ActiveEntry {
    QString ticket;
    QString plate;
    VehicleType type = VehicleType::CarSmall;
    int slot = -1;
    qint64 enteredAtMs = 0;
};

struct WaitEntry {
    QString ticket;
    QString plate;
    VehicleType type = VehicleType::CarSmall;
    qint64 queuedAtMs = 0;
};

enum class EnterStatus {
    Parked,
    Queued,
    DuplicateActive,
    DuplicateWaiting,
    QueueFull,
    InvalidPlate,
};

enum class ExitStatus {
    Ok,
    NotFound,
    StillWaiting,
};

struct EnterResult {
    EnterStatus status = EnterStatus::QueueFull;
    int slot = -1;
    QString ticket;
    QString errorDetail;  // 持久化失败时的附加说明
};

struct ExitResult {
    ExitStatus status = ExitStatus::NotFound;
    QString ticket;
    QString plate;
    VehicleType type = VehicleType::CarSmall;
    int slot = -1;
    qint64 enteredAtMs = 0;
    qint64 exitedAtMs = 0;
    qint64 stayMs = 0;
    qint64 feeCents = 0;
    bool promoted = false;
    QString promotedTicket;
    QString promotedPlate;
    VehicleType promotedType = VehicleType::CarSmall;
    qint64 promotedEnteredAtMs = 0;
};

qint64 nowMs();

} // namespace pl
