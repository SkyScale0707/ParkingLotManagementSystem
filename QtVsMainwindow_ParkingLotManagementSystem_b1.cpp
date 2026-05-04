#pragma comment(lib, "OpenGL32.lib")

#include "QtVsMainwindow_ParkingLotManagementSystem_b1.h"

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
#include <QCoreApplication>
#include <QDir>
#include <cstring>
#include <ctime>
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

void initialize_parking_lot(ParkingLot* p, int capacity) {
    if (p->car_ports != nullptr) {
        delete[] p->car_ports;
        p->car_ports = nullptr;
    }
    if (p->vehicles_in_lot != nullptr) {
        delete[] p->vehicles_in_lot;
        p->vehicles_in_lot = nullptr;
    }
    if (p->waiting_queue != nullptr) {
        delete[] p->waiting_queue;
        p->waiting_queue = nullptr;
    }
    p->capacity = capacity;
    p->vehicles_in_lot = new VehicleInfo * [capacity]();
    p->waiting_queue = new VehicleInfo * [100]();
    p->current_count = 0;
    p->waiting_count = 0;
    p->total_vehicles = 0;
    p->unit_price = 5.0f;

    p->car_ports = new CarPort[capacity];
    for (int i = 0; i < capacity; i++) {
        sprintf_s(p->car_ports[i].id, "P%03d", i + 1);
        p->car_ports[i].state = Empty;
        p->car_ports[i].position = i;
        p->car_ports[i].x = (i % 5) * 2.0f - 4.0f;
        p->car_ports[i].y = 0.0f;
        p->car_ports[i].z = (i / 5) * 2.5f - 5.0f;
        p->car_ports[i].width = 2.5f;
        p->car_ports[i].length = 5.0f;
    }
};

void cleanup_parking_lot(ParkingLot* p) {
    for (int i = 0; i < p->capacity; i++) {
        if (p->vehicles_in_lot[i]) {
            delete p->vehicles_in_lot[i];
        }
    }
    delete[] p->vehicles_in_lot;
    p->vehicles_in_lot = nullptr;

    for (int i = 0; i < p->waiting_count; i++) {
        if (p->waiting_queue[i]) {
            delete p->waiting_queue[i];
        }
    }
    delete[] p->waiting_queue;
    p->waiting_queue = nullptr;

    delete[] p->car_ports;
    p->car_ports = nullptr;

    p->capacity = 0;
    p->current_count = 0;
    p->waiting_count = 0;
    p->total_vehicles = 0;
};

int find_vehicle_by_plate(ParkingLot* p, const char* plate) {
    for (int i = 0; i < p->capacity; i++) {
        if (p->vehicles_in_lot[i] && strcmp(p->vehicles_in_lot[i]->plate_number, plate) == 0) {
            return i;
        }
    }

    for (int i = 0; i < p->waiting_count; i++) {
        if (p->waiting_queue[i] && strcmp(p->waiting_queue[i]->plate_number, plate) == 0) {
            return -2;
        }
    }

    return -1;
}

int find_empty_parking_slot(ParkingLot* p) {
    for (int i = 0; i < p->capacity; i++) {
        if (p->car_ports[i].state == Empty) {
            return i;
        }
    }
    return -1;
};

const char* get_vehicle_type_name(VehicleType type) {
    switch (type) {
    case CAR_SMALL: return "小汽车";
    case CAR_TRUCK_SMALL: return "小卡";
    case CAR_TRUCK_MEDIUM: return "中卡";
    case CAR_TRUCK_LARGE: return "大卡";
    default: return "未知车型";
    }
};

