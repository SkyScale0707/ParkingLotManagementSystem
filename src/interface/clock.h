#pragma once
// ---------------------------------------------------------------------------
// clock.h — 可注入时钟
//
// 设备与车道控制器只通过 IClock 取时间：
//   * 正式运行用 SystemClock（系统毫秒时间戳）
//   * 测试/回放用 ManualClock（可固定、可推进），保证时间相关断言可复现
// ---------------------------------------------------------------------------
#include <QtGlobal>

namespace pl {
namespace devices {

class IClock
{
public:
    virtual ~IClock() = default;
    virtual qint64 nowMs() const = 0;
};

class SystemClock final : public IClock
{
public:
    qint64 nowMs() const override;
};

class ManualClock final : public IClock
{
public:
    explicit ManualClock(qint64 startMs = 1'700'000'000'000);

    qint64 nowMs() const override;
    void setNowMs(qint64 nowMs);
    void advanceMs(qint64 deltaMs);

private:
    qint64 m_nowMs = 0;
};

} // namespace devices
} // namespace pl
