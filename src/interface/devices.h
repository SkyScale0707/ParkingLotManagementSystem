#pragma once
// ---------------------------------------------------------------------------
// devices.h — 外部设备抽象接口
//
// 真实硬件与模拟实现都实现这些接口；LaneController 只依赖接口，
// 因此下一轮既可以接真机（串口/网口/HTTP 网关），也可以继续用模拟设备。
//
// 注意：这些接口刻意保持「薄」——设备只上报事实（识别到什么、杆开/关、
// 地感有/无车），业务判断（计费、是否放行、入库/出场）不在这里做。
// ---------------------------------------------------------------------------
#include "device_types.h"

#include <QObject>
#include <QString>

namespace pl {
namespace devices {

// 地感线圈：车辆到达 / 离开
class ILoopDetector : public QObject
{
    Q_OBJECT

public:
    explicit ILoopDetector(QObject* parent = nullptr)
        : QObject(parent)
    {
    }
    ~ILoopDetector() override = default;

    virtual bool vehiclePresent() const = 0;
    virtual void setVehiclePresent(bool present) = 0;

signals:
    void vehicleArrived();
    void vehicleLeft();
};

// 车牌识别相机
class IPlateRecognizer : public QObject
{
    Q_OBJECT

public:
    explicit IPlateRecognizer(QObject* parent = nullptr)
        : QObject(parent)
    {
    }
    ~IPlateRecognizer() override = default;

    virtual void requestRecognition(LaneKind lane) = 0;
    virtual void cancel() = 0;
    virtual bool busy() const = 0;

signals:
    void recognized(const pl::devices::PlateReading& reading);
    void failed(pl::devices::DeviceError error, const QString& detail);
};

// 道闸
class IBarrier : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Closed,
        Opening,
        Open,
        Closing,
        Jammed,
    };

    explicit IBarrier(QObject* parent = nullptr)
        : QObject(parent)
    {
    }
    ~IBarrier() override = default;

    virtual State state() const = 0;
    virtual bool isOpen() const { return state() == State::Open; }
    virtual void open() = 0;
    virtual void close() = 0;

signals:
    void opened();
    void closed();
    void jammed(const QString& reason);
};

// LED 显示屏
class IDisplayBoard : public QObject
{
    Q_OBJECT

public:
    explicit IDisplayBoard(QObject* parent = nullptr)
        : QObject(parent)
    {
    }
    ~IDisplayBoard() override = default;

    virtual QString message() const = 0;
    virtual void showMessage(const QString& text) = 0;
    virtual void clear() = 0;

signals:
    void messageChanged(const QString& text);
};

// 入口票据打印机
class ITicketPrinter : public QObject
{
    Q_OBJECT

public:
    explicit ITicketPrinter(QObject* parent = nullptr)
        : QObject(parent)
    {
    }
    ~ITicketPrinter() override = default;

    virtual void print(const QString& ticket, const QString& plate, qint64 atMs) = 0;

signals:
    void printed(const QString& ticket, const QString& plate);
    void failed(const QString& reason);
};

} // namespace devices
} // namespace pl
