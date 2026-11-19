// ---------------------------------------------------------------------------
// main.cpp — 可行性验证场景
//
//   S1 多文件快照往返一致
//   S2 仅 journal（未压缩）→ 崩溃后重放恢复
//   S3 journal 残尾（写了一半）→ 丢弃残行，状态仍自洽
//   S4 快照全丢（CURRENT + gen-*）→ 从 journal 全量重建
//   S5 写放大对比：原方案「每次操作全量重写文本」 vs 新方案「追加一行」
//   S6 生成真实数据集，供 2D 视图渲染验证
// ---------------------------------------------------------------------------
#include "parking.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace pl;

namespace {

struct Report {
    std::string md;
    int checks = 0;
    int failures = 0;

    void line(const std::string& s) {
        md += s;
        md += "\n";
        std::printf("%s\n", s.c_str());
    }
    void check(bool ok, const std::string& name, const std::string& detail = std::string()) {
        ++checks;
        if (!ok) ++failures;
        line(std::string(ok ? "[PASS] " : "[FAIL] ") + name + (detail.empty() ? "" : "  —  " + detail));
    }
    void h2(const std::string& s) { line(""); line("## " + s); }
};

double benchMs(const std::function<void()>& fn) {
    const auto a = std::chrono::steady_clock::now();
    fn();
    const auto b = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(b - a).count();
}

int64_t fileSize(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec) ? static_cast<int64_t>(fs::file_size(p, ec)) : 0;
}

void listTree(const fs::path& root, const fs::path& dir, Report& r) {
    std::error_code ec;
    std::vector<fs::path> files;
    for (const auto& de : fs::recursive_directory_iterator(dir, ec)) {
        if (de.is_regular_file(ec)) files.push_back(de.path());
    }
    std::sort(files.begin(), files.end());
    for (const auto& f : files) {
        std::string rel = fs::relative(f, root, ec).string();
        std::replace(rel.begin(), rel.end(), '\\', '/');
        r.line("| `" + rel + "` | " + std::to_string(fileSize(f)) + " |");
    }
}

Config cfgOf(int capacity, int maxWaiting = 200) {
    Config c;
    c.capacity = capacity;
    c.maxWaiting = maxWaiting;
    c.unitPriceCents = 500;
    c.freeMinutes = 15;
    c.billingUnitMinutes = 60;
    return c;
}

void enterMany(Lot& lot, LotStore& store, int startIdx, int count, int64_t t0, int64_t stepMs) {
    for (int i = 0; i < count; ++i) {
        const int idx = startIdx + i;
        const int64_t at = t0 + static_cast<int64_t>(i) * stepMs;
        const auto type = static_cast<VehicleType>(idx % 4);
        const std::string plate = makePlate(idx);
        const auto e = lot.enter(plate, type, at);
        std::string err;
        if (!store.recordEnter(e, plate, type, at, err) && !err.empty()) {
            std::printf("recordEnter(%s) 失败: %s\n", plate.c_str(), err.c_str());
        }
    }
}

void leaveMany(Lot& lot, LotStore& store, int startIdx, int count, int64_t t0, int64_t stepMs) {
    for (int i = 0; i < count; ++i) {
        const int idx = startIdx + i;
        const int64_t at = t0 + static_cast<int64_t>(i) * stepMs;
        const auto r = lot.leave(makePlate(idx), at);
        std::string err;
        if (!store.recordExit(r, err) && !err.empty()) {
            std::printf("recordExit(%s) 失败: %s\n", makePlate(idx).c_str(), err.c_str());
        }
    }
}

