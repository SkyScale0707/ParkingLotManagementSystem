#pragma once
// ---------------------------------------------------------------------------
// billing.h — 计费与格式化（纯函数，便于单元测试）
// ---------------------------------------------------------------------------
#include "domain/types.h"

namespace pl {

// 计费规则：停车时长 ≤ freeMinutes 免费；超出部分按 billingUnitMinutes 向上取整，
// 每个计费单位收取 unitPriceCents。billableMs 返回实际计费时长（不含免费时长）。
qint64 feeCentsFor(qint64 enteredAtMs, qint64 nowMs, const Config& c, qint64* billableMs = nullptr);

QString formatMoney(qint64 cents);          // 500 -> "¥5.00"
QString formatDuration(qint64 ms);          // 3900000 -> "1 小时 05 分"
QString formatDateTime(qint64 ms);          // "2025-11-19 23:00"
QString formatClock(qint64 ms);             // "23:00:05"

} // namespace pl
