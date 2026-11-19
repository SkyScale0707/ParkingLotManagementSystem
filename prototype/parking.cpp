// ---------------------------------------------------------------------------
// parking.cpp — 领域模型 + 多文件 JSON 仓储实现
// ---------------------------------------------------------------------------
#include "parking.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <utility>

#ifdef _WIN32
#  define NOMINMAX
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

namespace fs = std::filesystem;

namespace pl {
namespace {

constexpr int kSchemaVersion = 1;
constexpr int64_t kMinuteMs = 60'000;

// ------------------------------ 文件工具 -----------------------------------
bool readFileText(const fs::path& p, std::string& out, std::string& err) {
    std::ifstream f(p, std::ios::binary);
    if (!f) {
        err = "无法读取: " + p.string();
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

// 临时文件 + 原子替换（Windows 用 MoveFileExW，与 QSaveFile 同机制）
bool writeFileAtomic(const fs::path& target, const std::string& content, std::string& err) {
    fs::path tmp = target;
    tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) {
            err = "无法写入临时文件: " + tmp.string();
            return false;
        }
        f.write(content.data(), static_cast<std::streamsize>(content.size()));
        f.flush();
        if (!f) {
            err = "写入失败: " + tmp.string();
            return false;
        }
    }
#ifdef _WIN32
    if (!MoveFileExW(tmp.wstring().c_str(), target.wstring().c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        err = "原子替换失败: " + target.string();
        return false;
    }
#else
    std::error_code ec;
    fs::rename(tmp, target, ec);
    if (ec) {
        err = "重命名失败: " + ec.message();
        return false;
    }
#endif
    return true;
}

bool writeFileNew(const fs::path& p, const std::string& content, std::string& err) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) {
        err = "无法写入: " + p.string();
        return false;
    }
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    f.flush();
    if (!f) {
        err = "写入失败: " + p.string();
        return false;
    }
    return true;
}

std::string monthKey(int64_t epochMs) {
    std::time_t t = static_cast<std::time_t>(epochMs / 1000);
    std::tm tm{};
    localtime_s(&tm, &t);
    char buf[16];
    std::snprintf(buf, sizeof buf, "%04d-%02d", tm.tm_year + 1900, tm.tm_mon + 1);
    return buf;
}

int64_t maxSeqInFile(const fs::path& p) {
    std::string text, err;
    if (!readFileText(p, text, err)) return 0;
    size_t pos = 0;
    int64_t best = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, (nl == std::string::npos ? text.size() : nl) - pos);
        pos = (nl == std::string::npos) ? text.size() : nl + 1;
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
        jx::Value v;
        std::string perr;
        if (!jx::parse(line, v, perr)) continue;
        best = std::max(best, v.getInt("seq", 0));
    }
    return best;
}

// 把一条 journal 记录应用到内存模型（重放）
bool applyOp(Lot& lot, const jx::Value& rec, std::string& err) {
    const std::string op = rec.getString("op");
    const int64_t at = rec.getInt("at", 0);

    if (op == "enter") {
        ActiveEntry e;
        e.ticket = rec.getString("ticket");
        e.plate = rec.getString("plate");
        VehicleType t;
        if (!typeFromToken(rec.getString("type"), t)) { err = "enter: 车型非法"; return false; }
        e.type = t;
        e.slot = static_cast<int>(rec.getInt("slot", -1));
        e.enteredAtMs = rec.getInt("enteredAtMs", at);
        if (e.plate.empty()) { err = "enter: 车牌为空"; return false; }
        if (e.slot < 0 || e.slot >= lot.cfg.capacity) { err = "enter: 车位越界"; return false; }
        if (lot.active.count(e.plate)) { err = "enter: 重复车牌"; return false; }
        lot.active.emplace(e.plate, e);
        lot.slotPlate[static_cast<size_t>(e.slot)] = e.plate;
        lot.freeSlots.erase(e.slot);
        ++lot.totalEntered;
        return true;
    }
    if (op == "queue") {
        WaitEntry w;
        w.ticket = rec.getString("ticket");
        w.plate = rec.getString("plate");
        VehicleType t;
        if (!typeFromToken(rec.getString("type"), t)) { err = "queue: 车型非法"; return false; }
        w.type = t;
        w.queuedAtMs = rec.getInt("queuedAtMs", at);
        if (w.plate.empty()) { err = "queue: 车牌为空"; return false; }
        lot.waiting.push_back(w);
        ++lot.totalEntered;
        return true;
    }
    if (op == "exit") {
        const std::string plate = rec.getString("plate");
        auto it = lot.active.find(plate);
        if (it == lot.active.end()) { err = "exit: 车辆不在场内 " + plate; return false; }
        const int slot = it->second.slot;
        lot.active.erase(it);
        lot.slotPlate[static_cast<size_t>(slot)].clear();
        lot.freeSlots.insert(slot);
        ++lot.totalExited;
        lot.revenueCents += rec.getInt("feeCents", 0);

        const jx::Value* promoted = rec.find("promoted");
        if (promoted && promoted->isObject()) {
            const std::string ticket = promoted->getString("ticket");
            bool found = false;
            for (auto wit = lot.waiting.begin(); wit != lot.waiting.end(); ++wit) {
                if (wit->ticket == ticket) {
                    lot.waiting.erase(wit);
                    found = true;
                    break;
                }
            }
            if (!found) { err = "exit.promoted: 凭证不在队列 " + ticket; return false; }
            ActiveEntry e;
            e.ticket = ticket;
            e.plate = promoted->getString("plate");
            VehicleType t;
            if (!typeFromToken(promoted->getString("type"), t)) { err = "exit.promoted: 车型非法"; return false; }
            e.type = t;
            e.slot = static_cast<int>(promoted->getInt("slot", slot));
            e.enteredAtMs = promoted->getInt("enteredAtMs", at);
            if (e.slot < 0 || e.slot >= lot.cfg.capacity) { err = "exit.promoted: 车位越界"; return false; }
            lot.active.emplace(e.plate, e);
            lot.slotPlate[static_cast<size_t>(e.slot)] = e.plate;
            lot.freeSlots.erase(e.slot);
        }
        return true;
    }
    if (op == "price") {
        lot.cfg.unitPriceCents = static_cast<int>(rec.getInt("unitPriceCents", lot.cfg.unitPriceCents));
        return true;
    }
    err = "未知操作: " + op;
    return false;
}

} // namespace

