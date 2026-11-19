#include "domain/types.h"

#include <QDateTime>

namespace pl {

QList<VehicleType> allTypes()
{
    return {VehicleType::CarSmall, VehicleType::TruckSmall,
            VehicleType::TruckMedium, VehicleType::TruckLarge};
}

QString typeToken(VehicleType t)
{
    switch (t) {
    case VehicleType::CarSmall:    return QStringLiteral("car_small");
    case VehicleType::TruckSmall:  return QStringLiteral("truck_small");
    case VehicleType::TruckMedium: return QStringLiteral("truck_medium");
    case VehicleType::TruckLarge:  return QStringLiteral("truck_large");
    }
    return QStringLiteral("car_small");
}

bool typeFromToken(const QString& s, VehicleType* out)
{
    if (!out)
        return false;
    if (s == QLatin1String("car_small"))    { *out = VehicleType::CarSmall;    return true; }
    if (s == QLatin1String("truck_small"))  { *out = VehicleType::TruckSmall;  return true; }
    if (s == QLatin1String("truck_medium")) { *out = VehicleType::TruckMedium; return true; }
    if (s == QLatin1String("truck_large"))  { *out = VehicleType::TruckLarge;  return true; }
    return false;
}

QString typeLabel(VehicleType t)
{
    switch (t) {
    case VehicleType::CarSmall:    return QStringLiteral("小汽车");
    case VehicleType::TruckSmall:  return QStringLiteral("小卡");
    case VehicleType::TruckMedium: return QStringLiteral("中卡");
    case VehicleType::TruckLarge:  return QStringLiteral("大卡");
    }
    return QStringLiteral("未知");
}

QString typeShortLabel(VehicleType t)
{
    switch (t) {
    case VehicleType::CarSmall:    return QStringLiteral("轿");
    case VehicleType::TruckSmall:  return QStringLiteral("轻");
    case VehicleType::TruckMedium: return QStringLiteral("中");
    case VehicleType::TruckLarge:  return QStringLiteral("重");
    }
    return QStringLiteral("?");
}

qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

} // namespace pl