bool add_vehicle(ParkingLot* p, const char* plate, VehicleType type) {
    if (p == nullptr) {
        qWarning() << "ParkingLot pointer is null!";
        return false;
    }
    if (find_vehicle_by_plate(p, plate) != -1) {
        qWarning() << "Vehicle with plate " << plate << " already exists!";
        return false;
    }

    VehicleInfo* vehicle = new VehicleInfo();
    strncpy_s(vehicle->plate_number, plate, 31);
    vehicle->plate_number[31] = '\0';
    vehicle->type = type;
    vehicle->arrival_time = time(NULL);
    vehicle->departure_time = 0;
    vehicle->on_waiting_queue = false;
    vehicle->parking_position = -1; //【★核心修复1★】初始化车位号，避免脏数据

    // 初始化随机颜色，绝对不能为空，绘制时会读这个字段
    vehicle->carColor = QVector3D(
        QRandomGenerator::global()->generateDouble(),
        QRandomGenerator::global()->generateDouble(),
        QRandomGenerator::global()->generateDouble()
    );

    int slot = find_empty_parking_slot(p);
    if (slot >= 0) {
        vehicle->parking_position = slot;
        vehicle->on_waiting_queue = false;
        p->vehicles_in_lot[slot] = vehicle;
        p->car_ports[slot].state = Full;
        p->current_count++;
        p->total_vehicles++;
        return true;
    }
    else {
        if (p->waiting_count < 100) {
            vehicle->on_waiting_queue = true;
            p->waiting_queue[p->waiting_count] = vehicle;
            p->waiting_count++;
            p->total_vehicles++;
            return false;
        }
    }
    return false;
};

bool remove_vehicle(ParkingLot* p, const char* plate) {
    int pos = find_vehicle_by_plate(p, plate);
    if (pos == -1) {
        return false;
    }

    if (pos >= 0) {
        delete p->vehicles_in_lot[pos];
        p->vehicles_in_lot[pos] = NULL;
        p->car_ports[pos].state = Empty;
        p->current_count--;

        if (p->waiting_count > 0) {
            VehicleInfo* wait_vehicle = p->waiting_queue[0];
            wait_vehicle->on_waiting_queue = false;
            wait_vehicle->parking_position = pos;
            p->vehicles_in_lot[pos] = wait_vehicle;
            p->car_ports[pos].state = Full;
            p->current_count++;

            for (int i = 0; i < p->waiting_count - 1; i++) {
                p->waiting_queue[i] = p->waiting_queue[i + 1];
            }
            p->waiting_queue[p->waiting_count - 1] = NULL;
            p->waiting_count--;
        }
    }
    else if (pos == -2) {
        for (int i = 0; i < p->waiting_count; i++) {
            if (p->waiting_queue[i] && strcmp(p->waiting_queue[i]->plate_number, plate) == 0) {
                delete p->waiting_queue[i];
                for (int j = i; j < p->waiting_count - 1; j++) {
                    p->waiting_queue[j] = p->waiting_queue[j + 1];
                }
                p->waiting_queue[p->waiting_count - 1] = NULL;
                p->waiting_count--;
                break;
            }
        }
    }

    return true;
};

bool save_to_file(ParkingLot* p, QString& errorMsg) {
    if (!p) {
        errorMsg = "停车场为空！保存白费力气！";
        return false;
    }

    QString appDir = QCoreApplication::applicationDirPath();
    QString filePath = QDir(appDir).filePath("parking_data.txt");

    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        errorMsg = QString("文件打开失败：%1\n路径：%2").arg(file.errorString()).arg(filePath);
        return false;
    }

    QTextStream out(&file);

    out << p->capacity << "\n"
        << p->current_count << "\n"
        << p->waiting_count << "\n"
        << p->total_vehicles << "\n"
        << 100 << "\n";

    out << "PARKED_VEHICLES_START\n";
    int parkedCount = 0;
    for (int i = 0; i < p->capacity; i++) {
        if (p->vehicles_in_lot[i]) {
            out << p->vehicles_in_lot[i]->plate_number << "\n"
                << (int)p->vehicles_in_lot[i]->type << "\n"
                << (qint64)p->vehicles_in_lot[i]->arrival_time << "\n"
                << p->vehicles_in_lot[i]->parking_position << "\n";
            parkedCount++;
        }
    }
    out << "PARKED_VEHICLES_END\n";

    out << "WAITING_VEHICLES_START\n";
    int waitingCount = 0;
    for (int i = 0; i < 100 && i < p->waiting_count; i++) {
        if (p->waiting_queue[i]) {
            out << p->waiting_queue[i]->plate_number << "\n"
                << (int)p->waiting_queue[i]->type << "\n"
                << (qint64)p->waiting_queue[i]->arrival_time << "\n";
            waitingCount++;
        }
    }
    out << "WAITING_VEHICLES_END\n";

    out.flush();
    file.close();

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || fileInfo.size() == 0) {
        errorMsg = QString("数据写入失败：文件为空\n路径：%1").arg(filePath);
        return false;
    }

    errorMsg = QString("保存成功！\n路径：%1\n场内车辆：%2 辆\n等待队列：%3 辆")
        .arg(filePath).arg(parkedCount).arg(waitingCount);
    return true;
};

