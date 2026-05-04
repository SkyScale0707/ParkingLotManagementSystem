#pragma once
#pragma comment(lib, "OpenGL32.lib")

#ifndef QTVSMAINWINDOW_PARKINGLOTMANAGEMENTSYSETM_B1
#define QTVSMAINWINDOW_PARKINGLOTMANAGEMENTSYSETM_B1

#include <QMainWindow>

#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <QString>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QMessageBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QTimer>
#include <QDateTime>
#include <QGroupBox>
#include <QApplication>
#include <QFont>
#include <QStatusBar>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QIntValidator>

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QKeyEvent>
#include <QMatrix4x4>
#include <QVector3D>
#include <QPoint>

#include <QRandomGenerator> 

#include <QIcon>

#include <QStyleFactory> 

#define TIME_RATE 100

#define CAR_SMALL_SCALE_X 0.8f
#define CAR_SMALL_SCALE_Y 0.5f
#define CAR_SMALL_SCALE_Z 1.8f
#define TRUCK_SMALL_SCALE_X 1.0f
#define TRUCK_SMALL_SCALE_Y 0.6f
#define TRUCK_SMALL_SCALE_Z 2.5f
#define TRUCK_MEDIUM_SCALE_X 1.2f
#define TRUCK_MEDIUM_SCALE_Y 0.8f
#define TRUCK_MEDIUM_SCALE_Z 3.5f
#define TRUCK_LARGE_SCALE_X 1.5f
#define TRUCK_LARGE_SCALE_Y 1.0f
#define TRUCK_LARGE_SCALE_Z 5.0f
#define PARKING_SLOT_HEIGHT 0.1f

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

typedef enum { Empty, Full } CarPortState;
typedef enum { CAR_SMALL, CAR_TRUCK_SMALL, CAR_TRUCK_MEDIUM, CAR_TRUCK_LARGE } VehicleType;

typedef struct {
    char id[32];
    CarPortState state;
    int position;
    float width, length;
    float x, y, z;
} CarPort;

typedef struct {
    char plate_number[32];
    VehicleType type;
    time_t arrival_time;
    time_t departure_time;
    int parking_position;
    bool on_waiting_queue;
    QVector3D carColor;
} VehicleInfo;

typedef struct {
    CarPort* car_ports;
    VehicleInfo** vehicles_in_lot;
    VehicleInfo** waiting_queue;
    float unit_price;
    int capacity;
    int current_count;
    int waiting_count;
    int total_vehicles;
} ParkingLot;

//================
//各类功能函数声明
//================
void initialize_parking_lot(ParkingLot* p, int capacity);

void cleanup_parking_lot(ParkingLot* p);

int find_vehicle_by_plate(ParkingLot* p, const char* plate);

int find_empty_parking_slot(ParkingLot* p);

const char* get_vehicle_type_name(VehicleType type);

bool add_vehicle(ParkingLot* p, const char* plate, VehicleType type);

bool remove_vehicle(ParkingLot* p, const char* plate);

bool save_to_file(ParkingLot* p, QString& errorMsg);

bool load_from_file(ParkingLot* p, QString& errorMsg);

double calculateParkingFee(VehicleInfo* vehicle, ParkingLot* parkingLot);

QString formatDuration(time_t arrivalTime);

class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    explicit OpenGLWidget(ParkingLot* parkingLot, QWidget* parent = nullptr);
    ~OpenGLWidget() override;

    ParkingLot* m_parkingLot = nullptr;

