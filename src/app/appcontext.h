#pragma once
// ---------------------------------------------------------------------------
// appcontext.h — 应用上下文：拥有领域模型 + 仓储，负责业务操作与持久化
//
// 所有写操作都遵循同一顺序：先改内存 → 追加 journal → 失败则精确回滚。
// ---------------------------------------------------------------------------
#include "domain/lot.h"
#include "persistence/lotstore.h"

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

#include <memory>

class QLockFile;

namespace app {

struct ExitRecord {
    QString plate;
    QString ticket;
    pl::VehicleType type = pl::VehicleType::CarSmall;
    int slot = -1;
    qint64 enteredAtMs = 0;
    qint64 exitedAtMs = 0;
    qint64 stayMs = 0;
    qint64 feeCents = 0;
};

class AppContext : public QObject
{
    Q_OBJECT

public:
    explicit AppContext(QString dataDir, QObject* parent = nullptr);
    ~AppContext() override;

    bool open(QString* error);
    bool isReadOnly() const { return readOnly_; }
    bool isOpen() const { return opened_; }
    QString dataDir() const { return dataDir_; }

    const pl::Lot& lot() const { return lot_; }
    const pl::LotStore& store() const { return store_; }
    pl::Config config() const { return lot_.cfg; }

    // 业务操作。返回结果的同时，*error 非空表示持久化失败（内存已回滚）。
    pl::EnterResult enterVehicle(const QString& plate, pl::VehicleType t, QString* error);
    pl::ExitResult leaveVehicle(const QString& plate, QString* error);
    bool applyConfig(const pl::Config& cfg, QString* error);
    bool compactNow(QString* error);
    bool exportHistoryCsv(const QString& path, QString* error) const;

    QList<ExitRecord> recentExits(int limit) const;
    QList<ExitRecord> allExits() const;
    QMap<QString, qint64> monthlyRevenue() const;

    // 生成演示数据（仅用于 --demo / 截图；要求当前库为空）
    bool seedDemo(QString* error);

signals:
    void changed();

private:
    QList<ExitRecord> readHistoryFromDisk() const;
    void reloadHistory();

    QString dataDir_;
    pl::Lot lot_;
    pl::LotStore store_;
    bool readOnly_ = false;
    bool opened_ = false;
    std::unique_ptr<QLockFile> lock_;
    QList<ExitRecord> history_;
    QList<ExitRecord> session_;
};

} // namespace app