bool load_from_file(ParkingLot* p, QString& errorMsg) {
    if (!p) {
        errorMsg = "停车场实例为空！";
        return false;
    }

    QString appDir = QCoreApplication::applicationDirPath();
    QString filePath = QDir(appDir).filePath("parking_data.txt");

    QFile file(filePath);
    if (!file.exists()) {
        errorMsg = QString("文件不存在！\n路径：%1").arg(filePath);
        return false;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = QString("文件打开失败：%1\n路径：%2").arg(file.errorString()).arg(filePath);
        return false;
    }

    QTextStream in(&file);

    QStringList lines = in.readAll().split("\n", Qt::SkipEmptyParts);
    file.close();

    if (lines.isEmpty()) {
        errorMsg = QString("文件为空！\n路径：%1").arg(filePath);
        return false;
    }

    cleanup_parking_lot(p);

    int lineIdx = 0;
    if (lines.size() < 5) {
        errorMsg = "文件格式错误：基本信息缺失！";
        initialize_parking_lot(p, 50);
        return false;
    }

    bool ok;
    int capacity = lines[lineIdx++].toInt(&ok);
    if (!ok || capacity <= 0) { capacity = 50; }
    int current_count = lines[lineIdx++].toInt(&ok);
    if (!ok) { current_count = 0; }
    int waiting_count = lines[lineIdx++].toInt(&ok);
    if (!ok) { waiting_count = 0; }
    int total_vehicles = lines[lineIdx++].toInt(&ok);
    if (!ok) { total_vehicles = 0; }
    int waiting_capacity = lines[lineIdx++].toInt(&ok);
    if (!ok || waiting_capacity <= 0) { waiting_capacity = 100; }

    initialize_parking_lot(p, capacity);
    p->current_count = current_count;
    p->waiting_count = waiting_count;
    p->total_vehicles = total_vehicles;

    while (lineIdx < lines.size() && lines[lineIdx] != "PARKED_VEHICLES_START") { lineIdx++; }
    if (lineIdx >= lines.size()) {
        errorMsg = "文件格式错误：未找到场内车辆起始标记！";
        return false;
    }
    lineIdx++;

    int loadedParked = 0;
    while (lineIdx < lines.size() && lines[lineIdx] != "PARKED_VEHICLES_END") {
        if (lineIdx + 3 >= lines.size()) { break; }

        QString plate = lines[lineIdx++];
        int type = lines[lineIdx++].toInt(&ok);
        if (!ok) { continue; }
        qint64 arrival = lines[lineIdx++].toLongLong(&ok);
        if (!ok) { continue; }
        int pos = lines[lineIdx++].toInt(&ok);
        if (!ok || pos < 0 || pos >= p->capacity) { continue; }

        VehicleInfo* vehicle = new VehicleInfo();
        strncpy_s(vehicle->plate_number, plate.toUtf8().constData(), 31);
        vehicle->plate_number[31] = '\0';
        vehicle->type = (VehicleType)type;
        vehicle->arrival_time = (time_t)arrival;
        vehicle->parking_position = pos;
        vehicle->on_waiting_queue = false;
        vehicle->departure_time = 0;
        //【★核心修复2★】加载数据时必须初始化颜色，否则绘制时读空指针崩溃
        vehicle->carColor = QVector3D(
            QRandomGenerator::global()->generateDouble(),
            QRandomGenerator::global()->generateDouble(),
            QRandomGenerator::global()->generateDouble()
        );

        p->vehicles_in_lot[pos] = vehicle;
        p->car_ports[pos].state = Full;
        loadedParked++;
    }

    while (lineIdx < lines.size() && lines[lineIdx] != "WAITING_VEHICLES_START") { lineIdx++; }
    if (lineIdx >= lines.size()) {
        errorMsg = "文件格式错误：未找到等待车辆起始标记！";
        return false;
    }
    lineIdx++;

    int loadedWaiting = 0;
    while (lineIdx < lines.size() && lines[lineIdx] != "WAITING_VEHICLES_END" && loadedWaiting < waiting_capacity) {
        if (lineIdx + 2 >= lines.size()) { break; }

        QString plate = lines[lineIdx++];
        int type = lines[lineIdx++].toInt(&ok);
        if (!ok) { continue; }
        qint64 arrival = lines[lineIdx++].toLongLong(&ok);
        if (!ok) { continue; }

        VehicleInfo* vehicle = new VehicleInfo();
        strncpy_s(vehicle->plate_number, plate.toUtf8().constData(), 31);
        vehicle->plate_number[31] = '\0';
        vehicle->type = (VehicleType)type;
        vehicle->arrival_time = (time_t)arrival;
        vehicle->on_waiting_queue = true;
        vehicle->departure_time = 0;
        vehicle->parking_position = -1;

        p->waiting_queue[loadedWaiting++] = vehicle;
    }

    errorMsg = QString("读取成功！\n路径：%1\n加载场内车辆：%2 辆\n加载等待队列：%3 辆")
        .arg(filePath).arg(loadedParked).arg(loadedWaiting);
    return true;
};