protected:
    virtual void initializeGL() override;
    virtual void resizeGL(int w, int h) override;
    virtual void paintGL() override;

    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void wheelEvent(QWheelEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;

private:
    bool m_isDragging;
    QPoint m_lastMousePos;
    float m_rotateX;
    float m_rotateY;
    float m_scale;
    QVector3D m_translate;
    int m_selectedCarPort;

    QOpenGLShaderProgram* m_shaderProgram;
    GLuint m_VAO;
    GLuint m_VBO;
    GLuint m_EBO;
    GLuint m_slotVAO;
    GLuint m_slotVBO;
    GLuint m_slotEBO;
    GLuint m_carVAO;
    GLuint m_carVBO;
    GLuint m_carEBO;

    QVector3D m_cameraPos;
    QVector3D m_cameraFront;
    QVector3D m_cameraUp;
    float m_cameraYaw;
    float m_cameraPitch;

    void initCamera();
    void initShader();
    void initBuffers();
    void drawParkingLot();
    void resetGraphicsState();
    void resetView();

    float m_deltaTime = 0.0f;   // 两帧之间的时间差
    float m_lastFrame = 0.0f;   // 上一帧的时间戳
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr) :QMainWindow(parent) {
        // 新增这一行：强制启用Windows视觉样式，让控件变圆润
        QApplication::setStyle(QStyleFactory::create("Fusion")); // 圆润控件样式核心
        QApplication::setFont(QFont("Microsoft YaHei", 9));      // 全局统一微软雅黑字体，清晰美观 
        // Fusion是QT自带的现代样式，也可以用"Windows"

        // 设置窗口图标
        QIcon appIcon(":/new/prefix1/creeper-outlined.ico");
        // 或者用标准路径：QIcon appIcon(":/creeper-outlined.ico");
        this->setWindowIcon(appIcon);

        initialize_parking_lot(&parkingLot, 50);
        setWindowTitle("停车场管理系统");
        setMinimumSize(768, 576);
        setMaximumSize(768, 576);

        QWidget* centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);

        QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

        QLabel* titleLabel = new QLabel("停车场管理系统", this);
        QFont titleFont = titleLabel->font();
        titleFont.setPointSize(16);
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        titleLabel->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(titleLabel);

        QTabWidget* tabWidget = new QTabWidget(this);
        mainLayout->addWidget(tabWidget);

        createParkingTab(tabWidget);
        createQueryTab(tabWidget);
        createManagementTab(tabWidget);
        createStatisticsTab(tabWidget);
        createGraphicsTab(tabWidget);

        statusBar()->showMessage("就绪");

        QTimer* timer = new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(refreshData()));
        timer->start(5000);
        refreshData();
    };
    ~MainWindow();

