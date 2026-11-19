#pragma once
// ---------------------------------------------------------------------------
// lotstore.h — 多文件 JSON 仓储（分代快照 + 追加日志）
//
// 磁盘布局：
//   <dir>/CURRENT                    当前快照代号（原子替换）
//   <dir>/gen-<seq>/config.json      容量 / 单价 / 免费时长 / 计费单位
//   <dir>/gen-<seq>/layout.json      2D 几何（派生，可随时重建）
//   <dir>/gen-<seq>/active.json      场内车辆 + 累计计数（权威）
//   <dir>/gen-<seq>/waiting.json     FIFO 队列（权威）
//   <dir>/gen-<seq>/manifest.json    本代元信息
//   <dir>/journal.ndjson             上次快照之后的增量（一行一条）
//   <dir>/history/YYYY-MM.ndjson     出场流水（压缩时归档，只追加）
//
// 一致性：写盘顺序 = 整代文件 → CURRENT → 清空 journal。
// CURRENT 是唯一的原子切换点，它指向的那一代必然完整。
// ---------------------------------------------------------------------------
#include "domain/lot.h"

#include <QJsonObject>
#include <QString>

namespace pl {

class LotStore
{
public:
    struct OpenReport {
        bool ok = false;
        bool created = false;
        QString error;
        int schemaVersion = 0;
        qint64 snapshotSeq = 0;
        int replayedOps = 0;
        int tornLines = 0;
        int generationsSeen = 0;
    };

    struct Stats {
        qint64 seq = 0;
        qint64 snapshotSeq = 0;
        int journalRecords = 0;
        qint64 journalBytes = 0;
        qint64 totalBytes = 0;
        int historyRecords = 0;
        int generations = 0;
    };

    explicit LotStore(QString dir);

    QString dir() const { return dir_; }
    qint64 seq() const { return seq_; }
    qint64 snapshotSeq() const { return snapshotSeq_; }
    QString lotId() const { return lotId_; }

    // 载入：CURRENT → gen 快照 → 重放 journal → 校验不变量
    OpenReport open(Lot& lot, const Config& fallback = Config{});

    // 全量快照（归档 → 写新代 → 切换 CURRENT → 清空 journal → 清理旧代）
    bool compact(const Lot& lot, QString* err);

    // 追加日志（每次业务操作一行，不重写快照）
    bool recordEnter(const EnterResult& r, const QString& plate, VehicleType t, qint64 atMs,
                     QString* err);
    bool recordExit(const ExitResult& r, QString* err);
    bool recordConfig(const Config& cfg, qint64 atMs, QString* err);

    bool archiveExits(QString* err);
    Stats stats() const;

private:
    bool appendRecord(QJsonObject rec, QString* err);
    bool writeGeneration(const Lot& lot, qint64 gen, QString* err);
    bool writeAtomic(const QString& path, const QByteArray& data, QString* err);

    QString dir_;
    QString lotId_;
    qint64 seq_ = 0;
    qint64 snapshotSeq_ = 0;
    int journalRecords_ = 0;   // journal 中的总行数（用于状态栏展示）
    int pendingOps_ = 0;       // 尚未进入快照的有效操作数
};

} // namespace pl
