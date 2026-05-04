#pragma once // 防止重复包含
#include <QString>
#include <vector>
#include <ctime>

// 枚举类型定义
typedef enum { Empty, Full } CarPortState;
typedef enum { CAR_SMALL, CAR_TRUCK_SMALL, CAR_TRUCK_MEDIUM, CAR_TRUCK_LARGE } VehicleType;

// 结构体定义
typedef struct {
    char id[32];
    CarPortState state;
    int position; // 简化：用车位编号代替3D坐标
    float width, length;
} CarPort;

typedef struct {
    char plate_number[32];
    VehicleType type;
    time_t arrival_time;
    time_t departure_time;
    int parking_position; // 所在车位编号
    bool on_waiting_queue; // 是否在等待队列
} VehicleInfo;

typedef struct {
    CarPort* car_ports;          // 车位数组
    VehicleInfo** vehicles_in_lot; // 场内车辆数组
    VehicleInfo** waiting_queue;   // 等待队列数组
    float unit_price;             // 单位停车费
    int capacity;                 // 停车场容量
    int current_count;            // 当前场内车辆数
    int waiting_count;            // 等待车辆数
    int total_vehicles;           // 累计总车辆数
} ParkingLot;