#include "device_types.h"

#include <QChar>
#include <QMetaType>

namespace pl {
namespace devices {
namespace {

const char* const kProvinces[] = {"鄂", "京", "沪", "粤", "苏", "浙", "川", "鲁", "豫", "皖"};

} // namespace

QString laneKindText(LaneKind lane)
{
    return lane == LaneKind::Entry ? QStringLiteral("入口") : QStringLiteral("出口");
}

QString laneStateText(LaneState state)
{
    switch (state) {
    case LaneState::Idle:             return QStringLiteral("空闲");
    case LaneState::VehicleDetected:  return QStringLiteral("检测到车辆");
    case LaneState::Recognizing:      return QStringLiteral("识别中");
    case LaneState::AwaitingApproval: return QStringLiteral("等待确认");
    case LaneState::OpeningBarrier:   return QStringLiteral("道闸开启中");
    case LaneState::VehiclePassing:   return QStringLiteral("车辆通行");
    case LaneState::ClosingBarrier:   return QStringLiteral("道闸关闭中");
    case LaneState::Rejected:         return QStringLiteral("已拒绝");
    case LaneState::Fault:            return QStringLiteral("故障");
    }
    return QStringLiteral("未知");
}

QString deviceErrorText(DeviceError error)
{
    switch (error) {
    case DeviceError::None:           return QStringLiteral("正常");
    case DeviceError::CameraTimeout:  return QStringLiteral("车牌识别超时");
    case DeviceError::LowConfidence:  return QStringLiteral("车牌置信度过低");
    case DeviceError::InvalidPlate:   return QStringLiteral("车牌无效");
    case DeviceError::BarrierJam:     return QStringLiteral("道闸卡死");
    case DeviceError::PassageTimeout: return QStringLiteral("车辆通行超时");
    case DeviceError::Offline:        return QStringLiteral("设备离线");
    case DeviceError::Rejected:       return QStringLiteral("业务拒绝放行");
    case DeviceError::Busy:           return QStringLiteral("车道忙");
    }
    return QStringLiteral("未知错误");
}

QString normalizePlate(const QString& raw)
{
    QString text = raw;

    // 全角字符转半角（含全角空格）
    for (int i = 0; i < text.size(); ++i) {
        const ushort code = text.at(i).unicode();
        if (code >= 0xFF01 && code <= 0xFF5E)
            text[i] = QChar(static_cast<ushort>(code - 0xFEE0));
        else if (code == 0x3000)
            text[i] = QLatin1Char(' ');
    }

    text = text.toUpper();
    text.remove(QLatin1Char(' '));
    text.remove(QLatin1Char('-'));
    text.remove(QLatin1Char('.'));
    text.remove(QChar(0x00B7));   // 间隔号 ·
    return text.trimmed();
}

bool isPlausiblePlate(const QString& plate)
{
    const QString text = normalizePlate(plate);
    if (text.size() < 5 || text.size() > 10)
        return false;
    for (const QChar ch : text) {
        if (!ch.isLetterOrNumber())
            return false;
    }
    return true;
}

QString makeDemoPlate(quint32 seed)
{
    const int provinceIndex = static_cast<int>(seed % 10u);
    const QString province = QString::fromUtf8(kProvinces[provinceIndex]);
    return QStringLiteral("%1A%2")
        .arg(province)
        .arg(static_cast<int>(seed % 100000u), 5, 10, QLatin1Char('0'));
}

void registerDeviceMetaTypes()
{
    static const bool registered = []() {
        qRegisterMetaType<pl::devices::PlateReading>("pl::devices::PlateReading");
        qRegisterMetaType<pl::devices::PassageEvent>("pl::devices::PassageEvent");
        return true;
    }();
    Q_UNUSED(registered)
}

} // namespace devices
} // namespace pl