// ------------------------------ 车型 ---------------------------------------
const char* typeToken(VehicleType t) {
    switch (t) {
    case VehicleType::CarSmall: return "car_small";
    case VehicleType::TruckSmall: return "truck_small";
    case VehicleType::TruckMedium: return "truck_medium";
    case VehicleType::TruckLarge: return "truck_large";
    }
    return "car_small";
}

bool typeFromToken(std::string_view s, VehicleType& out) {
    if (s == "car_small") { out = VehicleType::CarSmall; return true; }
    if (s == "truck_small") { out = VehicleType::TruckSmall; return true; }
    if (s == "truck_medium") { out = VehicleType::TruckMedium; return true; }
    if (s == "truck_large") { out = VehicleType::TruckLarge; return true; }
    return false;
}

const char* typeLabelZh(VehicleType t) {
    switch (t) {
    case VehicleType::CarSmall: return "小汽车";
    case VehicleType::TruckSmall: return "小卡";
    case VehicleType::TruckMedium: return "中卡";
    case VehicleType::TruckLarge: return "大卡";
    }
    return "未知";
}

// ------------------------------ 计费 ---------------------------------------
int64_t feeCentsFor(int64_t enteredAtMs, int64_t nowMs, const Config& c, int64_t* billableMs) {
    int64_t stay = nowMs - enteredAtMs;
    if (stay < 0) stay = 0;
    const int64_t freeMs = static_cast<int64_t>(c.freeMinutes) * kMinuteMs;
    if (stay <= freeMs) {
        if (billableMs) *billableMs = 0;
        return 0;
    }
    int64_t billable = stay - freeMs;
    int64_t unitMs = static_cast<int64_t>(c.billingUnitMinutes) * kMinuteMs;
    if (unitMs <= 0) unitMs = kMinuteMs;
    const int64_t units = (billable + unitMs - 1) / unitMs;
    if (billableMs) *billableMs = billable;
    return units * static_cast<int64_t>(c.unitPriceCents);
}

// ------------------------------ Lot ----------------------------------------
void Lot::reset(const Config& c) {
    cfg = c;
    if (cfg.capacity < 1) cfg.capacity = 1;
    if (cfg.maxWaiting < 0) cfg.maxWaiting = 0;
    active.clear();
    waiting.clear();
    slotPlate.assign(static_cast<size_t>(cfg.capacity), std::string());
    freeSlots.clear();
    for (int i = 0; i < cfg.capacity; ++i) freeSlots.insert(i);
    totalEntered = totalExited = revenueCents = 0;
}

bool Lot::hasPlate(const std::string& plate) const {
    return active.count(plate) != 0;
}