// 原项目的「每次操作全量重写文本文件」格式（用于写放大对比）
std::string legacySerialize(const Lot& lot) {
    std::string out;
    out += std::to_string(lot.cfg.capacity) + "\n";
    out += std::to_string(lot.occupyCount()) + "\n";
    out += std::to_string(static_cast<int>(lot.waiting.size())) + "\n";
    out += std::to_string(lot.totalEntered) + "\n";
    out += "100\n";
    out += "PARKED_VEHICLES_START\n";
    for (int i = 0; i < lot.cfg.capacity; ++i) {
        if (i >= static_cast<int>(lot.slotPlate.size())) break;
        const std::string& plate = lot.slotPlate[static_cast<size_t>(i)];
        if (plate.empty()) continue;
        auto it = lot.active.find(plate);
        if (it == lot.active.end()) continue;
        out += plate + "\n";
        out += std::to_string(static_cast<int>(it->second.type)) + "\n";
        out += std::to_string(it->second.enteredAtMs) + "\n";
        out += std::to_string(it->second.slot) + "\n";
    }
    out += "PARKED_VEHICLES_END\n";
    out += "WAITING_VEHICLES_START\n";
    for (const auto& w : lot.waiting) {
        out += w.plate + "\n";
        out += std::to_string(static_cast<int>(w.type)) + "\n";
        out += std::to_string(w.queuedAtMs) + "\n";
    }
    out += "WAITING_VEHICLES_END\n";
    return out;
}

bool legacyWrite(const fs::path& p, const std::string& text) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
    f.flush();
    return static_cast<bool>(f);
}

bool truncateTail(const fs::path& p, int cutBytes) {
    std::error_code ec;
    const auto sz = fs::file_size(p, ec);
    if (ec || sz <= static_cast<uintmax_t>(cutBytes)) return false;
    fs::resize_file(p, sz - static_cast<uintmax_t>(cutBytes), ec);
    return !ec;
}

} // namespace