double calculateParkingFee(VehicleInfo* vehicle, ParkingLot* parkingLot) {
    if (!vehicle) return 0.0;

    time_t currentTime = time(NULL);
    double hours = difftime(currentTime, vehicle->arrival_time) / 3600.0;
    int roundedHours = (int)ceil(hours);
    return roundedHours * parkingLot->unit_price;
}

QString formatDuration(time_t arrivalTime)
{
    time_t now = time(nullptr);
    double totalSeconds = TIME_RATE * difftime(now, arrivalTime);
    int hours = (int)(totalSeconds / 3600);
    int minutes = (int)((totalSeconds - hours * 3600) / 60);
    if (hours == 0) {
        return QString("%1分钟").arg(minutes);
    }
    else {
        return QString("%1小时%2分钟").arg(hours).arg(minutes);
    }
}

//【★核心修复3★】重新定义车辆/车位顶点数据，修正顶点步长和索引数量，完全匹配着色器！
// 车位顶点数据 (位置+颜色，6个float/顶点，4顶点，6索引)
const float PARKING_SLOT_VERTICES[] = {
    -0.5f, 0.0f, -0.5f,   0.0f, 0.0f, 1.0f,
     0.5f, 0.0f, -0.5f,   0.0f, 0.0f, 1.0f,
     0.5f, 0.0f,  0.5f,   0.0f, 0.0f, 1.0f,
    -0.5f, 0.0f,  0.5f,   0.0f, 0.0f, 1.0f
};
const unsigned int PARKING_SLOT_INDICES[] = { 0,1,2, 2,3,0 };
const int PARKING_SLOT_INDEX_COUNT = 6;

// 车辆顶点数据 (立方体：8顶点，位置+颜色，6个float/顶点，★修正索引数量为36★)
const float CAR_VERTICES[] = {
    -0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f
};
const unsigned int CAR_INDICES[] = {
    0,1,2, 2,3,0, // 前
    4,5,6, 6,7,4, // 后
    0,1,5, 5,4,0, // 下
    3,2,6, 6,7,3, // 上
    0,3,7, 7,4,0, // 左
    1,2,6, 6,5,1  // 右
};
const int CAR_INDEX_COUNT = 36; // 固定死，绝对正确

OpenGLWidget::OpenGLWidget(ParkingLot* parkingLot, QWidget* parent)
    : QOpenGLWidget(parent),
    m_parkingLot(parkingLot),
    m_isDragging(false),
    m_rotateX(0.0f),
    m_rotateY(0.0f),
    m_scale(8.0f),
    m_translate(QVector3D(0, 0, 0)),
    m_selectedCarPort(-1),
    m_shaderProgram(nullptr),
    m_VAO(0), m_VBO(0), m_EBO(0),
    m_slotVAO(0), m_slotVBO(0), m_slotEBO(0),
    m_carVAO(0), m_carVBO(0), m_carEBO(0),
    m_cameraYaw(-90.0f), m_cameraPitch(0.0f)
{
    initCamera();
    this->setAttribute(Qt::WA_KeyCompression, false);
    this->setFocusPolicy(Qt::StrongFocus);

}