private slots:
    void refreshData() {
        updateParkingStatus();
        updateVehicleTables();
        updateStatistics();
    }

    void handleParking() {
        QString plate = m_plateEdit->text().trimmed();
        int typeIndex = m_vehicleTypeCombo->currentIndex();

        if (plate.isEmpty()) {
            QMessageBox::warning(this, "输入错误", "请输入车牌号");
            return;
        }

        if (typeIndex < 0) {
            QMessageBox::warning(this, "输入错误", "请选择车型");
            return;
        }

        bool success = add_vehicle(&parkingLot, plate.toUtf8().constData(), (VehicleType)typeIndex);
        if (success) {
            QMessageBox::information(this, "成功", "车辆入库成功！");
            openglWidget->update();
            refreshData();
        }
        else {
            QMessageBox::warning(this, "提示", "停车场已满，车辆加入等待队列！");
        }

        m_plateEdit->clear();
        m_vehicleTypeCombo->setCurrentIndex(-1);
        updateParkingStatus();
        updateVehicleTables();
        updateStatistics();
    }

    void handleUnparking() {
        QString plate = m_unparkPlateEdit->text().trimmed();
        if (plate.isEmpty()) {
            QMessageBox::warning(this, "输入错误", "请输入车牌号");
            return;
        }

        int pos = find_vehicle_by_plate(&parkingLot, plate.toUtf8().constData());
        if (pos == -1) {
            QMessageBox::warning(this, "操作失败", "未找到该车辆");
            m_unparkPlateEdit->clear();
            return;
        }
        if (pos == -2) {
            QMessageBox::warning(this, "操作失败", "该车辆在等待队列中，无法出库");
            m_unparkPlateEdit->clear();
            return;
        }

        VehicleInfo* vehicle = parkingLot.vehicles_in_lot[pos];
        double fee = calculateParkingFee(vehicle, &parkingLot);
        QString duration = formatDuration(vehicle->arrival_time);
        QString VehicleType;
        switch (vehicle->type) {
        case CAR_SMALL:
            VehicleType = "小汽车"; break;
        case CAR_TRUCK_SMALL:
            VehicleType = "小卡"; break;
        case CAR_TRUCK_MEDIUM:
            VehicleType = "中卡"; break;
        case CAR_TRUCK_LARGE:
            VehicleType = "大卡"; break;
        }

        bool success = remove_vehicle(&parkingLot, plate.toUtf8().constData());
        if (success) {
            QString saveErrorMsg;
            bool saveSuccess = save_to_file(&parkingLot, saveErrorMsg);
            QString msg = QString("✅ 取车成功！\n\n")
                + QString("车牌号：%1\n").arg(plate)
                + QString("车型：%1\n").arg(VehicleType)
                + QString("停车时长：%1\n").arg(duration)
                + QString("应付金额：¥%1\n").arg(fee, 0, 'f', 2)
                + QString("\n感谢您的使用！");
            QMessageBox::information(this, "取车完成", msg);
            if (openglWidget) {
                openglWidget->update();
            }
            refreshData();
            m_unparkPlateEdit->clear();
        }
        else {
            QMessageBox::warning(this, "失败", "车辆出库失败！");
        }
        refreshData();
    }

    void handleQuery() {
        m_queryResult->clear();
        QString plate = m_queryPlateEdit->text().trimmed();
        int typeIndex = m_queryTypeCombo->currentIndex();

        if (!plate.isEmpty()) {
            int pos = find_vehicle_by_plate(&parkingLot, plate.toUtf8().constData());
            if (pos >= 0) {
                VehicleInfo* v = parkingLot.vehicles_in_lot[pos];
                QDateTime arrivalTime = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(v->arrival_time));
                m_queryResult->append(QString("车牌号: %1").arg(v->plate_number));
                m_queryResult->append(QString("车型: %1").arg(get_vehicle_type_name(v->type)));
                m_queryResult->append(QString("车位: %1").arg(parkingLot.car_ports[pos].id));
                m_queryResult->append(QString("到达时间: %1").arg(arrivalTime.toString()));
            }
            else if (pos == -2) {
                m_queryResult->append(QString("车牌号: %1").arg(plate));
                m_queryResult->append("状态: 便道中等待");
            }
            else {
                m_queryResult->append("未找到匹配的车辆");
            }
        }
        else if (typeIndex >= 0) {
            bool found = false;
            for (int i = 0; i < parkingLot.capacity; i++) {
                if (parkingLot.vehicles_in_lot[i] &&
                    parkingLot.vehicles_in_lot[i]->type == (VehicleType)typeIndex) {
                    VehicleInfo* v = parkingLot.vehicles_in_lot[i];
                    QDateTime arrivalTime = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(v->arrival_time));
                    m_queryResult->append(QString("车牌号: %1").arg(v->plate_number));
                    m_queryResult->append(QString("车型: %1").arg(get_vehicle_type_name(v->type)));
                    m_queryResult->append(QString("车位: %1").arg(parkingLot.car_ports[i].id));
                    m_queryResult->append(QString("到达时间: %1").arg(arrivalTime.toString()));
                    m_queryResult->append("---------------------------------");
                    found = true;
                }
            }
            if (!found) {
                m_queryResult->append("未找到匹配的车辆");
            }
        }
        else {
            QMessageBox::warning(this, "输入错误", "请输入车牌号或选择车型");
        }
    }

    void handleSaveData() {
        QString errorMsg;
        bool state = save_to_file(&parkingLot, errorMsg);
        if (state == 1)QMessageBox::information(this, "操作成功", "数据已保存到 parking_data.txt");
        else QMessageBox::critical(this, "保存失败", errorMsg);
    }

    void handleLoadData() {
        QString errorMsg;
        bool state = load_from_file(&parkingLot, errorMsg);
        if (state == 1) {
            QMessageBox::information(this, "操作成功", "数据已从 parking_data.txt 加载");
            updateParkingStatus();
            updateVehicleTables();
            updateStatistics();
        }
        else {
            QMessageBox::warning(this, "读取失败", errorMsg);
        }
    };