int main() {
    const fs::path root = fs::path("out");
    std::error_code ec;
    fs::create_directories(root, ec);

    Report r;
    r.line("# 多文件 JSON 存储 + 2D 布局 可行性验证报告");
    r.line("");
    r.line("由 `prototype/` 下的 C++ 原型实际运行生成（MSVC 19.44 / C++20）。");

    const int64_t T0 = 1763000000000;   // 固定基准时间，保证可复现

    // ---------------------------------------------------------------- S1
    r.h2("S1 多文件快照往返一致");
    {
        const fs::path dir = root / "s1-roundtrip";
        fs::remove_all(dir, ec);

        Lot lot;
        LotStore store(dir);
        const auto rep = store.open(lot, cfgOf(50));
        r.check(rep.ok, "打开全新库", rep.error);

        enterMany(lot, store, 0, 60, T0, 60000);
        r.check(lot.occupyCount() == 50 && lot.waiting.size() == 10,
                "60 辆入场 → 50 场内 + 10 排队",
                "场内=" + std::to_string(lot.occupyCount()) + " 排队=" + std::to_string(lot.waiting.size()));

        leaveMany(lot, store, 0, 5, T0 + 3 * 3600000, 30000);
        r.check(lot.occupyCount() == 50 && lot.waiting.size() == 5,
                "5 辆出场 → 队首自动补位 5 次",
                "场内=" + std::to_string(lot.occupyCount()) + " 排队=" + std::to_string(lot.waiting.size()));

        std::string err;
        const bool compacted = store.compact(lot, err);
        r.check(compacted, "压缩为第 1 代快照", err);

        const std::string sig1 = signature(lot);

        Lot lot2;
        LotStore store2(dir);
        const auto rep2 = store2.open(lot2);
        const std::string sig2 = signature(lot2);
        r.check(rep2.ok, "重新打开库", rep2.error);
        r.check(sig1 == sig2, "往返状态指纹一致",
                "len=" + std::to_string(sig1.size()) + " / " + std::to_string(sig2.size()));
        r.check(rep2.replayedOps == 0, "压缩后无残留日志可重放",
                "replayed=" + std::to_string(rep2.replayedOps));
    }

    // ---------------------------------------------------------------- S1b
    r.h2("S1b 计费口径：排队时间不计费（决策 2）");
    {
        const fs::path dir = root / "s1b-billing";
        fs::remove_all(dir, ec);

        Config c = cfgOf(1);
        c.unitPriceCents = 500;     // ¥5 / 小时
        c.freeMinutes = 15;
        c.billingUnitMinutes = 60;

        Lot lot;
        LotStore store(dir);
        const auto rep = store.open(lot, c);
        r.check(rep.ok, "打开 1 位库", rep.error);

        const auto enterA = lot.enter("鄂A00001", VehicleType::CarSmall, T0);
        r.check(enterA.status == EnterStatus::Parked, "A 车入位");
        std::string err;
        store.recordEnter(enterA, "鄂A00001", VehicleType::CarSmall, T0, err);

        const auto enterB = lot.enter("鄂A00002", VehicleType::CarSmall, T0 + 60000);
        r.check(enterB.status == EnterStatus::Queued, "B 车于 1 分钟后到达 → 排队");
        store.recordEnter(enterB, "鄂A00002", VehicleType::CarSmall, T0 + 60000, err);

        // A 停 2 小时后离场 → B 补位，计费起点应重置为补位时刻
        const auto leaveA = lot.leave("鄂A00001", T0 + 2 * 3600000);
        store.recordExit(leaveA, err);
        r.check(leaveA.status == ExitStatus::Ok && leaveA.promoted, "A 离场，B 补位");

        // B 入位后再停 1 小时离场
        const auto leaveB = lot.leave("鄂A00002", T0 + 3 * 3600000);
        store.recordExit(leaveB, err);
        r.check(leaveB.status == ExitStatus::Ok, "B 离场");
        r.check(leaveB.feeCents == 500, "B 只按入位后 1 小时计费（排队 2 小时不计）",
                "fee=" + std::to_string(leaveB.feeCents) + " 分, 入位后在场=" +
                    std::to_string(leaveB.stayMs / 60000) + " 分钟");

        // 崩溃重启后，重放必须得到同一个金额与同一个计费起点
        Lot lot2;
        LotStore store2(dir);
        const auto rep2 = store2.open(lot2);
        r.check(rep2.ok && rep2.replayedOps == 4, "重放 4 条操作", rep2.error);
        r.check(lot2.totalExited == 2 && lot2.revenueCents == leaveA.feeCents + leaveB.feeCents,
                "重放后收入与退出次数一致",
                "revenue=" + std::to_string(lot2.revenueCents));
    }

    // ---------------------------------------------------------------- S2
    r.h2("S2 仅追加日志（不压缩）→ 崩溃后重放恢复");
    std::string sigS2;
    const fs::path dirS2 = root / "s2-replay";
    {
        fs::remove_all(dirS2, ec);
        Lot lot;
        LotStore store(dirS2);
        const auto rep = store.open(lot, cfgOf(50));
        r.check(rep.ok, "打开库", rep.error);

        enterMany(lot, store, 0, 120, T0, 60000);
        leaveMany(lot, store, 0, 30, T0 + 5 * 3600000, 20000);
        sigS2 = signature(lot);

        // 进程「崩溃」：不调用 compact，直接重新打开
        Lot lot2;
        LotStore store2(dirS2);
        const auto rep2 = store2.open(lot2);
        const std::string sig = signature(lot2);
        r.check(rep2.ok, "重新打开（模拟崩溃重启）", rep2.error);
        r.check(sig == sigS2, "重放后状态与原进程一致",
                "replayed=" + std::to_string(rep2.replayedOps));
        r.check(rep2.replayedOps == 150, "重放操作数 = 120 入场 + 30 出场",
                std::to_string(rep2.replayedOps));
        r.check(rep2.tornLines == 0, "无残尾");
    }

    // ---------------------------------------------------------------- S3
    r.h2("S3 日志残尾（最后一行写了一半）");
    {
        const fs::path dir = root / "s3-torn";
        fs::remove_all(dir, ec);
        fs::copy(dirS2, dir, fs::copy_options::recursive, ec);
        const bool cut = truncateTail(dir / "journal.ndjson", 17);
        r.check(cut, "人为截断日志最后 17 字节");

        Lot lot;
        LotStore store(dir);
        const auto rep = store.open(lot);
        r.check(rep.ok, "带残尾仍能打开", rep.error);
        r.check(rep.tornLines == 1, "识别出 1 条残尾并丢弃",
                "torn=" + std::to_string(rep.tornLines));
        r.check(rep.replayedOps == 149, "仅少应用最后 1 条操作",
                "replayed=" + std::to_string(rep.replayedOps));
        std::string why;
        r.check(lot.validate(why), "残留状态仍满足全部不变量", why);
        r.check(lot.totalEntered + lot.totalExited == 149, "计数器与已应用操作数自洽",
                "entered=" + std::to_string(lot.totalEntered) + " exited=" + std::to_string(lot.totalExited));
    }

    // ---------------------------------------------------------------- S4
    r.h2("S4 快照全丢 → 从日志重建");
    {
        const fs::path dir = root / "s4-rebuild";
        fs::remove_all(dir, ec);
        fs::copy(dirS2, dir, fs::copy_options::recursive, ec);
        fs::remove(dir / "CURRENT", ec);
        for (const auto& de : fs::directory_iterator(dir, ec)) {
            if (de.path().filename().string().rfind("gen-", 0) == 0) fs::remove_all(de.path(), ec);
        }

        Lot lot;
        LotStore store(dir);
        const auto rep = store.open(lot);
        const std::string sig = signature(lot);
        r.check(rep.ok, "无快照也能打开", rep.error);
        r.check(sig == sigS2, "从日志全量重建后状态一致",
                "replayed=" + std::to_string(rep.replayedOps));
    }

    // ---------------------------------------------------------------- S5
    r.h2("S5 写放大对比：全量重写 vs 追加一行");
    {
        const fs::path dir = root / "s5-perf";
        fs::remove_all(dir, ec);
        fs::create_directories(dir, ec);

        Lot lot;
        LotStore store(dir);
        const auto rep = store.open(lot, cfgOf(200));
        r.check(rep.ok, "打开 200 位库", rep.error);
        enterMany(lot, store, 0, 180, T0, 60000);

        const std::string legacyText = legacySerialize(lot);
        const fs::path legacyPath = dir / "legacy_parking_data.txt";

        const int N = 1000;
        double legacyMs = benchMs([&] {
            for (int i = 0; i < N; ++i) legacyWrite(legacyPath, legacyText);
        });

        std::string jerr;
        double journalMs = benchMs([&] {
            for (int i = 0; i < N; ++i) store.recordPrice(500 + (i % 7), T0 + i, jerr);
        });
        const auto st = store.stats();

        // 变体：日志流常开 + 每条 flush（正式实现应采用的方式，避免每次 open/close）
        jx::Value sample = jx::Value::object();
        sample.set("seq", jx::Value::integer(1));
        sample.set("op", jx::Value::str("exit"));
        sample.set("ticket", jx::Value::str("T-000123"));
        sample.set("plate", jx::Value::str("鄂A00123"));
        sample.set("type", jx::Value::str("truck_medium"));
        sample.set("slot", jx::Value::integer(47));
        sample.set("enteredAtMs", jx::Value::integer(T0));
        sample.set("stayMs", jx::Value::integer(3600000));
        sample.set("feeCents", jx::Value::integer(1500));
        sample.set("at", jx::Value::integer(T0 + 3600000));
        const std::string sampleLine = jx::dump(sample, false) + "\n";

        double streamMs = 0.0;
        {
            std::ofstream jf(dir / "bench_stream.ndjson", std::ios::binary | std::ios::trunc);
            streamMs = benchMs([&] {
                for (int i = 0; i < N; ++i) {
                    jf.write(sampleLine.data(), static_cast<std::streamsize>(sampleLine.size()));
                    jf.flush();
                }
            });
        }

        const auto legacyBytes = static_cast<double>(fileSize(legacyPath));
        r.line("| 指标 | 原方案（每次全量重写） | 新方案（每次开/关日志） | 新方案（日志流常开） |");
        r.line("|---|---|---|---|");
        r.line("| 单次写入字节 | " + std::to_string(static_cast<long long>(legacyBytes)) + " | " +
               std::to_string(N > 0 ? static_cast<long long>(st.journalBytes / N) : 0) + " | " +
               std::to_string(sampleLine.size()) + " |");
        r.line("| " + std::to_string(N) + " 次耗时 | " + std::to_string(legacyMs) + " ms | " + std::to_string(journalMs) + " ms | " +
               std::to_string(streamMs) + " ms |");
        r.line("| 单次耗时 | " + std::to_string(legacyMs * 1000.0 / N) + " µs | " + std::to_string(journalMs * 1000.0 / N) +
               " µs | " + std::to_string(streamMs * 1000.0 / N) + " µs |");
        r.line("| 1000 次累计写入 | " + std::to_string(static_cast<long long>(legacyBytes * N)) + " B | " +
               std::to_string(static_cast<long long>(st.journalBytes)) + " B | " +
               std::to_string(static_cast<long long>(sampleLine.size() * N)) + " B |");
        r.check(streamMs < legacyMs, "常开日志流显著快于全量重写",
                "x" + std::to_string(legacyMs / (streamMs > 0 ? streamMs : 1.0)) + " 倍");
        r.check(journalMs < legacyMs, "即便每次开关文件也不慢于全量重写",
                "x" + std::to_string(legacyMs / (journalMs > 0 ? journalMs : 1.0)) + " 倍");

        // 启动/恢复耗时：纯快照加载 vs 快照 + 重放 1180 条日志
        const int R = 20;
        const double snapMs = benchMs([&] {
            for (int i = 0; i < R; ++i) {
                Lot l;
                LotStore s(root / "s1-roundtrip");
                const auto rr = s.open(l);
                (void)rr;
            }
        }) / R;
        const double replayMs = benchMs([&] {
            for (int i = 0; i < R; ++i) {
                Lot l;
                LotStore s(dir);
                const auto rr = s.open(l);
                (void)rr;
            }
        }) / R;
        r.line("");
        r.line("| 启动耗时 | 纯快照（120 位/0 条日志） | 快照 + 重放 1180 条日志 |");
        r.line("|---|---|---|");
        r.line("| 单次打开 | " + std::to_string(snapMs) + " ms | " + std::to_string(replayMs) + " ms |");
    }

    // ---------------------------------------------------------------- S6
    r.h2("S6 生成 2D 视图数据集");
    {
        // A：未满场（有绿色空位）
        const fs::path dirA = root / "lot-a";
        fs::remove_all(dirA, ec);
        {
            Lot lot;
            LotStore store(dirA);
            const auto rep = store.open(lot, cfgOf(120));
            r.check(rep.ok, "A 库打开", rep.error);
            enterMany(lot, store, 0, 130, T0, 60000);          // 120 场内 + 10 排队
            leaveMany(lot, store, 0, 10, T0 + 3 * 3600000, 60000);   // 10 次补位
            leaveMany(lot, store, 10, 16, T0 + 5 * 3600000, 60000);  // 再走 16 辆，产生空位
            std::string err;
            r.check(store.compact(lot, err), "A 库压缩", err);
            r.line("A 库：容量 120，场内 " + std::to_string(lot.occupyCount()) +
                   "，空余 " + std::to_string(lot.emptyCount()));
        }

        // B：满场 + 等待队列
        const fs::path dirB = root / "lot-b";
        fs::remove_all(dirB, ec);
        {
            Lot lot;
            LotStore store(dirB);
            const auto rep = store.open(lot, cfgOf(60, 100));
            r.check(rep.ok, "B 库打开", rep.error);
            enterMany(lot, store, 0, 60, T0, 60000);
            enterMany(lot, store, 1000, 12, T0 + 3600000, 60000);
            leaveMany(lot, store, 0, 4, T0 + 6 * 3600000, 60000);
            std::string err;
            r.check(store.compact(lot, err), "B 库压缩", err);
            r.line("B 库：容量 60，场内 " + std::to_string(lot.occupyCount()) +
                   "，排队 " + std::to_string(lot.waiting.size()));
        }

        r.line("");
        r.line("磁盘布局（A 库）：");
        r.line("");
        r.line("| 文件 | 字节 |");
        r.line("|---|---|");
        listTree(root, dirA, r);
    }

    // ---------------------------------------------------------------- 汇总
    r.h2("汇总");
    r.line("- 检查项：" + std::to_string(r.checks) + "，失败：" + std::to_string(r.failures));
    r.line(r.failures == 0 ? "- 结论：**全部通过**，多文件 JSON 方案在 C++/Windows 上可行。"
                           : "- 结论：**存在失败项**，需要修正设计。");

    {
        std::ofstream f(root / "report.md", std::ios::binary | std::ios::trunc);
        f.write(r.md.data(), static_cast<std::streamsize>(r.md.size()));
    }
    std::printf("\n报告已写入 out/report.md\n");
    return r.failures == 0 ? 0 : 1;
}