bool Lot::validate(std::string& why) const {
    if (cfg.capacity <= 0) { why = "容量非法"; return false; }
    if (static_cast<int>(slotPlate.size()) != cfg.capacity) { why = "slotPlate 尺寸不符"; return false; }
    if (static_cast<int>(active.size()) + static_cast<int>(freeSlots.size()) != cfg.capacity) {
        why = "空闲集合与场内车辆数不互补"; return false;
    }
    std::set<int> occupied;
    for (const auto& kv : active) {
        const ActiveEntry& e = kv.second;
        if (e.slot < 0 || e.slot >= cfg.capacity) { why = "车位越界: " + e.plate; return false; }
        if (kv.first != e.plate) { why = "键与车牌不一致"; return false; }
        if (slotPlate[static_cast<size_t>(e.slot)] != e.plate) { why = "车位映射不一致: " + e.plate; return false; }
        if (!occupied.insert(e.slot).second) { why = "车位重复占用"; return false; }
        if (e.enteredAtMs <= 0) { why = "入场时间非法: " + e.plate; return false; }
    }
    for (int s : freeSlots) {
        if (s < 0 || s >= cfg.capacity) { why = "空闲车位越界"; return false; }
        if (!slotPlate[static_cast<size_t>(s)].empty()) { why = "空闲车位却有车"; return false; }
        if (occupied.count(s)) { why = "空闲集合含已占车位"; return false; }
    }
    if (static_cast<int>(waiting.size()) > cfg.maxWaiting) { why = "等待队列超限"; return false; }
    std::set<std::string> waitingPlates;
    for (const auto& w : waiting) {
        if (!waitingPlates.insert(w.plate).second) { why = "队列内车牌重复"; return false; }
        if (active.count(w.plate)) { why = "同一车既在场内又在队列: " + w.plate; return false; }
    }
    if (!waiting.empty() && !freeSlots.empty()) { why = "有车在排队却仍有空位"; return false; }
    return true;
}

EnterResult Lot::enter(const std::string& plate, VehicleType t, int64_t now) {
    EnterResult r;
    if (plate.empty()) { r.status = EnterStatus::InvalidPlate; return r; }
    if (active.count(plate)) { r.status = EnterStatus::DuplicateActive; return r; }
    for (const auto& w : waiting) {
        if (w.plate == plate) { r.status = EnterStatus::DuplicateWaiting; return r; }
    }

    const std::string ticket = "T-" + std::to_string(totalEntered + 1);

    if (!freeSlots.empty()) {
        const int slot = *freeSlots.begin();
        freeSlots.erase(freeSlots.begin());
        ActiveEntry e{ticket, plate, t, slot, now};
        active.emplace(plate, e);
        slotPlate[static_cast<size_t>(slot)] = plate;
        ++totalEntered;
        r.status = EnterStatus::Parked;
        r.slot = slot;
        r.ticket = ticket;
        return r;
    }
    if (static_cast<int>(waiting.size()) >= cfg.maxWaiting) {
        r.status = EnterStatus::QueueFull;
        return r;
    }
    waiting.push_back(WaitEntry{ticket, plate, t, now});
    ++totalEntered;
    r.status = EnterStatus::Queued;
    r.ticket = ticket;
    return r;
}

ExitResult Lot::leave(const std::string& plate, int64_t now) {
    ExitResult r;
    auto it = active.find(plate);
    if (it == active.end()) {
        for (const auto& w : waiting) {
            if (w.plate == plate) {
                r.status = ExitStatus::StillWaiting;
                r.plate = plate;
                r.ticket = w.ticket;
                return r;
            }
        }
        r.status = ExitStatus::NotFound;
        r.plate = plate;
        return r;
    }

    const ActiveEntry e = it->second;
    r.status = ExitStatus::Ok;
    r.ticket = e.ticket;
    r.plate = e.plate;
    r.type = e.type;
    r.slot = e.slot;
    r.enteredAtMs = e.enteredAtMs;
    r.exitedAtMs = now;
    r.stayMs = now - e.enteredAtMs;
    r.feeCents = feeCentsFor(e.enteredAtMs, now, cfg);

    active.erase(it);
    slotPlate[static_cast<size_t>(e.slot)].clear();
    freeSlots.insert(e.slot);
    ++totalExited;
    revenueCents += r.feeCents;

    if (!waiting.empty()) {
        const WaitEntry w = waiting.front();
        waiting.pop_front();
        // 计费口径：排队时间不计费，计费起点 = 补位入位时刻
        ActiveEntry promoted{w.ticket, w.plate, w.type, e.slot, now};
        active.emplace(w.plate, promoted);
        slotPlate[static_cast<size_t>(e.slot)] = w.plate;
        freeSlots.erase(e.slot);
        r.promoted = true;
        r.promotedTicket = w.ticket;
        r.promotedPlate = w.plate;
        r.promotedType = w.type;
        r.promotedEnteredAtMs = now;
    }
    return r;
}

