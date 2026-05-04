#include "parkingLotNeeded.h"
#include "openGL.h"
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QMatrix4x4>


OpenGLWidget::OpenGLWidget(ParkingLot* parkingLot, QWidget* parent)
    : QOpenGLWidget(parent),
    m_parkingLot(parkingLot),
    m_isDragging(false),
    m_rotateX(30.0f),
    m_rotateY(45.0f),
    m_scale(10.0f),
    m_translateX(0.0f),
    m_translateY(0.0f),
    m_translateZ(-50.0f)
{
    setFocusPolicy(Qt::StrongFocus); // 确保能接收键盘事件
}

OpenGLWidget::~OpenGLWidget()
{
    makeCurrent();
    doneCurrent();
}

void OpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f); // 背景色
    glEnable(GL_DEPTH_TEST); // 启用深度测试
    glEnable(GL_LIGHTING);   // 启用光照
    glEnable(GL_LIGHT0);     // 启用光源0
}

void OpenGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // 避免h=0导致除零错误
    float aspect = static_cast<float>(w) / qMax(static_cast<float>(h), 1.0f);
    // 用Qt QMatrix4x4实现透视投影（等价于gluPerspective）
    QMatrix4x4 projection;
    // 参数：垂直视角(°)、宽高比、近裁剪面、远裁剪面
    projection.perspective(45.0f, aspect, 0.1f, 500.0f);
    // 将Qt矩阵数据传入OpenGL的投影矩阵
    glLoadMatrixf(projection.constData());
    glMatrixMode(GL_MODELVIEW);
}

void OpenGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // 设置相机位置
    glTranslatef(m_translateX, m_translateY, m_translateZ);
    glScalef(m_scale, m_scale, m_scale);
    glRotatef(m_rotateX, 1.0f, 0.0f, 0.0f);
    glRotatef(m_rotateY, 0.0f, 1.0f, 0.0f);

    // 绘制停车场地面
    glColor3f(0.1f, 0.1f, 0.1f);
    glBegin(GL_QUADS);
    glVertex3f(-20.0f, -20.0f, 0.0f);
    glVertex3f(20.0f, -20.0f, 0.0f);
    glVertex3f(20.0f, 20.0f, 0.0f);
    glVertex3f(-20.0f, 20.0f, 0.0f);
    glEnd();

    // 绘制车位
    const int rows = 5; // 5行
    const int cols = 10; // 10列
    const float slotSpacing = 6.0f; // 车位间距

    for (int i = 0; i < m_parkingLot->capacity; ++i) {
        int row = i / cols;
        int col = i % cols;

        // 计算车位位置
        float x = (col - cols / 2) * slotSpacing;
        float y = (row - rows / 2) * slotSpacing;

        glPushMatrix();
        glTranslatef(x, y, 0.0f);

        // 检查车位是否被占用
        bool isOccupied = (m_parkingLot->vehicles_in_lot[i] != nullptr);
        drawParkingSlot(i, isOccupied);

        // 如果有车，绘制车辆
        if (isOccupied) {
            drawVehicle(m_parkingLot->vehicles_in_lot[i]->type);
        }
        glPopMatrix();
    }
}