OpenGLWidget::~OpenGLWidget()
{
    //【★核心修复4★】安全释放OpenGL资源，必须先判断上下文有效
    if (this->isValid()) {
        makeCurrent();
        glDeleteVertexArrays(1, &m_VAO);
        glDeleteBuffers(1, &m_VBO);
        glDeleteBuffers(1, &m_EBO);
        glDeleteVertexArrays(1, &m_slotVAO);
        glDeleteBuffers(1, &m_slotVBO);
        glDeleteBuffers(1, &m_slotEBO);
        glDeleteVertexArrays(1, &m_carVAO);
        glDeleteBuffers(1, &m_carVBO);
        glDeleteBuffers(1, &m_carEBO);
        delete m_shaderProgram;
        doneCurrent();
    }
}

void OpenGLWidget::initCamera()
{
    // 调整相机位置：拉远+降低高度，完美适配车位布局
    m_cameraPos = QVector3D(0.0f, 6.0f, 10.0f);
    // 调整相机朝向：更平缓的俯视角度，停车场完整显示
    m_cameraFront = QVector3D(0.0f, -0.2f, -1.0f);
    m_cameraUp = QVector3D(0.0f, 1.0f, 0.0f);
    m_cameraYaw = -90.0f;
    m_cameraPitch = -11.0f;
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    // 基础配置，解决深度冲突+抗锯齿
    glEnable(GL_MULTISAMPLE); // 开启多重采样抗锯齿
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST); // 启用多边形平滑
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_MULTISAMPLE);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);

    initShader();
    initBuffers();
}

void OpenGLWidget::initShader()
{
    m_shaderProgram = new QOpenGLShaderProgram(this);
    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aColor;
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        out vec3 ourColor;
        uniform vec3 colorUniform;
        void main()
        {
            gl_Position = projection * view * model * vec4(aPos, 1.0);
            ourColor = colorUniform;
        }
    )";
    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec3 ourColor;
        out vec4 FragColor;
        void main() { FragColor = vec4(ourColor, 1.0); }
    )";
    m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    m_shaderProgram->link();
    m_shaderProgram->bind();
}

//【★核心修复5★】重中之重！彻底修复车辆缓冲区初始化错误，这是glDrawElements崩溃的头号原因！
// 问题：原代码中车辆VBO/EBO绑定后，没有正确写入数据+启用顶点属性，绘制时缓冲区是空的！
void OpenGLWidget::initBuffers()
{
    // ========== 1. 初始化地面缓冲区 ==========
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);
    glBindVertexArray(m_VAO);
    float groundVertices[] = { -6.0f,0.0f,-6.0f,0.3f,0.3f,0.35f, 6.0f,0.0f,-6.0f,0.3f,0.3f,0.35f, 6.0f,0.0f,6.0f,0.3f,0.3f,0.35f, -6.0f,0.0f,6.0f,0.3f,0.3f,0.35f };
    unsigned int groundIndices[] = { 0,1,2,2,3,0 };
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(groundVertices), groundVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(groundIndices), groundIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);

    // ========== 2. 初始化车位缓冲区 ==========
    glGenVertexArrays(1, &m_slotVAO);
    glGenBuffers(1, &m_slotVBO);
    glGenBuffers(1, &m_slotEBO);
    glBindVertexArray(m_slotVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_slotVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(PARKING_SLOT_VERTICES), PARKING_SLOT_VERTICES, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_slotEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(PARKING_SLOT_INDICES), PARKING_SLOT_INDICES, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);

    // ========== 3. 初始化车辆缓冲区【★完全修复★】 ==========
    glGenVertexArrays(1, &m_carVAO);
    glGenBuffers(1, &m_carVBO);
    glGenBuffers(1, &m_carEBO);
    glBindVertexArray(m_carVAO); // 绑定VAO
    glBindBuffer(GL_ARRAY_BUFFER, m_carVBO); // 绑定VBO
    glBufferData(GL_ARRAY_BUFFER, sizeof(CAR_VERTICES), CAR_VERTICES, GL_STATIC_DRAW); // 写入顶点数据
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_carEBO); // 绑定EBO
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(CAR_INDICES), CAR_INDICES, GL_STATIC_DRAW); // 写入索引数据
    // 启用顶点属性，和着色器一一对应，缺一不可！
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 解绑所有缓冲区，防止状态污染
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void OpenGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    m_shaderProgram->bind();
    QMatrix4x4 proj; proj.perspective(45.0f, (float)w / h, 0.1f, 100.0f);
    m_shaderProgram->setUniformValue("projection", proj);
}