private:
    ParkingLot parkingLot;
    OpenGLWidget* openglWidget;

    QLineEdit* m_plateEdit;
    QComboBox* m_vehicleTypeCombo;
    QLineEdit* m_unparkPlateEdit;
    QLineEdit* m_priceEdit;
    QLineEdit* m_queryPlateEdit;
    QComboBox* m_queryTypeCombo;
    QTextEdit* m_queryResult;
    QTableWidget* m_parkedTable;
    QTableWidget* m_waitingTable;
    QLabel* m_totalVehiclesLabel;
    QLabel* m_parkedVehiclesLabel;
    QLabel* m_waitingVehiclesLabel;
    QLabel* m_emptySlotsLabel;
    QLabel* m_smallCarLabel;
    QLabel* m_smallTruckLabel;
    QLabel* m_mediumTruckLabel;
    QLabel* m_largeTruckLabel;
    QLabel* initLabel;
    QLineEdit* initCarPortEdit;
    QPushButton* initCarPortBtn;

    void createParkingTab(QTabWidget* tabWidget) {
        QWidget* parkingWidget = new QWidget(tabWidget);
        QVBoxLayout* layout = new QVBoxLayout(parkingWidget);
        layout->setSpacing(10);
        layout->setContentsMargins(10, 10, 10, 10);

        QGroupBox* initLotSpaceGroup = new QGroupBox("初始化停车场大小", parkingWidget);
        QFormLayout* initLotSpaceForm = new QFormLayout(initLotSpaceGroup);
        QWidget* initCarPortWidget = new QWidget(initLotSpaceGroup);
        QHBoxLayout* initLayout = new QHBoxLayout(initCarPortWidget);
        initLayout->setSpacing(8);
        initLayout->setContentsMargins(0, 0, 0, 10);

        initLabel = new QLabel("停车场车位数量初始化\n默认50个车位：", initLotSpaceGroup);
        initLabel->setFixedWidth(140);
        initCarPortEdit = new QLineEdit(initLotSpaceGroup);
        initCarPortEdit->setPlaceholderText("请输入一个在1到1000之间的整数：");
        initCarPortEdit->setFixedWidth(230);
        initCarPortEdit->setValidator(new QIntValidator(1, 999, initLotSpaceGroup));
        initCarPortBtn = new QPushButton("输入后点击进行初始化车位", initLotSpaceGroup);
        initCarPortBtn->setFixedWidth(200);

        initLayout->addWidget(initLabel);
        initLayout->addWidget(initCarPortEdit);
        initLayout->addWidget(initCarPortBtn);
        initLayout->addStretch();
        initLotSpaceForm->addRow(initCarPortWidget);
        layout->addWidget(initLotSpaceGroup);

        connect(initCarPortBtn, &QPushButton::clicked, this, [=]()
            {
                QString inputText = initCarPortEdit->text().trimmed();
                if (inputText.isEmpty())
                {
                    QMessageBox::warning(this, "提示", "请输入车位数量后再点击初始化！");
                    return;
                }
                int newCarPortCount = inputText.toInt();
                initialize_parking_lot(&parkingLot, newCarPortCount);
                m_parkedTable->setRowCount(0);
                openglWidget->m_parkingLot = &parkingLot;
                openglWidget->update();
                QMessageBox::information(this, "初始化成功", QString("停车场已初始化完成！\n当前车位总数：%1 个").arg(newCarPortCount));
                initCarPortEdit->clear();
            });

        QGroupBox* parkingGroup = new QGroupBox("车辆入库", parkingWidget);
        QFormLayout* parkingForm = new QFormLayout(parkingGroup);

        m_plateEdit = new QLineEdit(parkingGroup);
        m_vehicleTypeCombo = new QComboBox(parkingGroup);
        m_vehicleTypeCombo->addItems({ "小汽车", "小卡", "中卡", "大卡" });
        m_vehicleTypeCombo->setCurrentIndex(-1);

        parkingForm->addRow("车牌号:", m_plateEdit);
        parkingForm->addRow("车型:", m_vehicleTypeCombo);

        QPushButton* parkButton = new QPushButton("确认入库", parkingGroup);
        connect(parkButton, SIGNAL(clicked()), this, SLOT(handleParking()));
        parkingForm->addRow(parkButton);

        QGroupBox* unparkingGroup = new QGroupBox("车辆出库", parkingWidget);
        QFormLayout* unparkingForm = new QFormLayout(unparkingGroup);

        m_unparkPlateEdit = new QLineEdit(unparkingGroup);
        unparkingForm->addRow("车牌号:", m_unparkPlateEdit);

        QPushButton* unparkButton = new QPushButton("确认出库", unparkingGroup);
        connect(unparkButton, SIGNAL(clicked()), this, SLOT(handleUnparking()));
        unparkingForm->addRow(unparkButton);

        QGroupBox* priceGroup = new QGroupBox("停车费设置", parkingWidget);
        QHBoxLayout* priceLayout = new QHBoxLayout(priceGroup);

        m_priceEdit = new QLineEdit(QString::number(parkingLot.unit_price), priceGroup);
        m_priceEdit->setValidator(new QDoubleValidator(0.01, 100.0, 2, this));

        QPushButton* updatePriceBtn = new QPushButton("更新单价", priceGroup);
        connect(updatePriceBtn, &QPushButton::clicked, this, [=]() {
            bool ok;
            double newPrice = m_priceEdit->text().toDouble(&ok);
            if (ok && newPrice > 0) {
                parkingLot.unit_price = newPrice;
                QMessageBox::information(this, "成功", "停车费单价已更新");
            }
            else {
                QMessageBox::warning(this, "错误", "请输入有效的单价");
                m_priceEdit->setText(QString::number(parkingLot.unit_price));
            }
            });

        priceLayout->addWidget(new QLabel("每小时停车费(元):", priceGroup));
        priceLayout->addWidget(m_priceEdit);
        priceLayout->addWidget(updatePriceBtn);

        layout->addWidget(parkingGroup);
        layout->addWidget(unparkingGroup);
        layout->addWidget(priceGroup);
        layout->addStretch();

        tabWidget->addTab(parkingWidget, "停车操作");
    }

    void createQueryTab(QTabWidget* tabWidget) {
        QWidget* queryWidget = new QWidget(tabWidget);
        QVBoxLayout* layout = new QVBoxLayout(queryWidget);

        QGroupBox* queryGroup = new QGroupBox("车辆查询", queryWidget);
        QFormLayout* queryForm = new QFormLayout(queryGroup);

        m_queryPlateEdit = new QLineEdit(queryGroup);
        m_queryTypeCombo = new QComboBox(queryGroup);
        m_queryTypeCombo->addItems({ "小汽车", "小卡", "中卡", "大卡" });
        m_queryTypeCombo->setCurrentIndex(-1);
        m_queryTypeCombo->setPlaceholderText("选择车型(可选)");

        queryForm->addRow("车牌号(可选):", m_queryPlateEdit);
        queryForm->addRow("车型(可选):", m_queryTypeCombo);

        QPushButton* queryButton = new QPushButton("查询", queryGroup);
        connect(queryButton, SIGNAL(clicked()), this, SLOT(handleQuery()));
        queryForm->addRow(queryButton);

        m_queryResult = new QTextEdit(queryWidget);
        m_queryResult->setReadOnly(true);

        layout->addWidget(queryGroup);
        layout->addWidget(new QLabel("查询结果:", queryWidget));
        layout->addWidget(m_queryResult);

        tabWidget->addTab(queryWidget, "车辆查询");
    }

    void createManagementTab(QTabWidget* tabWidget) {
        QWidget* managementWidget = new QWidget(tabWidget);
        QVBoxLayout* layout = new QVBoxLayout(managementWidget);

        QHBoxLayout* buttonLayout = new QHBoxLayout();
        QPushButton* saveButton = new QPushButton("保存数据", managementWidget);
        QPushButton* loadButton = new QPushButton("加载数据", managementWidget);
        connect(saveButton, SIGNAL(clicked()), this, SLOT(handleSaveData()));
        connect(loadButton, SIGNAL(clicked()), this, SLOT(handleLoadData()));
        buttonLayout->addWidget(saveButton);
        buttonLayout->addWidget(loadButton);

        QLabel* parkedLabel = new QLabel("停车场内车辆:", managementWidget);
        m_parkedTable = new QTableWidget(0, 4, managementWidget);
        m_parkedTable->setHorizontalHeaderLabels({ "车牌号", "车型", "车位", "到达时间" });
        m_parkedTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

        QLabel* waitingLabel = new QLabel("等待队列车辆:", managementWidget);
        m_waitingTable = new QTableWidget(0, 3, managementWidget);
        m_waitingTable->setHorizontalHeaderLabels({ "车牌号", "车型", "到达时间" });
        m_waitingTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

        layout->addLayout(buttonLayout);
        layout->addWidget(parkedLabel);
        layout->addWidget(m_parkedTable);
        layout->addWidget(waitingLabel);
        layout->addWidget(m_waitingTable);

        tabWidget->addTab(managementWidget, "车辆管理");
    }

    void createStatisticsTab(QTabWidget* tabWidget) {
        QWidget* statsWidget = new QWidget(tabWidget);
        QVBoxLayout* layout = new QVBoxLayout(statsWidget);

        QGroupBox* statsGroup = new QGroupBox("统计信息", statsWidget);
        QVBoxLayout* statsLayout = new QVBoxLayout(statsGroup);

        m_totalVehiclesLabel = new QLabel("历史停放总车辆数: 0", statsGroup);
        m_parkedVehiclesLabel = new QLabel("停车场内: 0辆", statsGroup);
        m_waitingVehiclesLabel = new QLabel("便道等待: 0辆", statsGroup);
        m_emptySlotsLabel = new QLabel("空余车位: 0个", statsGroup);

        QLabel* typeLabel = new QLabel("车型分布:", statsGroup);
        m_smallCarLabel = new QLabel("  小汽车: 0辆", statsGroup);
        m_smallTruckLabel = new QLabel("  小卡: 0辆", statsGroup);
        m_mediumTruckLabel = new QLabel("  中卡: 0辆", statsGroup);
        m_largeTruckLabel = new QLabel("  大卡: 0辆", statsGroup);

        statsLayout->addWidget(m_totalVehiclesLabel);
        statsLayout->addWidget(m_parkedVehiclesLabel);
        statsLayout->addWidget(m_waitingVehiclesLabel);
        statsLayout->addWidget(m_emptySlotsLabel);
        statsLayout->addSpacing(20);
        statsLayout->addWidget(typeLabel);
        statsLayout->addWidget(m_smallCarLabel);
        statsLayout->addWidget(m_smallTruckLabel);
        statsLayout->addWidget(m_mediumTruckLabel);
        statsLayout->addWidget(m_largeTruckLabel);

        layout->addWidget(statsGroup);

        tabWidget->addTab(statsWidget, "统计信息");
    }

    void createGraphicsTab(QTabWidget* tabWidget) {
        QWidget* graphicsWidget = new QWidget(tabWidget);
        QVBoxLayout* layout = new QVBoxLayout(graphicsWidget);

        openglWidget = new OpenGLWidget(&parkingLot, graphicsWidget);
        layout->addWidget(openglWidget);

        QLabel* infoLabel = new QLabel("操作说明:\n\n"
            "W/A/S/D - 前后左右移动\n"
            "空格/Ctrl - 上升/下降\n"
            "鼠标右键拖动 - 旋转视角\n"
            "鼠标滚轮 - 缩放\n"
            "R键 - 重置视角\n"
            "ESC键 - 返回主菜单", graphicsWidget);
        layout->addWidget(infoLabel);

        tabWidget->addTab(graphicsWidget, "图形视图");
    }

    void updateParkingStatus() {
        if (!m_parkedVehiclesLabel || !m_waitingVehiclesLabel || !m_emptySlotsLabel) {
            qWarning("状态标签未初始化，更新失败！");
            return;
        }
        m_parkedVehiclesLabel->setText(QString("当前停放: %1辆").arg(this->parkingLot.current_count));
        m_waitingVehiclesLabel->setText(QString("等待车辆: %1辆").arg(this->parkingLot.waiting_count));
        m_emptySlotsLabel->setText(QString("空余车位: %1个").arg(this->parkingLot.capacity - this->parkingLot.current_count));
    }

    void updateVehicleTables() {
        m_parkedTable->setRowCount(0);
        for (int i = 0; i < parkingLot.capacity; i++) {
            if (parkingLot.vehicles_in_lot[i] != nullptr) {
                VehicleInfo* v = parkingLot.vehicles_in_lot[i];
                int row = m_parkedTable->rowCount();
                m_parkedTable->insertRow(row);

                m_parkedTable->setItem(row, 0, new QTableWidgetItem(v->plate_number));
                m_parkedTable->setItem(row, 1, new QTableWidgetItem(get_vehicle_type_name(v->type)));
                m_parkedTable->setItem(row, 2, new QTableWidgetItem(parkingLot.car_ports[i].id));

                QDateTime arrivalTime = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(v->arrival_time));
                m_parkedTable->setItem(row, 3, new QTableWidgetItem(arrivalTime.toString()));
            }
        }

        m_waitingTable->setRowCount(0);
        for (int i = 0; i < parkingLot.waiting_count; i++) {
            if (parkingLot.waiting_queue[i]) {
                VehicleInfo* v = parkingLot.waiting_queue[i];
                int row = m_waitingTable->rowCount();
                m_waitingTable->insertRow(row);

                m_waitingTable->setItem(row, 0, new QTableWidgetItem(v->plate_number));
                m_waitingTable->setItem(row, 1, new QTableWidgetItem(get_vehicle_type_name(v->type)));

                QDateTime arrivalTime = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(v->arrival_time));
                m_waitingTable->setItem(row, 2, new QTableWidgetItem(arrivalTime.toString()));
            }
        }
    }

    void updateStatistics() {
        int smallCar = 0, smallTruck = 0, mediumTruck = 0, largeTruck = 0;
        for (int i = 0; i < parkingLot.capacity; i++) {
            if (parkingLot.vehicles_in_lot[i] != nullptr) {
                switch (parkingLot.vehicles_in_lot[i]->type) {
                case CAR_SMALL: smallCar++; break;
                case CAR_TRUCK_SMALL: smallTruck++; break;
                case CAR_TRUCK_MEDIUM: mediumTruck++; break;
                case CAR_TRUCK_LARGE: largeTruck++; break;
                }
            }
        }
        for (int i = 0; i < parkingLot.waiting_count; i++) {
            if (parkingLot.waiting_queue[i]) {
                switch (parkingLot.waiting_queue[i]->type) {
                case CAR_SMALL: smallCar++; break;
                case CAR_TRUCK_SMALL: smallTruck++; break;
                case CAR_TRUCK_MEDIUM: mediumTruck++; break;
                case CAR_TRUCK_LARGE: largeTruck++; break;
                }
            }
        }

        m_totalVehiclesLabel->setText(QString("历史停放总车辆数: %1").arg(parkingLot.total_vehicles));
        m_parkedVehiclesLabel->setText(QString("停车场内: %1辆").arg(parkingLot.current_count));
        m_waitingVehiclesLabel->setText(QString("便道等待: %1辆").arg(parkingLot.waiting_count));
        m_emptySlotsLabel->setText(QString("空余车位: %1个").arg(parkingLot.capacity - parkingLot.current_count));
        m_smallCarLabel->setText(QString("  小汽车: %1辆").arg(smallCar));
        m_smallTruckLabel->setText(QString("  小卡: %1辆").arg(smallTruck));
        m_mediumTruckLabel->setText(QString("  中卡: %1辆").arg(mediumTruck));
        m_largeTruckLabel->setText(QString("  大卡: %1辆").arg(largeTruck));
    }
};

#endif // MAINWINDOW_H