// ------------------------------ Layout -------------------------------------
Layout makeLayout(int capacity) {
    Layout L;
    if (capacity < 1) capacity = 1;
    L.slotW = 26;
    L.slotH = 52;
    L.gapX = 6;
    L.gapY = 8;
    L.aislePx = 22;
    L.margin = 12;
    L.aisleEvery = 2;

    // 目标宽高比 ≈ 1.6（横向布局），列数限制在 4..30 以保证可读性
    int cols = static_cast<int>(std::lround(std::sqrt(static_cast<double>(capacity) * 1.6)));
    if (cols < 4) cols = 4;
    if (cols > 30) cols = 30;
    if (cols > capacity) cols = capacity;
    const int rows = (capacity + cols - 1) / cols;
    L.cols = cols;
    L.rows = rows;

    L.slots.reserve(static_cast<size_t>(capacity));
    for (int i = 0; i < capacity; ++i) {
        SlotGeom g;
        g.index = i;
        g.row = i / cols;
        g.col = i % cols;
        const int aisles = g.row / L.aisleEvery;
        g.x = L.margin + g.col * (L.slotW + L.gapX);
        g.y = L.margin + g.row * (L.slotH + L.gapY) + aisles * L.aislePx;
        g.w = L.slotW;
        g.h = L.slotH;
        L.slots.push_back(g);
    }
    return L;
}

// ------------------------------ LotStore -----------------------------------
bool LotStore::appendRecord(jx::Value rec, std::string& err) {
    ++seq_;
    rec.o.insert(rec.o.begin(), std::make_pair(std::string("seq"), jx::Value::integer(seq_)));

    std::string line = jx::dump(rec, false);
    line += '\n';

    const fs::path jp = dir_ / "journal.ndjson";
    std::ofstream f(jp, std::ios::binary | std::ios::app);
    if (!f) {
        err = "无法追加日志: " + jp.string();
        return false;
    }
    f.write(line.data(), static_cast<std::streamsize>(line.size()));
    f.flush();
    if (!f) {
        err = "日志写入失败";
        return false;
    }
    ++journalRecords_;
    return true;
}

bool LotStore::recordEnter(const EnterResult& r, const std::string& plate, VehicleType t,
                           int64_t now, std::string& err) {
    if (r.status != EnterStatus::Parked && r.status != EnterStatus::Queued) return true;
    jx::Value rec = jx::Value::object();
    if (r.status == EnterStatus::Parked) {
        rec.set("op", jx::Value::str("enter"));
        rec.set("ticket", jx::Value::str(r.ticket));
        rec.set("plate", jx::Value::str(plate));
        rec.set("type", jx::Value::str(typeToken(t)));
        rec.set("slot", jx::Value::integer(r.slot));
        rec.set("enteredAtMs", jx::Value::integer(now));
    } else {
        rec.set("op", jx::Value::str("queue"));
        rec.set("ticket", jx::Value::str(r.ticket));
        rec.set("plate", jx::Value::str(plate));
        rec.set("type", jx::Value::str(typeToken(t)));
        rec.set("queuedAtMs", jx::Value::integer(now));
    }
    rec.set("at", jx::Value::integer(now));
    return appendRecord(std::move(rec), err);
}

bool LotStore::recordExit(const ExitResult& r, std::string& err) {
    if (r.status != ExitStatus::Ok) return true;
    jx::Value rec = jx::Value::object();
    rec.set("op", jx::Value::str("exit"));
    rec.set("ticket", jx::Value::str(r.ticket));
    rec.set("plate", jx::Value::str(r.plate));
    rec.set("type", jx::Value::str(typeToken(r.type)));
    rec.set("slot", jx::Value::integer(r.slot));
    rec.set("enteredAtMs", jx::Value::integer(r.enteredAtMs));
    rec.set("stayMs", jx::Value::integer(r.stayMs));
    rec.set("feeCents", jx::Value::integer(r.feeCents));
    rec.set("at", jx::Value::integer(r.exitedAtMs));
    if (r.promoted) {
        jx::Value p = jx::Value::object();
        p.set("ticket", jx::Value::str(r.promotedTicket));
        p.set("plate", jx::Value::str(r.promotedPlate));
        p.set("type", jx::Value::str(typeToken(r.promotedType)));
        p.set("slot", jx::Value::integer(r.slot));
        p.set("enteredAtMs", jx::Value::integer(r.promotedEnteredAtMs));
        rec.set("promoted", std::move(p));
    }
    return appendRecord(std::move(rec), err);
}

bool LotStore::recordPrice(int unitPriceCents, int64_t now, std::string& err) {
    jx::Value rec = jx::Value::object();
    rec.set("op", jx::Value::str("price"));
    rec.set("unitPriceCents", jx::Value::integer(unitPriceCents));
    rec.set("at", jx::Value::integer(now));
    return appendRecord(std::move(rec), err);
}

