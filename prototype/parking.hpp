#pragma once
// ---------------------------------------------------------------------------
// parking.hpp — 重写原型：领域模型 + 多文件 JSON 仓储
//
// 相对原项目的关键改动：
//   1. 单一真相源：车位占用由 active 车辆推导，不再同时维护 car_ports[].state
//   2. 索引化：plate→ActiveEntry 哈希表 + 空闲车位有序集合（替代 O(n) 线性扫描）
//   3. 整数金额（分）+ int64 毫秒时间戳（替代 float 单价 + time_t + TIME_RATE）
//   4. 多文件存储 + 分代快照（gen-N/）+ 追加日志（journal.ndjson）
//   5. 崩溃安全：每次操作只追加一行；快照整代写入，用 CURRENT 单文件原子切换
// ---------------------------------------------------------------------------
#include "json.hpp"

#include <cstdint>
#include <deque>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pl {

// ------------------------------- 车型 --------------------------------------
enum class VehicleType { CarSmall = 0, TruckSmall = 1, TruckMedium = 2, TruckLarge = 3 };

const char* typeToken(VehicleType t);            // "car_small"
bool typeFromToken(std::string_view s, VehicleType& out);
const char* typeLabelZh(VehicleType t);          // "小汽车"

// ------------------------------- 配置 --------------------------------------
struct Config {
    int capacity = 50;
    int unitPriceCents = 500;       // ¥5.00 / 小时
    int freeMinutes = 15;           // 免费时长
    int billingUnitMinutes = 60;    // 计费单位
    int maxWaiting = 200;           // 等待队列上限
};

// ------------------------------- 实体 --------------------------------------
struct ActiveEntry {
    std::string ticket;
    std::string plate;
    VehicleType type = VehicleType::CarSmall;
    int slot = -1;
    int64_t enteredAtMs = 0;
};

struct WaitEntry {
    std::string ticket;
    std::string plate;
    VehicleType type = VehicleType::CarSmall;
    int64_t queuedAtMs = 0;
};

// ------------------------------- 结果 --------------------------------------
enum class EnterStatus { Parked, Queued, DuplicateActive, DuplicateWaiting, QueueFull, InvalidPlate };
enum class ExitStatus { Ok, NotFound, StillWaiting };

struct EnterResult {
    EnterStatus status = EnterStatus::QueueFull;
    int slot = -1;
    std::string ticket;
};

struct ExitResult {
    ExitStatus status = ExitStatus::NotFound;
    std::string ticket;
    std::string plate;
    VehicleType type = VehicleType::CarSmall;
    int slot = -1;
    int64_t enteredAtMs = 0;
    int64_t exitedAtMs = 0;
    int64_t stayMs = 0;
    int64_t feeCents = 0;
    bool promoted = false;
    std::string promotedTicket;
    std::string promotedPlate;
    VehicleType promotedType = VehicleType::CarSmall;
    int64_t promotedEnteredAtMs = 0;
};

// 计费：免费时长内 0 元；超出部分按 billingUnitMinutes 向上取整
int64_t feeCentsFor(int64_t enteredAtMs, int64_t nowMs, const Config& c, int64_t* billableMs = nullptr);

// ------------------------------- 停车场 ------------------------------------
class Lot {
public:
    Config cfg;

    std::unordered_map<std::string, ActiveEntry> active;  // 车牌 → 场内记录
    std::deque<WaitEntry> waiting;                        // FIFO，队首先补位
    std::vector<std::string> slotPlate;                   // 车位 → 车牌（"" 空）【派生】
    std::set<int> freeSlots;                              // 空闲车位【派生】

    int64_t totalEntered = 0;   // 入场次数（含入队）
    int64_t totalExited = 0;    // 出场次数
    int64_t revenueCents = 0;   // 累计收入

    void reset(const Config& c);
    bool validate(std::string& why) const;

    EnterResult enter(const std::string& plate, VehicleType t, int64_t nowMs);
    ExitResult leave(const std::string& plate, int64_t nowMs);

    int occupyCount() const { return static_cast<int>(active.size()); }
    int emptyCount() const { return cfg.capacity - occupyCount(); }
    bool hasPlate(const std::string& plate) const;
};

// ------------------------------- 2D 布局 -----------------------------------
struct SlotGeom {
    int index = 0;
    int row = 0;
    int col = 0;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct Layout {
    int rows = 0;
    int cols = 0;
    int aisleEvery = 2;      // 每 N 行一条行车道
    int aislePx = 22;
    int margin = 12;
    int slotW = 26;
    int slotH = 52;
    int gapX = 6;
    int gapY = 8;
    std::vector<SlotGeom> slots;   // 由参数派生，供 2D 视图直接使用
};

Layout makeLayout(int capacity);

// ------------------------------- 仓储 --------------------------------------
// 目录结构：
//   <dir>/CURRENT            当前快照代号（单个整数，原子替换）
//   <dir>/gen-<seq>/         config.json layout.json active.json waiting.json manifest.json
//   <dir>/journal.ndjson     上次快照之后的增量操作（一行一条）
//   <dir>/history/YYYY-MM.ndjson   出场流水（压缩时归档）
class LotStore {
public:
    struct OpenReport {
        bool ok = false;
        bool created = false;
        std::string error;
        int schemaVersion = 0;
        int64_t snapshotSeq = 0;
        int replayedOps = 0;
        int tornLines = 0;
        int generationsSeen = 0;
    };

    struct Stats {
        int64_t seq = 0;
        int64_t snapshotSeq = 0;
        int journalRecords = 0;
        size_t journalBytes = 0;
        size_t totalBytes = 0;
        int historyRecords = 0;
        int generations = 0;
    };

    explicit LotStore(std::filesystem::path dir) : dir_(std::move(dir)) {}

    const std::filesystem::path& dir() const { return dir_; }
    int64_t seq() const { return seq_; }

    // 载入：CURRENT → gen 快照 → 重放 journal → 校验不变量
    OpenReport open(Lot& lot, Config fallback = Config{});

    // 全量快照（写新代 → 切换 CURRENT → 归档流水 → 清空 journal）
    bool compact(const Lot& lot, std::string& err);

    // 追加日志（每次业务操作一行，不重写快照）
    bool recordEnter(const EnterResult& r, const std::string& plate, VehicleType t, int64_t nowMs, std::string& err);
    bool recordExit(const ExitResult& r, std::string& err);
    bool recordPrice(int unitPriceCents, int64_t nowMs, std::string& err);

    Stats stats() const;

private:
    std::filesystem::path dir_;
    std::string lotId_;
    int64_t seq_ = 0;
    int journalRecords_ = 0;

    bool appendRecord(jx::Value rec, std::string& err);
    bool archiveExits(std::string& err);
    bool writeGeneration(const Lot& lot, int64_t gen, std::string& err);
};

// ------------------------------- 工具 --------------------------------------
int64_t nowMs();
std::string makePlate(int n);          // 确定性生成车牌，便于复现
std::string signature(const Lot& lot); // 状态指纹，用于比较前后一致

} // namespace pl
