#pragma once
// ---------------------------------------------------------------------------
// device_types.h — 外设模拟层公共类型
//
// 这一层只模拟「停车场进出口的外部设备」，不依赖领域层与 UI：
//   * 车牌识别相机（IPlateRecognizer）
//   * 地感线圈（ILoopDetector）
//   * 道闸（IBarrier）
//   * LED 显示屏（IDisplayBoard）
//   * 票据打印机（ITicketPrinter）
//
// 时间统一用 qint64 毫秒；车牌统一由 normalizePlate 归一化。
// ---------------------------------------------------------------------------
#include <QMetaType>
#include <QString>
#include <QtGlobal>

namespace pl {
namespace devices {

enum class LaneKind : int {
    Entry = 0,   // 入口
    Exit = 1,    // 出口
};

QString laneKindText(LaneKind lane);

// 单条车道的状态机状态
enum class LaneState : int {
    Idle,              // 空闲，等待车辆
    VehicleDetected,   // 地感刚触发
    Recognizing,       // 车牌识别中
    AwaitingApproval,  // 出口：已识别，等待业务确认/缴费
    OpeningBarrier,    // 道闸开启中
    VehiclePassing,    // 道闸已开，车辆通行
    ClosingBarrier,    // 车辆已过，道闸关闭中
    Rejected,          // 本次通行被拒绝（可等车辆倒出后复位）
    Fault,             // 设备故障（识别超时/道闸卡死等）
};

QString laneStateText(LaneState state);

enum class DeviceError : int {
    None = 0,
    CameraTimeout,     // 相机无响应 / 识别超时
    LowConfidence,     // 置信度低于阈值
    InvalidPlate,      // 车牌不合法或为空
    BarrierJam,        // 道闸卡死
    PassageTimeout,    // 车辆迟迟不离开地感
    Offline,           // 设备离线
    Rejected,          // 业务层拒绝放行
    Busy,              // 同一车道重复触发
};

QString deviceErrorText(DeviceError error);

// 一次车牌识别结果
struct PlateReading {
    QString plate;
    double confidence = 0.0;       // 0.0 ~ 1.0
    qint64 recognizedAtMs = 0;     // 识别完成的时刻（由相机时钟给出）
    LaneKind lane = LaneKind::Entry;
    // 车型识别（部分 ANPR 相机可给出）：0=小汽车 1=小卡 2=中卡 3=大卡，-1=未知
    int vehicleCategory = -1;

    bool isValid() const { return !plate.isEmpty() && confidence > 0.0; }
};

// 一辆车完成一次进/出的事件（供上层业务层消费）
struct PassageEvent {
    LaneKind lane = LaneKind::Entry;
    QString plate;
    qint64 recognizedAtMs = 0;   // 车牌识别完成时刻
    qint64 passedAtMs = 0;       // 车辆离开地感（真正通过）时刻
    double confidence = 0.0;
    int vehicleCategory = -1;
    QString ticket;              // 入口票据号（由业务层回填，可为空）
    bool simulated = true;       // 数据来自模拟设备
};

// 模拟设备的可调参数（真实设备对应固件配置）
struct SimulationProfile {
    int recognitionDelayMs = 350;      // 识别耗时
    int recognitionJitterMs = 150;     // 识别耗时抖动
    double recognitionFailureRate = 0.02;  // 随机识别失败概率
    double minimumConfidence = 0.75;   // 低于此值视为不可用
    int barrierOpenMs = 450;           // 道闸抬杆时间
    int barrierCloseMs = 400;          // 道闸落杆时间
    int vehiclePassMs = 900;           // 模拟车辆从触发地感到离开的时长
};

// 车牌归一化：全角转半角、去空格/分隔符、字母转大写
QString normalizePlate(const QString& raw);

// 合法性检查：5~10 个字符，且只能包含字母、数字或汉字
bool isPlausiblePlate(const QString& plate);

// 确定性生成演示车牌，便于测试与复现：seed 7 -> "鲁A00007"
QString makeDemoPlate(quint32 seed);

// 注册信号参数所需的 Qt 元类型（构造设备时自动调用，可重复调用）
void registerDeviceMetaTypes();

} // namespace devices
} // namespace pl

Q_DECLARE_METATYPE(pl::devices::PlateReading)
Q_DECLARE_METATYPE(pl::devices::PassageEvent)