bool LotStore::writeGeneration(const Lot& lot, int64_t gen, std::string& err) {
    const fs::path gdir = dir_ / ("gen-" + std::to_string(gen));
    std::error_code ec;
    fs::create_directories(gdir, ec);
    if (!fs::exists(gdir)) { err = "无法创建快照目录: " + gdir.string(); return false; }

    // config.json
    jx::Value cfg = jx::Value::object();
    cfg.set("schemaVersion", jx::Value::integer(kSchemaVersion));
    cfg.set("capacity", jx::Value::integer(lot.cfg.capacity));
    cfg.set("unitPriceCents", jx::Value::integer(lot.cfg.unitPriceCents));
    cfg.set("freeMinutes", jx::Value::integer(lot.cfg.freeMinutes));
    cfg.set("billingUnitMinutes", jx::Value::integer(lot.cfg.billingUnitMinutes));
    cfg.set("maxWaiting", jx::Value::integer(lot.cfg.maxWaiting));
    if (!writeFileNew(gdir / "config.json", jx::dump(cfg, true), err)) return false;

    // layout.json（2D 视图几何，派生数据）
    const Layout L = makeLayout(lot.cfg.capacity);
    jx::Value lay = jx::Value::object();
    lay.set("capacity", jx::Value::integer(lot.cfg.capacity));
    lay.set("rows", jx::Value::integer(L.rows));
    lay.set("cols", jx::Value::integer(L.cols));
    lay.set("aisleEvery", jx::Value::integer(L.aisleEvery));
    lay.set("aislePx", jx::Value::integer(L.aislePx));
    lay.set("margin", jx::Value::integer(L.margin));
    lay.set("slotW", jx::Value::integer(L.slotW));
    lay.set("slotH", jx::Value::integer(L.slotH));
    lay.set("gapX", jx::Value::integer(L.gapX));
    lay.set("gapY", jx::Value::integer(L.gapY));
    jx::Value sarr = jx::Value::array();
    for (const auto& g : L.slots) {
        jx::Value o = jx::Value::object();
        o.set("i", jx::Value::integer(g.index));
        o.set("r", jx::Value::integer(g.row));
        o.set("c", jx::Value::integer(g.col));
        o.set("x", jx::Value::integer(g.x));
        o.set("y", jx::Value::integer(g.y));
        o.set("w", jx::Value::integer(g.w));
        o.set("h", jx::Value::integer(g.h));
        sarr.push(std::move(o));
    }
    lay.set("slots", std::move(sarr));
    if (!writeFileNew(gdir / "layout.json", jx::dump(lay, true), err)) return false;

    // active.json（场内车辆，按车位排序保证稳定）
    std::vector<const ActiveEntry*> parked;
    parked.reserve(lot.active.size());
    for (const auto& kv : lot.active) parked.push_back(&kv.second);
    std::sort(parked.begin(), parked.end(),
              [](const ActiveEntry* a, const ActiveEntry* b) { return a->slot < b->slot; });

    jx::Value act = jx::Value::object();
    act.set("seq", jx::Value::integer(gen));
    act.set("totalEntered", jx::Value::integer(lot.totalEntered));
    act.set("totalExited", jx::Value::integer(lot.totalExited));
    act.set("revenueCents", jx::Value::integer(lot.revenueCents));
    jx::Value aarr = jx::Value::array();
    for (const ActiveEntry* e : parked) {
        jx::Value o = jx::Value::object();
        o.set("ticket", jx::Value::str(e->ticket));
        o.set("plate", jx::Value::str(e->plate));
        o.set("type", jx::Value::str(typeToken(e->type)));
        o.set("slot", jx::Value::integer(e->slot));
        o.set("enteredAtMs", jx::Value::integer(e->enteredAtMs));
        aarr.push(std::move(o));
    }
    act.set("entries", std::move(aarr));
    if (!writeFileNew(gdir / "active.json", jx::dump(act, true), err)) return false;

    // waiting.json（FIFO，数组顺序即队列顺序）
    jx::Value wv = jx::Value::object();
    wv.set("seq", jx::Value::integer(gen));
    jx::Value warr = jx::Value::array();
    for (const auto& w : lot.waiting) {
        jx::Value o = jx::Value::object();
        o.set("ticket", jx::Value::str(w.ticket));
        o.set("plate", jx::Value::str(w.plate));
        o.set("type", jx::Value::str(typeToken(w.type)));
        o.set("queuedAtMs", jx::Value::integer(w.queuedAtMs));
        warr.push(std::move(o));
    }
    wv.set("entries", std::move(warr));
    if (!writeFileNew(gdir / "waiting.json", jx::dump(wv, true), err)) return false;

    // manifest.json（本代元信息；CURRENT 最后才切换，所以它不承担原子性）
    jx::Value man = jx::Value::object();
    man.set("schemaVersion", jx::Value::integer(kSchemaVersion));
    man.set("lotId", jx::Value::str(lotId_));
    man.set("snapshotSeq", jx::Value::integer(gen));
    man.set("writtenAtMs", jx::Value::integer(nowMs()));
    man.set("capacity", jx::Value::integer(lot.cfg.capacity));
    man.set("occupy", jx::Value::integer(lot.occupyCount()));
    man.set("waiting", jx::Value::integer(static_cast<long long>(lot.waiting.size())));
    if (!writeFileNew(gdir / "manifest.json", jx::dump(man, true), err)) return false;

    return true;
}