void OpenGLWidget::paintGL()
{
    //计算帧率时差
    float currentFrame = static_cast<float>(QDateTime::currentMSecsSinceEpoch()) / 1000.0f;
    m_deltaTime = currentFrame - m_lastFrame;
    m_lastFrame = currentFrame;

    if (!this->isValid() || !m_shaderProgram) return; // 防崩溃
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_shaderProgram->bind();

    // 计算视角矩阵
    QMatrix4x4 view;
    view.lookAt(m_cameraPos, m_cameraPos + m_cameraFront, m_cameraUp);
    /*
    view.rotate(m_rotateX, 1, 0, 0);
    view.rotate(m_rotateY, 0, 1, 0);
    view.scale(m_scale);
    */
    m_shaderProgram->setUniformValue("view", view);

    drawParkingLot();
    m_shaderProgram->release();
}

void OpenGLWidget::drawParkingLot()
{
    if (!m_parkingLot || !m_parkingLot->car_ports || m_parkingLot->capacity == 0) return;
    int modelLoc = m_shaderProgram->uniformLocation("model");
    int colorLoc = m_shaderProgram->uniformLocation("colorUniform");
    if (modelLoc == -1 || colorLoc == -1) return;

    // 1. 绘制地面：先缩放，再平移（顺序正确）
    QMatrix4x4 model;
    model.scale(8.0f); // 地面基础缩放
    model.translate(0.0f, 0.0f, 0.0f);
    m_shaderProgram->setUniformValue(modelLoc, model);
    m_shaderProgram->setUniformValue(colorLoc, QVector3D(0.25f, 0.25f, 0.3f));
    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // 2. 绘制车位：矩阵顺序→缩放→平移（核心修复）
    for (int i = 0; i < m_parkingLot->capacity; i++) {
        CarPort& port = m_parkingLot->car_ports[i];
        model.setToIdentity();
        // 正确顺序：先缩放，再平移
        model.scale(port.width * 0.3f, 0.01f, port.length * 0.3f); // 缩小缩放系数，避免挤在一起
        model.translate(port.x * 0.5f, port.y, port.z * 0.5f); // 调整坐标间距，布局整齐
        m_shaderProgram->setUniformValue(modelLoc, model);
        m_shaderProgram->setUniformValue(colorLoc, port.state == Empty ? QVector3D(0.0f, 0.6f, 1.0f) : QVector3D(1.0f, 0.2f, 0.2f));
        glBindVertexArray(m_slotVAO);
        glDrawElements(GL_TRIANGLES, PARKING_SLOT_INDEX_COUNT, GL_UNSIGNED_INT, nullptr);
    }

    // 3. 绘制车辆：矩阵顺序→缩放→平移（核心修复）
    for (int i = 0; i < m_parkingLot->capacity; i++)
    {
        if (m_parkingLot->vehicles_in_lot[i] == nullptr) continue;
        VehicleInfo* car = m_parkingLot->vehicles_in_lot[i];
        if (car->parking_position < 0 || car->parking_position >= m_parkingLot->capacity) continue;
        CarPort& port = m_parkingLot->car_ports[car->parking_position];

        model.setToIdentity();
        // 正确顺序：先缩放，再平移
        switch (car->type)
        {
        case CAR_SMALL: model.scale(0.4f, 0.25f, 0.9f); break; // 缩小车辆比例
        case CAR_TRUCK_SMALL: model.scale(0.5f, 0.3f, 1.25f); break;
        case CAR_TRUCK_MEDIUM: model.scale(0.6f, 0.4f, 1.75f); break;
        case CAR_TRUCK_LARGE: model.scale(0.75f, 0.5f, 2.5f); break;
        }
        model.translate(port.x * 0.5f, port.y + 0.05f, port.z * 0.5f); // 调整车辆位置，和车位对齐
        m_shaderProgram->setUniformValue(modelLoc, model);
        m_shaderProgram->setUniformValue(colorLoc, car->carColor * (car->type == CAR_SMALL ? 1.0f : 0.7f));
        glBindVertexArray(m_carVAO);
        glDrawElements(GL_TRIANGLES, CAR_INDEX_COUNT, GL_UNSIGNED_INT, nullptr);
    }

    glBindVertexArray(0);
}

void OpenGLWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::RightButton && m_isDragging)
    {
        QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();

        const float sensitivity = 0.25f;
        m_rotateY += delta.x() * sensitivity;
        m_rotateX -= delta.y() * sensitivity;
        m_rotateX = qBound(-89.0f, m_rotateX, 89.0f);

        QVector3D front;
        front.setX(cos(qDegreesToRadians(m_rotateY)) * cos(qDegreesToRadians(m_rotateX)));
        front.setY(sin(qDegreesToRadians(m_rotateX)));
        front.setZ(sin(qDegreesToRadians(m_rotateY)) * cos(qDegreesToRadians(m_rotateX)));
        m_cameraFront = front.normalized();

        update();
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void OpenGLWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton)
    {
        m_isDragging = false;
        setCursor(Qt::ArrowCursor); // 可选：松开右键恢复箭头光标
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void OpenGLWidget::wheelEvent(QWheelEvent* event)
{
    // 缩放灵敏度：0.003f 手感最佳，数值越小越细腻，越大越快
    m_scale += event->angleDelta().y() * 0.003f;
    m_scale = qBound(0.8f, m_scale, 12.0f); // 限制缩放范围，防止画面过大/过小丢失
    update(); // 强制刷新画面，滚轮每帧都生效
}

void OpenGLWidget::keyPressEvent(QKeyEvent* event)
{
    const float moveSpeed = 0.15f;
    bool updateFlag = false;

    QVector3D cameraFront = m_cameraFront.normalized();
    QVector3D cameraRight = QVector3D::crossProduct(cameraFront, m_cameraUp).normalized();
    const QVector3D cameraUp = QVector3D(0.0f, 1.0f, 0.0f);

    // ========== 方向移动 - 完全保留，正常生效 ==========
    if (event->key() == Qt::Key_W) { m_cameraPos += cameraFront * moveSpeed; updateFlag = true; }
    if (event->key() == Qt::Key_S) { m_cameraPos -= cameraFront * moveSpeed; updateFlag = true; }
    if (event->key() == Qt::Key_A) { m_cameraPos -= cameraRight * moveSpeed; updateFlag = true; }
    if (event->key() == Qt::Key_D) { m_cameraPos += cameraRight * moveSpeed; updateFlag = true; }

    // ========== 核心修复：空格上移 + 左/右Ctrl都能连续下移 ==========
    if (event->key() == Qt::Key_Space) { m_cameraPos += cameraUp * moveSpeed; updateFlag = true; } // 空格：持续上移
    if (event->key() == Qt::Key_Control || event->key() == Qt::Key_Meta) { m_cameraPos -= cameraUp * moveSpeed; updateFlag = true; } // 左/右Ctrl：持续下移

    // ========== R键重置 - 保留 ==========
    if (event->key() == Qt::Key_R) { resetGraphicsState(); updateFlag = true; }

    // ========== 关键：强制允许持续按键，取消Qt的系统按键过滤 ==========
    if (updateFlag) {
        event->ignore(); // 告诉Qt不要过滤这个按键事件，允许持续触发
        update();
    }
    QOpenGLWidget::keyPressEvent(event);
}

void OpenGLWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton)
    {
        m_lastMousePos = event->pos();
        m_isDragging = true;
        setCursor(Qt::ClosedHandCursor); // 可选：按下右键显示抓手光标，体验更好
    }
    // 必须传递事件给父类，防止事件丢失
    QOpenGLWidget::mousePressEvent(event);
}

void OpenGLWidget::resetGraphicsState()
{
    m_rotateX = 0; m_rotateY = 0;
    m_scale = 3.0f; // 初始缩放值，完美适配停车场大小
    m_selectedCarPort = -1;
    initCamera();
    m_lastFrame = static_cast<float>(QDateTime::currentMSecsSinceEpoch()) / 1000.0f;
    update();
}

void OpenGLWidget::resetView() {}

MainWindow::~MainWindow() {
    cleanup_parking_lot(&parkingLot);
}