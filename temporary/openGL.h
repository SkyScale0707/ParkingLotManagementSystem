#pragma once
#include "parkingLotNeeded.h"

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMouseEvent>
#include <QWheelEvent>

#include "QtVsMainwindow_ParkingLotManagementSystem_b1.h"

#ifndef OPENGL_H
#define OPENGL_H

class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit OpenGLWidget(ParkingLot* parkingLot, QWidget* parent = nullptr);
    ~OpenGLWidget() override;

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    // 鼠标事件
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    ParkingLot* m_parkingLot = nullptr;  // 停车场数据指针
    bool m_isDragging;         // 鼠标拖动状态
    QPoint m_lastMousePos;     // 上次鼠标位置
    float m_rotateX;           // X轴旋转角度
    float m_rotateY;           // Y轴旋转角度
    float m_scale;             // 缩放因子
    float m_translateX;        // X轴平移
    float m_translateY;        // Y轴平移
    float m_translateZ;        // Z轴平移

    // 绘制车位
    void drawParkingSlot(int index, bool isOccupied);
    // 绘制车辆
    void drawVehicle(VehicleType type);
    // 重置视角
    void resetView();
};

#endif // OPENGLWIDGET_H