bool LotStore::archiveExits(std::string& err) {
    std::string text;
    std::string rerr;
    if (!readFileText(dir_ / "journal.ndjson", text, rerr)) return true;  // 没有日志即无事可做

    std::map<std::string, std::vector<std::string>> byMonth;  // 月份 → 行内容
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, (nl == std::string::npos ? text.size() : nl) - pos);
        pos = (nl == std::string::npos) ? text.size() : nl + 1;
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
        jx::Value rec;
        std::string perr;
        if (!jx::parse(line, rec, perr)) break;
        if (rec.getString("op") != "exit") continue;
        byMonth[monthKey(rec.getInt("at", 0))].push_back(jx::dump(rec, false));
    }
    if (byMonth.empty()) return true;

    const fs::path hdir = dir_ / "history";
    std::error_code ec;
    fs::create_directories(hdir, ec);

    for (auto& kv : byMonth) {
        const fs::path hp = hdir / (kv.first + ".ndjson");
        const int64_t lastSeq = maxSeqInFile(hp);
        std::ofstream f(hp, std::ios::binary | std::ios::app);
        if (!f) { err = "无法写入归档: " + hp.string(); return false; }
        for (const auto& line : kv.second) {
            jx::Value rec;
            std::string perr;
            if (!jx::parse(line, rec, perr)) continue;
            if (rec.getInt("seq", 0) <= lastSeq) continue;   // 幂等：跳过已归档
            f.write(line.data(), static_cast<std::streamsize>(line.size()));
            f.put('\n');
        }
        f.flush();
        if (!f) { err = "归档写入失败: " + hp.string(); return false; }
    }
    return true;
}

bool LotStore::compact(const Lot& lot, std::string& err) {
    const int64_t nextGen = seq_;

    // 1) 先归档流水（journal 尚未截断，失败可重来）
    if (!archiveExits(err)) return false;

    // 2) 写完整的一代快照
    if (!writeGeneration(lot, nextGen, err)) return false;

    // 3) 原子切换 CURRENT（唯一的原子点）
    if (!writeFileAtomic(dir_ / "CURRENT", std::to_string(nextGen), err)) return false;

    // 4) 截断 journal（若此前崩溃，重放会按 seq 跳过，不会重复应用）
    if (!writeFileAtomic(dir_ / "journal.ndjson", std::string(), err)) return false;
    journalRecords_ = 0;

    // 5) 清理旧代（保留最近 2 代以便回退）
    std::vector<int64_t> gens;
    std::error_code ec;
    for (const auto& de : fs::directory_iterator(dir_, ec)) {
        const std::string name = de.path().filename().string();
        if (name.rfind("gen-", 0) == 0) {
            int64_t g = 0;
            if (sscanf_s(name.c_str(), "gen-%lld", &g) == 1) gens.push_back(g);
        }
    }
    std::sort(gens.begin(), gens.end());
    while (gens.size() > 2) {
        fs::remove_all(dir_ / ("gen-" + std::to_string(gens.front())), ec);
        gens.erase(gens.begin());
    }
    return true;
}