void OpenGLWidget::drawParkingSlot(int index, bool isOccupied)
{
    // 车位大小
    const float width = 3.0f;
    const float length = 5.0f;

    // 根据是否占用设置颜色
    if (isOccupied) {
        glColor3f(0.8f, 0.2f, 0.2f); // 红色表示占用
    }
    else {
        glColor3f(0.2f, 0.8f, 0.2f); // 绿色表示空闲
    }

    // 绘制车位底面
    glBegin(GL_QUADS);
    glVertex3f(-width / 2, -length / 2, 0.01f);
    glVertex3f(width / 2, -length / 2, 0.01f);
    glVertex3f(width / 2, length / 2, 0.01f);
    glVertex3f(-width / 2, length / 2, 0.01f);
    glEnd();

    // 绘制车位线（白色）
    glColor3f(1.0f, 1.0f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(-width / 2, -length / 2, 0.02f);
    glVertex3f(width / 2, -length / 2, 0.02f);
    glVertex3f(width / 2, length / 2, 0.02f);
    glVertex3f(-width / 2, length / 2, 0.02f);
    glEnd();
    glLineWidth(1.0f);
}

void OpenGLWidget::drawVehicle(VehicleType type)
{
    // 根据车型设置不同大小和颜色
    float scale = 1.0f;
    switch (type) {
    case CAR_SMALL:
        glColor3f(0.2f, 0.5f, 0.8f); // 蓝色小汽车
        scale = 0.8f;
        break;
    case CAR_TRUCK_SMALL:
        glColor3f(0.8f, 0.5f, 0.2f); // 橙色小卡
        scale = 1.0f;
        break;
    case CAR_TRUCK_MEDIUM:
        glColor3f(0.8f, 0.2f, 0.8f); // 紫色中卡
        scale = 1.2f;
        break;
    case CAR_TRUCK_LARGE:
        glColor3f(0.2f, 0.8f, 0.8f); // 青色大卡
        scale = 1.5f;
        break;
    default:
        glColor3f(0.5f, 0.5f, 0.5f); // 灰色未知车型
        scale = 1.0f;
    }

    glPushMatrix();
    glScalef(scale, scale, scale);

    // 绘制车身
    // 由于在这里 glutSolidCube(1.0f) 函数无法编译，故采取手动绘制
    const float half = 0.5f;
    glBegin(GL_QUADS);
    // 前面
    glVertex3f(-half, -half, half);
    glVertex3f(half, -half, half);
    glVertex3f(half, half, half);
    glVertex3f(-half, half, half);
    // 后面
    glVertex3f(-half, -half, -half);
    glVertex3f(-half, half, -half);
    glVertex3f(half, half, -half);
    glVertex3f(half, -half, -half);
    // 左面
    glVertex3f(-half, -half, -half);
    glVertex3f(-half, -half, half);
    glVertex3f(-half, half, half);
    glVertex3f(-half, half, -half);
    // 右面
    glVertex3f(half, -half, -half);
    glVertex3f(half, half, -half);
    glVertex3f(half, half, half);
    glVertex3f(half, -half, half);
    // 顶面
    glVertex3f(-half, half, -half);
    glVertex3f(half, half, -half);
    glVertex3f(half, half, half);
    glVertex3f(-half, half, half);
    // 底面
    glVertex3f(-half, -half, -half);
    glVertex3f(-half, -half, half);
    glVertex3f(half, -half, half);
    glVertex3f(half, -half, -half);
    glEnd();

    // 绘制车顶
    // 同理，手动绘制
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.5f);
    glScalef(0.8f, 0.6f, 0.4f);
    glBegin(GL_QUADS);
    // 车顶面（简化）
    glVertex3f(-half, -half, half);
    glVertex3f(half, -half, half);
    glVertex3f(half, half, half);
    glVertex3f(-half, half, half);
    glVertex3f(-half, -half, -half);
    glVertex3f(-half, half, -half);
    glVertex3f(half, half, -half);
    glVertex3f(half, -half, -half);
    glEnd();
    glPopMatrix();

    glPopMatrix();
}

void OpenGLWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        m_isDragging = true;
        m_lastMousePos = event->pos();
    }
    QOpenGLWidget::mousePressEvent(event);
}

void OpenGLWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        m_isDragging = false;
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void OpenGLWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isDragging) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_rotateX += delta.y() * 0.5f;
        m_rotateY += delta.x() * 0.5f;
        m_lastMousePos = event->pos();
        update();  // 重绘
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void OpenGLWidget::wheelEvent(QWheelEvent* event)
{
    // 处理滚轮缩放
    float scaleFactor = 1.1f;
    if (event->angleDelta().y() < 0) {
        m_scale /= scaleFactor;
    }
    else {
        m_scale *= scaleFactor;
    }
    update();
    QOpenGLWidget::wheelEvent(event);
}

void OpenGLWidget::keyPressEvent(QKeyEvent* event)
{
    float moveSpeed = 0.5f;
    switch (event->key()) {
    case Qt::Key_W:
        m_translateY += moveSpeed;
        break;
    case Qt::Key_S:
        m_translateY -= moveSpeed;
        break;
    case Qt::Key_A:
        m_translateX -= moveSpeed;
        break;
    case Qt::Key_D:
        m_translateX += moveSpeed;
        break;
    case Qt::Key_Space:
        m_translateZ += moveSpeed;
        break;
    case Qt::Key_Control:
        m_translateZ -= moveSpeed;
        break;
    case Qt::Key_R:
        resetView();
        break;
    case Qt::Key_Escape:
        parentWidget()->setFocus(); // 返回主窗口焦点
        break;
    }
    update();
    QOpenGLWidget::keyPressEvent(event);
}

void OpenGLWidget::resetView()
{
    m_rotateX = 30.0f;
    m_rotateY = 45.0f;
    m_scale = 10.0f;
    m_translateX = 0.0f;
    m_translateY = 0.0f;
    m_translateZ = -50.0f;
}