LotStore::OpenReport LotStore::open(Lot& lot, Config fallback) {
    OpenReport rep;
    std::error_code ec;
    fs::create_directories(dir_, ec);
    if (!fs::exists(dir_)) {
        rep.error = "无法创建数据目录: " + dir_.string();
        return rep;
    }

    // 收集可用的快照代号
    std::vector<int64_t> gens;
    for (const auto& de : fs::directory_iterator(dir_, ec)) {
        const std::string name = de.path().filename().string();
        if (name.rfind("gen-", 0) == 0) {
            int64_t g = 0;
            if (sscanf_s(name.c_str(), "gen-%lld", &g) == 1) gens.push_back(g);
        }
    }
    std::sort(gens.begin(), gens.end(), std::greater<int64_t>());
    rep.generationsSeen = static_cast<int>(gens.size());

    int64_t cur = -1;
    {
        std::string text, rerr;
        if (readFileText(dir_ / "CURRENT", text, rerr)) {
            try { cur = std::stoll(text); } catch (...) { cur = -1; }
        }
    }

    // 候选：CURRENT 指向的代优先，其次按代号降序
    std::vector<int64_t> candidates;
    if (cur >= 0) candidates.push_back(cur);
    for (int64_t g : gens) {
        if (std::find(candidates.begin(), candidates.end(), g) == candidates.end()) candidates.push_back(g);
    }

    Config cfg = fallback;
    int64_t snapshotSeq = 0;
    std::string text, err;
    bool loaded = false;

    for (int64_t g : candidates) {
        const fs::path gdir = dir_ / ("gen-" + std::to_string(g));
        if (!fs::exists(gdir / "active.json") || !fs::exists(gdir / "waiting.json")) continue;

        Config c = cfg;
        bool cfgOk = false;
        if (readFileText(gdir / "config.json", text, err)) {
            jx::Value v;
            std::string perr;
            if (jx::parse(text, v, perr)) {
                c.capacity = static_cast<int>(v.getInt("capacity", c.capacity));
                c.unitPriceCents = static_cast<int>(v.getInt("unitPriceCents", c.unitPriceCents));
                c.freeMinutes = static_cast<int>(v.getInt("freeMinutes", c.freeMinutes));
                c.billingUnitMinutes = static_cast<int>(v.getInt("billingUnitMinutes", c.billingUnitMinutes));
                c.maxWaiting = static_cast<int>(v.getInt("maxWaiting", c.maxWaiting));
                cfgOk = true;
            }
        }
        if (!cfgOk) continue;

        lot.reset(c);
        cfg = c;

        bool ok = true;
        if (readFileText(gdir / "active.json", text, err)) {
            jx::Value a;
            std::string perr;
            if (!jx::parse(text, a, perr)) { ok = false; }
            else {
                snapshotSeq = std::max(snapshotSeq, a.getInt("seq", g));
                lot.totalEntered = a.getInt("totalEntered", 0);
                lot.totalExited = a.getInt("totalExited", 0);
                lot.revenueCents = a.getInt("revenueCents", 0);
                const jx::Value* arr = a.find("entries");
                if (arr && arr->isArray()) {
                    for (const auto& e : arr->a) {
                        ActiveEntry ae;
                        ae.ticket = e.getString("ticket");
                        ae.plate = e.getString("plate");
                        VehicleType t;
                        if (!typeFromToken(e.getString("type"), t)) { ok = false; break; }
                        ae.type = t;
                        ae.slot = static_cast<int>(e.getInt("slot", -1));
                        ae.enteredAtMs = e.getInt("enteredAtMs", 0);
                        if (ae.plate.empty() || ae.slot < 0 || ae.slot >= lot.cfg.capacity) { ok = false; break; }
                        lot.active.emplace(ae.plate, ae);
                        lot.slotPlate[static_cast<size_t>(ae.slot)] = ae.plate;
                        lot.freeSlots.erase(ae.slot);
                    }
                }
            }
        } else {
            ok = false;
        }
        if (!ok) continue;

        if (readFileText(gdir / "waiting.json", text, err)) {
            jx::Value w;
            std::string perr;
            if (!jx::parse(text, w, perr)) continue;
            snapshotSeq = std::max(snapshotSeq, w.getInt("seq", g));
            const jx::Value* arr = w.find("entries");
            if (arr && arr->isArray()) {
                for (const auto& e : arr->a) {
                    WaitEntry we;
                    we.ticket = e.getString("ticket");
                    we.plate = e.getString("plate");
                    VehicleType t;
                    if (!typeFromToken(e.getString("type"), t)) { ok = false; break; }
                    we.type = t;
                    we.queuedAtMs = e.getInt("queuedAtMs", 0);
                    if (we.plate.empty()) { ok = false; break; }
                    lot.waiting.push_back(we);
                }
            }
        } else {
            ok = false;
        }
        if (!ok) continue;

        if (readFileText(gdir / "manifest.json", text, err)) {
            jx::Value m;
            std::string perr;
            if (jx::parse(text, m, perr)) {
                rep.schemaVersion = static_cast<int>(m.getInt("schemaVersion", 0));
                lotId_ = m.getString("lotId");
            }
        }
        loaded = true;
        break;
    }

    if (!loaded && !gens.empty()) {
        rep.error = "所有快照代均不可用";
        return rep;
    }
    if (!loaded) {
        // 全新目录：保留已存在的 config.json（若有）
        if (readFileText(dir_ / "config.json", text, err)) {
            jx::Value v;
            std::string perr;
            if (jx::parse(text, v, perr)) {
                cfg.capacity = static_cast<int>(v.getInt("capacity", cfg.capacity));
                cfg.unitPriceCents = static_cast<int>(v.getInt("unitPriceCents", cfg.unitPriceCents));
            }
        }
        lot.reset(cfg);
        rep.created = true;
    }
    if (lotId_.empty()) {
        lotId_ = "lot-" + std::to_string(nowMs());
    }

    // 重放 journal
    if (readFileText(dir_ / "journal.ndjson", text, err)) {
        size_t pos = 0;
        while (pos < text.size()) {
            size_t nl = text.find('\n', pos);
            std::string line = text.substr(pos, (nl == std::string::npos ? text.size() : nl) - pos);
            pos = (nl == std::string::npos) ? text.size() : nl + 1;
            if (line.find_first_not_of(" \t\r") == std::string::npos) continue;

            jx::Value rec;
            std::string perr;
            if (!jx::parse(line, rec, perr)) {
                ++rep.tornLines;   // 残尾：最后一行写了一半，说明上次是崩溃退出
                break;
            }
            const int64_t s = rec.getInt("seq", 0);
            seq_ = std::max(seq_, s);
            ++journalRecords_;
            if (s <= snapshotSeq) continue;
            std::string aerr;
            if (!applyOp(lot, rec, aerr)) {
                rep.error = "重放失败: " + aerr;
                return rep;
            }
            ++rep.replayedOps;
        }
    }

    seq_ = std::max(seq_, snapshotSeq);

    std::string why;
    if (!lot.validate(why)) {
        rep.error = "不变量被破坏: " + why;
        return rep;
    }

    rep.snapshotSeq = snapshotSeq;
    rep.ok = true;
    return rep;
}

LotStore::Stats LotStore::stats() const {
    Stats s;
    s.seq = seq_;
    s.snapshotSeq = seq_;
    s.journalRecords = journalRecords_;
    std::error_code ec;
    const fs::path jp = dir_ / "journal.ndjson";
    if (fs::exists(jp, ec)) s.journalBytes = static_cast<size_t>(fs::file_size(jp, ec));
    for (const auto& de : fs::directory_iterator(dir_, ec)) {
        std::error_code ec2;
        if (de.is_directory(ec2)) {
            if (de.path().filename().string().rfind("gen-", 0) == 0) ++s.generations;
            for (const auto& sub : fs::recursive_directory_iterator(de.path(), ec2)) {
                if (sub.is_regular_file(ec2)) s.totalBytes += static_cast<size_t>(fs::file_size(sub.path(), ec2));
            }
        } else {
            s.totalBytes += static_cast<size_t>(fs::file_size(de.path(), ec2));
        }
    }
    const fs::path hdir = dir_ / "history";
    for (const auto& de : fs::directory_iterator(hdir, ec)) {
        std::error_code ec2;
        if (!de.is_regular_file(ec2)) continue;
        std::string t, rerr;
        if (!readFileText(de.path(), t, rerr)) continue;
        for (char ch : t) if (ch == '\n') ++s.historyRecords;
    }
    return s;
}

// ------------------------------ 工具 ---------------------------------------
int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string makePlate(int n) {
    static const char* prov[] = {"鄂", "京", "沪", "粤", "苏", "浙", "川", "鲁", "豫", "皖"};
    if (n < 0) n = 0;
    char buf[32];
    std::snprintf(buf, sizeof buf, "%sA%05d", prov[static_cast<size_t>(n) % 10], n % 100000);
    return std::string(buf);
}

std::string signature(const Lot& lot) {
    jx::Value root = jx::Value::object();
    root.set("capacity", jx::Value::integer(lot.cfg.capacity));
    root.set("priceCents", jx::Value::integer(lot.cfg.unitPriceCents));
    root.set("freeMinutes", jx::Value::integer(lot.cfg.freeMinutes));
    root.set("billingUnit", jx::Value::integer(lot.cfg.billingUnitMinutes));
    root.set("maxWaiting", jx::Value::integer(lot.cfg.maxWaiting));
    root.set("totalEntered", jx::Value::integer(lot.totalEntered));
    root.set("totalExited", jx::Value::integer(lot.totalExited));
    root.set("revenue", jx::Value::integer(lot.revenueCents));

    std::vector<const ActiveEntry*> parked;
    for (const auto& kv : lot.active) parked.push_back(&kv.second);
    std::sort(parked.begin(), parked.end(),
              [](const ActiveEntry* a, const ActiveEntry* b) { return a->slot < b->slot; });
    jx::Value arr = jx::Value::array();
    for (const ActiveEntry* e : parked) {
        jx::Value o = jx::Value::object();
        o.set("slot", jx::Value::integer(e->slot));
        o.set("plate", jx::Value::str(e->plate));
        o.set("type", jx::Value::str(typeToken(e->type)));
        o.set("ticket", jx::Value::str(e->ticket));
        o.set("at", jx::Value::integer(e->enteredAtMs));
        arr.push(std::move(o));
    }
    root.set("active", std::move(arr));

    jx::Value q = jx::Value::array();
    for (const auto& w : lot.waiting) {
        jx::Value o = jx::Value::object();
        o.set("ticket", jx::Value::str(w.ticket));
        o.set("plate", jx::Value::str(w.plate));
        o.set("type", jx::Value::str(typeToken(w.type)));
        o.set("at", jx::Value::integer(w.queuedAtMs));
        q.push(std::move(o));
    }
    root.set("waiting", std::move(q));
    return jx::dump(root, false);
}

} // namespace pl
