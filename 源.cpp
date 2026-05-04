#define _CRT_SECURE_NO_WARNINGS

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdlib.h>

#define MAX_VEHICLES 100
#define TIME_MULTIPLIER 180

typedef enum { Empty, Full } CarPortState;
typedef enum { CAR_SMALL, CAR_TRUCK_SMALL, CAR_TRUCK_MEDIUM, CAR_TRUCK_LARGE } VehicleType;

typedef struct {
    char id[32];
    CarPortState state;
    glm::vec3 position;
    float width, length;
} CarPort;

typedef struct {
    char plate_number[32];
    VehicleType type;
    time_t arrival_time;
    time_t departure_time;
    glm::vec3 color;
    int parking_position;
    bool on_waiting_queue;
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

GLFWwindow* window;
ParkingLot parkingLot = { 0 };  // 显式初始化为0
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 800;

glm::vec3 cameraPos = glm::vec3(0.0f, 40.0f, 60.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, -0.4f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float cameraYaw = -90.0f;
float cameraPitch = -30.0f;
bool rightMousePressed = false;
double lastMouseX = 0.0, lastMouseY = 0.0;
int selectedCarPort = -1;

enum State { MAIN_MENU, GRAPHICS_VIEW, PARKING_INPUT, UNPARKING_INPUT, VEHICLE_MANAGEMENT, EXIT_SYSTEM };
State currentState = MAIN_MENU;

struct { bool w, a, s, d, space, ctrl, r; } keys = { false };

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 ourColor;
void main() {
    FragColor = vec4(ourColor, 1.0);
}
)";

void clearScreen() { system("cls"); }
void wait_for_enter() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
        // 消耗输入缓冲区
    }
}

void initialize_parking_lot(ParkingLot* p, int capacity) {
    // 确保结构体已清零
    memset(p, 0, sizeof(ParkingLot));

    p->capacity = capacity;
    p->current_count = 0;
    p->waiting_count = 0;
    p->total_vehicles = 0;
    p->unit_price = 2.5f;

    // 分配内存并初始化为0
    p->car_ports = (CarPort*)calloc(capacity, sizeof(CarPort));
    p->vehicles_in_lot = (VehicleInfo**)calloc(capacity, sizeof(VehicleInfo*));
    p->waiting_queue = (VehicleInfo**)calloc(MAX_VEHICLES, sizeof(VehicleInfo*));

    if (p->car_ports == NULL || p->vehicles_in_lot == NULL || p->waiting_queue == NULL) {
        printf("内存分配失败\n");
        exit(1);
    }

    float portWidth = 3.5f;
    float portLength = 6.0f;
    float colSpacing = 10.0f;
    float rowSpacing = 12.0f;

    int portsPerRow = capacity > 20 ? 10 : 8;
    if (capacity <= 10) portsPerRow = 5;
    else if (capacity <= 20) portsPerRow = 8;
    else portsPerRow = 10;

    for (int i = 0; i < capacity; i++) {
        sprintf_s(p->car_ports[i].id, sizeof(p->car_ports[i].id), "P%d", i + 1);
        p->car_ports[i].state = Empty;
        p->car_ports[i].width = portWidth;
        p->car_ports[i].length = portLength;

        int row = i / portsPerRow;
        int col = i % portsPerRow;
        p->car_ports[i].position = glm::vec3(
            (col - portsPerRow / 2.0f) * colSpacing + colSpacing / 2.0f,
            0.0f,
            -row * rowSpacing
        );
    }
}

void cleanup_parking_lot(ParkingLot* p) {
    if (p->car_ports != NULL) {
        free(p->car_ports);
    }

    if (p->vehicles_in_lot != NULL) {
        for (int i = 0; i < p->capacity; i++) {
            if (p->vehicles_in_lot[i] != NULL) {
                free(p->vehicles_in_lot[i]);
                p->vehicles_in_lot[i] = NULL;  // 置为NULL避免悬空指针
            }
        }
        free(p->vehicles_in_lot);
    }

    if (p->waiting_queue != NULL) {
        for (int i = 0; i < p->waiting_count; i++) {
            if (p->waiting_queue[i] != NULL) {
                free(p->waiting_queue[i]);
                p->waiting_queue[i] = NULL;  // 置为NULL避免悬空指针
            }
        }
        free(p->waiting_queue);
    }

    // 将整个结构体清零
    memset(p, 0, sizeof(ParkingLot));
}

int find_vehicle_by_plate(ParkingLot* p, const char* plate) {
    if (p->vehicles_in_lot != NULL) {
        for (int i = 0; i < p->capacity; i++) {
            if (p->vehicles_in_lot[i] != NULL && strcmp(p->vehicles_in_lot[i]->plate_number, plate) == 0) return i;
        }
    }

    if (p->waiting_queue != NULL) {
        for (int i = 0; i < p->waiting_count; i++) {
            if (p->waiting_queue[i] != NULL && strcmp(p->waiting_queue[i]->plate_number, plate) == 0) return -2;
        }
    }
    return -1;
}

int find_empty_parking_slot(ParkingLot* p) {
    if (p->car_ports != NULL) {
        for (int i = 0; i < p->capacity; i++) {
            if (p->car_ports[i].state == Empty) return i;
        }
    }
    return -1;
}

const char* get_vehicle_type_name(VehicleType type) {
    switch (type) {
    case CAR_SMALL: return "小汽车";
    case CAR_TRUCK_SMALL: return "小卡";
    case CAR_TRUCK_MEDIUM: return "中卡";
    case CAR_TRUCK_LARGE: return "大卡";
    default: return "未知";
    }
}

void add_vehicle(ParkingLot* p, const char* plate, VehicleType type) {
    if (find_vehicle_by_plate(p, plate) != -1) {
        printf("车牌号 %s 已存在\n", plate);
        return;
    }

    VehicleInfo* info = (VehicleInfo*)calloc(1, sizeof(VehicleInfo));
    if (info == NULL) {
        printf("内存分配失败\n");
        return;
    }

    strcpy_s(info->plate_number, sizeof(info->plate_number), plate);
    info->type = type;
    info->arrival_time = time(NULL);
    info->departure_time = 0;
    info->color = glm::vec3((float)(rand() % 100) / 100.0f, (float)(rand() % 100) / 100.0f, (float)(rand() % 100) / 100.0f);
    info->on_waiting_queue = false;
    info->parking_position = -1;

    int slot = find_empty_parking_slot(p);
    if (slot != -1) {
        info->parking_position = slot;
        p->vehicles_in_lot[slot] = info;
        p->car_ports[slot].state = Full;
        p->current_count++;
        printf("车辆 %s 已停入车位 %s\n", plate, p->car_ports[slot].id);
    }
    else {
        info->on_waiting_queue = true;
        if (p->waiting_queue != NULL && p->waiting_count < MAX_VEHICLES) {
            p->waiting_queue[p->waiting_count++] = info;
            printf("停车场已满，车辆 %s 进入便道等待\n", plate);
        }
        else {
            printf("等待队列已满，无法添加车辆 %s\n", plate);
            free(info);
            return;
        }
    }
    p->total_vehicles++;
}

bool remove_vehicle(ParkingLot* p, const char* plate) {
    int pos = find_vehicle_by_plate(p, plate);
    if (pos == -1) {
        printf("未找到车辆 %s\n", plate);
        return false;
    }

    if (pos == -2) {
        if (p->waiting_queue != NULL) {
            for (int i = 0; i < p->waiting_count; i++) {
                if (p->waiting_queue[i] != NULL && strcmp(p->waiting_queue[i]->plate_number, plate) == 0) {
                    free(p->waiting_queue[i]);
                    for (int j = i; j < p->waiting_count - 1; j++) {
                        p->waiting_queue[j] = p->waiting_queue[j + 1];
                    }
                    p->waiting_queue[--p->waiting_count] = NULL;
                    printf("车辆 %s 已从便道中移除\n", plate);
                    return true;
                }
            }
        }
        return false;
    }

    if (p->vehicles_in_lot == NULL || p->vehicles_in_lot[pos] == NULL) {
        printf("车辆信息错误\n");
        return false;
    }

    time_t current_time = time(NULL);
    int duration = (int)difftime(current_time, p->vehicles_in_lot[pos]->arrival_time) * TIME_MULTIPLIER;
    int hour = duration / 3600;
    int minute = (duration % 3600) / 60;
    float cost = 0.0f;

    if (hour > 0 || minute >= 30) cost = (minute > 0 ? hour + 1 : hour) * p->unit_price;

    if (cost == 0.0f) {
        printf("尊敬的%s车主,停车时长：%d小时%d分钟,不足30分钟不收费\n", p->vehicles_in_lot[pos]->plate_number, hour, minute);
    }
    else {
        printf("尊敬的%s车主,停车时长：%d小时%d分钟，费用：%.2f元\n", p->vehicles_in_lot[pos]->plate_number, hour, minute, cost);
    }
    printf("停车位置：%s\n", p->car_ports[pos].id);

    free(p->vehicles_in_lot[pos]);
    p->vehicles_in_lot[pos] = NULL;
    p->car_ports[pos].state = Empty;
    p->current_count--;

    if (p->waiting_queue != NULL && p->waiting_count > 0) {
        VehicleInfo* next = p->waiting_queue[0];
        if (next != NULL) {
            next->parking_position = pos;
            next->arrival_time = time(NULL);
            next->on_waiting_queue = false;
            p->vehicles_in_lot[pos] = next;
            p->car_ports[pos].state = Full;
            p->current_count++;

            for (int i = 0; i < p->waiting_count - 1; i++) {
                p->waiting_queue[i] = p->waiting_queue[i + 1];
            }
            p->waiting_queue[--p->waiting_count] = NULL;

            printf("便道中车辆 %s 已进入车位 %s\n", next->plate_number, p->car_ports[pos].id);
        }
    }

    return true;
}

void query_vehicle(ParkingLot* p, const char* plate, int type) {
    bool found = false;
    printf("\n查询结果：\n");
    printf("=================================\n");

    if (strlen(plate) > 0) {
        int pos = find_vehicle_by_plate(p, plate);
        if (pos >= 0) {
            VehicleInfo* v = p->vehicles_in_lot[pos];
            char timeBuf[26];
            ctime_s(timeBuf, sizeof(timeBuf), &v->arrival_time);
            printf("车牌号: %s\n", v->plate_number);
            printf("车型: %s\n", get_vehicle_type_name(v->type));
            printf("位置: %s\n", p->car_ports[pos].id);
            printf("到达时间: %s", timeBuf);
            found = true;
        }
        else if (pos == -2) {
            printf("车牌号: %s\n", plate);
            printf("状态: 便道中等待\n");
            found = true;
        }
    }
    else if (type != -1) {
        for (int i = 0; i < p->capacity; i++) {
            if (p->vehicles_in_lot[i] != NULL && p->vehicles_in_lot[i]->type == type) {
                VehicleInfo* v = p->vehicles_in_lot[i];
                char timeBuf[26];
                ctime_s(timeBuf, sizeof(timeBuf), &v->arrival_time);
                printf("车牌号: %s\n", v->plate_number);
                printf("车型: %s\n", get_vehicle_type_name(v->type));
                printf("位置: %s\n", p->car_ports[i].id);
                printf("到达时间: %s", timeBuf);
                printf("---------------------------------\n");
                found = true;
            }
        }
    }

    if (!found) printf("未找到匹配的车辆\n");
    printf("=================================\n");
}

void display_all_vehicles(ParkingLot* p) {
    clearScreen();
    printf("\n=================================\n");
    printf("         所有车辆信息            \n");
    printf("=================================\n\n");

    printf("停车场内车辆 (%d辆):\n", p->current_count);
    for (int i = 0; i < p->capacity; i++) {
        if (p->vehicles_in_lot != NULL && p->vehicles_in_lot[i] != NULL) {
            VehicleInfo* v = p->vehicles_in_lot[i];
            char timeBuf[26];
            ctime_s(timeBuf, sizeof(timeBuf), &v->arrival_time);
            printf("  车位 %s: %s (%s) - 到达: %s",
                p->car_ports[i].id, v->plate_number,
                get_vehicle_type_name(v->type), timeBuf);
        }
    }

    printf("\n便道等待车辆 (%d辆):\n", p->waiting_count);
    for (int i = 0; i < p->waiting_count; i++) {
        if (p->waiting_queue != NULL && p->waiting_queue[i] != NULL) {
            VehicleInfo* v = p->waiting_queue[i];
            char timeBuf[26];
            ctime_s(timeBuf, sizeof(timeBuf), &v->arrival_time);
            printf("  等待 %d: %s (%s) - 到达: %s",
                i + 1, v->plate_number,
                get_vehicle_type_name(v->type), timeBuf);
        }
    }

    printf("\n=================================\n");
}

void edit_vehicle(ParkingLot* p, const char* plate) {
    int pos = find_vehicle_by_plate(p, plate);
    if (pos == -1) {
        printf("未找到车辆 %s\n", plate);
        return;
    }

    VehicleInfo* v = NULL;
    if (pos >= 0) {
        if (p->vehicles_in_lot != NULL) {
            v = p->vehicles_in_lot[pos];
        }
    }
    else {
        if (p->waiting_queue != NULL && p->waiting_count > 0) {
            v = p->waiting_queue[0];
        }
    }

    if (v == NULL) {
        printf("车辆信息错误\n");
        return;
    }

    printf("当前信息：\n");
    printf("  车牌号: %s\n", v->plate_number);
    printf("  车型: %s\n", get_vehicle_type_name(v->type));

    printf("\n输入新车牌号 (直接回车保持原车牌): ");
    char new_plate[32];
    fgets(new_plate, sizeof(new_plate), stdin);
    new_plate[strcspn(new_plate, "\n")] = '\0';

    if (strlen(new_plate) > 0) {
        if (find_vehicle_by_plate(p, new_plate) != -1) {
            printf("车牌号 %s 已存在\n", new_plate);
            return;
        }
        strcpy_s(v->plate_number, sizeof(v->plate_number), new_plate);
    }

    printf("选择车型 (1-小汽车, 2-小卡, 3-中卡, 4-大卡): ");
    char type_choice[10];
    fgets(type_choice, sizeof(type_choice), stdin);
    type_choice[strcspn(type_choice, "\n")] = '\0';

    if (strlen(type_choice) == 1 && type_choice[0] >= '1' && type_choice[0] <= '4') {
        v->type = (VehicleType)(type_choice[0] - '1');
    }
    else if (strlen(type_choice) > 0) {
        printf("输入无效！请输入1-4之间的单个数字。\n");
        return;
    }

    printf("车辆信息已更新\n");
}

void delete_vehicle(ParkingLot* p, const char* plate) {
    if (p->total_vehicles == 0) {
        printf("记录为空！\n");
        return;
    }

    if (remove_vehicle(p, plate)) {
        printf("车辆 %s 已删除\n", plate);
    }
}

void statistics(ParkingLot* p) {
    clearScreen();
    printf("\n=================================\n");
    printf("         统计信息               \n");
    printf("=================================\n\n");

    printf("总车辆数: %d\n", p->total_vehicles);
    printf("停车场内: %d辆\n", p->current_count);
    printf("便道等待: %d辆\n", p->waiting_count);
    printf("空余车位: %d个\n", p->capacity - p->current_count);

    int type_counts[4] = { 0 };
    for (int i = 0; i < p->capacity; i++) {
        if (p->vehicles_in_lot != NULL && p->vehicles_in_lot[i] != NULL) {
            type_counts[(int)p->vehicles_in_lot[i]->type]++;
        }
    }
    for (int i = 0; i < p->waiting_count; i++) {
        if (p->waiting_queue != NULL && p->waiting_queue[i] != NULL) {
            type_counts[(int)p->waiting_queue[i]->type]++;
        }
    }

    printf("\n车型分布：\n");
    printf("  小汽车: %d辆\n", type_counts[CAR_SMALL]);
    printf("  小卡: %d辆\n", type_counts[CAR_TRUCK_SMALL]);
    printf("  中卡: %d辆\n", type_counts[CAR_TRUCK_MEDIUM]);
    printf("  大卡: %d辆\n", type_counts[CAR_TRUCK_LARGE]);

    printf("\n=================================\n");
}

void save_to_file(ParkingLot* p) {
    FILE* file = NULL;
    errno_t err = fopen_s(&file, "parking_data.txt", "w");
    if (err != 0 || !file) {
        printf("无法打开文件进行保存\n");
        return;
    }

    fprintf(file, "==================================================\n");
    fprintf(file, "               停车场管理系统数据文件             \n");
    fprintf(file, "==================================================\n\n");

    fprintf(file, "停车场基本信息：\n");
    fprintf(file, "==================================================\n");
    fprintf(file, "停车场容量: %d\n", p->capacity);
    fprintf(file, "停车费单价: %.2f元/小时\n", p->unit_price);
    fprintf(file, "总车辆数: %d\n", p->total_vehicles);
    fprintf(file, "当前停放车辆数: %d\n", p->current_count);
    fprintf(file, "便道等待车辆数: %d\n", p->waiting_count);
    fprintf(file, "空余车位: %d\n", p->capacity - p->current_count);
    fprintf(file, "\n");

    fprintf(file, "停车场内车辆信息（%d辆）：\n", p->current_count);
    fprintf(file, "==================================================\n");
    for (int i = 0; i < p->capacity; i++) {
        if (p->vehicles_in_lot != NULL && p->vehicles_in_lot[i] != NULL) {
            VehicleInfo* v = p->vehicles_in_lot[i];
            fprintf(file, "车辆信息：\n");
            fprintf(file, "  车牌号: %s\n", v->plate_number);
            fprintf(file, "  车型: %s\n", get_vehicle_type_name(v->type));
            fprintf(file, "  车位号: %s\n", p->car_ports[i].id);
            fprintf(file, "  到达时间: %lld\n", (long long)v->arrival_time);
            fprintf(file, "  离开时间: %lld\n", (long long)v->departure_time);
            fprintf(file, "  停车位置索引: %d\n", i);
            fprintf(file, "  是否在便道等待: %s\n", v->on_waiting_queue ? "是" : "否");
            fprintf(file, "----------------------------------------\n");
        }
    }
    fprintf(file, "\n");

    fprintf(file, "便道等待车辆信息（%d辆）：\n", p->waiting_count);
    fprintf(file, "==================================================\n");
    for (int i = 0; i < p->waiting_count; i++) {
        if (p->waiting_queue != NULL && p->waiting_queue[i] != NULL) {
            VehicleInfo* v = p->waiting_queue[i];
            fprintf(file, "车辆信息：\n");
            fprintf(file, "  车牌号: %s\n", v->plate_number);
            fprintf(file, "  车型: %s\n", get_vehicle_type_name(v->type));
            fprintf(file, "  到达时间: %lld\n", (long long)v->arrival_time);
            fprintf(file, "  离开时间: %lld\n", (long long)v->departure_time);
            fprintf(file, "  等待队列位置: %d\n", i);
            fprintf(file, "  是否在便道等待: %s\n", v->on_waiting_queue ? "是" : "否");
            fprintf(file, "----------------------------------------\n");
        }
    }
    fprintf(file, "\n");

    fprintf(file, "车位状态信息：\n");
    fprintf(file, "==================================================\n");
    for (int i = 0; i < p->capacity; i++) {
        fprintf(file, "车位 %s: %s\n",
            p->car_ports[i].id,
            p->car_ports[i].state == Empty ? "空" : "占用");
    }
    fprintf(file, "\n");

    fprintf(file, "==================================================\n");
    fprintf(file, "               数据保存完成                       \n");
    fprintf(file, "==================================================\n");

    fclose(file);
    printf("数据已保存到 parking_data.txt\n");
    printf("已保存详细信息：停车场容量=%d，停车中车辆=%d，等待中车辆=%d\n",
        p->capacity, p->current_count, p->waiting_count);
}

void load_from_file(ParkingLot* p) {
    FILE* file = NULL;
    errno_t err = fopen_s(&file, "parking_data.txt", "r");
    if (err != 0 || !file) {
        printf("无法打开文件读取数据\n");
        return;
    }

    char line[256];
    int capacity = 0;
    float unit_price = 0.0f;
    int total_vehicles = 0;
    int current_count = 0;
    int waiting_count = 0;

    bool reading_parked_vehicles = false;
    bool reading_waiting_vehicles = false;
    bool reading_parking_slots = false;

    VehicleInfo* temp_parked_vehicles[MAX_VEHICLES] = { 0 };
    VehicleInfo* temp_waiting_queue[MAX_VEHICLES] = { 0 };
    CarPortState temp_slot_states[MAX_VEHICLES] = { (CarPortState)0 };

    int parked_count = 0;
    int waiting_count_read = 0;

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';

        if (strstr(line, "停车场容量:") != NULL) {
            sscanf_s(line, "停车场容量: %d", &capacity);
        }
        else if (strstr(line, "停车费单价:") != NULL) {
            sscanf_s(line, "停车费单价: %f", &unit_price);
        }
        else if (strstr(line, "总车辆数:") != NULL) {
            sscanf_s(line, "总车辆数: %d", &total_vehicles);
        }
        else if (strstr(line, "当前停放车辆数:") != NULL) {
            sscanf_s(line, "当前停放车辆数: %d", &current_count);
        }
        else if (strstr(line, "便道等待车辆数:") != NULL) {
            sscanf_s(line, "便道等待车辆数: %d", &waiting_count);
        }
        else if (strstr(line, "停车场内车辆信息") != NULL) {
            reading_parked_vehicles = true;
            reading_waiting_vehicles = false;
            reading_parking_slots = false;
        }
        else if (strstr(line, "便道等待车辆信息") != NULL) {
            reading_parked_vehicles = false;
            reading_waiting_vehicles = true;
            reading_parking_slots = false;
        }
        else if (strstr(line, "车位状态信息") != NULL) {
            reading_parked_vehicles = false;
            reading_waiting_vehicles = false;
            reading_parking_slots = true;
        }
        else if (strstr(line, "车牌号:") != NULL && (reading_parked_vehicles || reading_waiting_vehicles)) {
            VehicleInfo* info = (VehicleInfo*)calloc(1, sizeof(VehicleInfo));
            if (info == NULL) {
                printf("内存分配失败\n");
                fclose(file);
                for (int i = 0; i < MAX_VEHICLES; i++) {
                    if (temp_parked_vehicles[i] != NULL) free(temp_parked_vehicles[i]);
                    if (temp_waiting_queue[i] != NULL) free(temp_waiting_queue[i]);
                }
                return;
            }

            char plate[32];
            char type_str[32];
            char slot_id[32];
            long long arrival, departure;
            int slot_index = -1;
            int queue_pos = -1;
            char waiting_str[10];

            sscanf_s(line, "  车牌号: %s", plate, (unsigned)_countof(plate));
            strcpy_s(info->plate_number, sizeof(info->plate_number), plate);

            fgets(line, sizeof(line), file);
            line[strcspn(line, "\n")] = '\0';
            sscanf_s(line, "  车型: %s", type_str, (unsigned)_countof(type_str));

            if (strcmp(type_str, "小汽车") == 0) info->type = CAR_SMALL;
            else if (strcmp(type_str, "小卡") == 0) info->type = CAR_TRUCK_SMALL;
            else if (strcmp(type_str, "中卡") == 0) info->type = CAR_TRUCK_MEDIUM;
            else if (strcmp(type_str, "大卡") == 0) info->type = CAR_TRUCK_LARGE;

            if (reading_parked_vehicles) {
                fgets(line, sizeof(line), file);
                line[strcspn(line, "\n")] = '\0';
                sscanf_s(line, "  车位号: %s", slot_id, (unsigned)_countof(slot_id));

                fgets(line, sizeof(line), file);
                line[strcspn(line, "\n")] = '\0';
                sscanf_s(line, "  到达时间: %lld", &arrival);
                info->arrival_time = (time_t)arrival;

                fgets(line, sizeof(line), file);
                line[strcspn(line, "\n")] = '\0';
                sscanf_s(line, "  离开时间: %lld", &departure);
                info->departure_time = (time_t)departure;

                fgets(line, sizeof(line), file);
                line[strcspn(line, "\n")] = '\0';
                sscanf_s(line, "  停车位置索引: %d", &slot_index);
                info->parking_position = slot_index;

                fgets(line, sizeof(line), file);
                line[strcspn(line, "\n")] = '\0';
                sscanf_s(line, "  是否在便道等待: %s", waiting_str, (unsigned)_countof(waiting_str));
                info->on_waiting_queue = (strcmp(waiting_str, "是") == 0);

                if (slot_index >= 0 && slot_index < MAX_VEHICLES) {
                    temp_parked_vehicles[slot_index] = info;
                    parked_count++;
                }
            }
            else if (reading_waiting_vehicles) {
                fgets(line, sizeof(line), file);
                line[strcspn(line, "\n")] = '\0';
                sscanf_s(line, "  到达时间: %lld", &arrival);
                info->arrival_time = (time_t)arrival;

                fgets(line, sizeof(line), file);
                line[strcspn(line, "\n")] = '\0';
                sscanf_s(line, "  离开时间: %lld", &departure);
                info->departure_time = (time_t)departure;

                fgets(line, sizeof(line), file);
                line[strcspn(line, "\n")] = '\0';
                sscanf_s(line, "  等待队列位置: %d", &queue_pos);

                fgets(line, sizeof(line), file);
                line[strcspn(line, "\n")] = '\0';
                sscanf_s(line, "  是否在便道等待: %s", waiting_str, (unsigned)_countof(waiting_str));
                info->on_waiting_queue = (strcmp(waiting_str, "是") == 0);

                if (queue_pos >= 0 && queue_pos < MAX_VEHICLES) {
                    temp_waiting_queue[queue_pos] = info;
                    waiting_count_read++;
                }
            }

            info->color = glm::vec3((float)(rand() % 100) / 100.0f, (float)(rand() % 100) / 100.0f, (float)(rand() % 100) / 100.0f);

            fgets(line, sizeof(line), file);
        }
        else if (reading_parking_slots && strstr(line, "车位 ") != NULL) {
            char slot_id[32];
            char state_str[10];
            int slot_num;

            if (sscanf_s(line, "车位 %s: %s", slot_id, (unsigned)_countof(slot_id), state_str, (unsigned)_countof(state_str)) == 2) {
                slot_num = atoi(slot_id + 1) - 1;
                if (slot_num >= 0 && slot_num < MAX_VEHICLES) {
                    temp_slot_states[slot_num] = (strcmp(state_str, "空") == 0) ? Empty : Full;
                }
            }
        }
    }

    fclose(file);

    if (capacity == 0) {
        printf("数据文件格式错误或为空\n");
        for (int i = 0; i < MAX_VEHICLES; i++) {
            if (temp_parked_vehicles[i] != NULL) free(temp_parked_vehicles[i]);
            if (temp_waiting_queue[i] != NULL) free(temp_waiting_queue[i]);
        }
        return;
    }

    cleanup_parking_lot(p);
    initialize_parking_lot(p, capacity);

    p->unit_price = unit_price;
    p->total_vehicles = total_vehicles;
    p->current_count = current_count;
    p->waiting_count = waiting_count;

    for (int i = 0; i < p->capacity; i++) {
        if (temp_parked_vehicles[i] != NULL) {
            p->vehicles_in_lot[i] = temp_parked_vehicles[i];
            p->car_ports[i].state = Full;
        }
        else {
            p->car_ports[i].state = temp_slot_states[i];
        }
    }

    for (int i = 0; i < waiting_count_read; i++) {
        if (temp_waiting_queue[i] != NULL) {
            p->waiting_queue[i] = temp_waiting_queue[i];
        }
    }

    printf("数据已从 parking_data.txt 加载\n");
    printf("成功加载：停车场容量=%d，停车中车辆=%d，等待中车辆=%d\n",
        p->capacity, p->current_count, p->waiting_count);
}

void showGraphicsControls() {
    clearScreen();
    printf("\n=================================\n");
    printf("       图形界面操作说明           \n");
    printf("=================================\n\n");
    printf("   W/A/S/D      - 前后左右移动\n");
    printf("   空格/Ctrl    - 上升/下降\n");
    printf("   鼠标右键拖动  - 旋转视角\n");
    printf("   R键         - 重置视角\n");
    printf("   鼠标左键     - 查看车位信息\n");
    printf("   ESC键       - 返回主菜单\n\n");
    printf("=================================\n");
}

void resetGraphicsState() {
    cameraPos = glm::vec3(0.0f, 40.0f, 60.0f);
    cameraFront = glm::vec3(0.0f, -0.4f, -1.0f);
    cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    cameraYaw = -90.0f;
    cameraPitch = -30.0f;
    selectedCarPort = -1;
    keys = { false };
}

void printTitle(const char* title) {
    printf("\n=================================\n");
    printf("           %s              \n", title);
    printf("=================================\n\n");
}

void vehicle_management_menu() {
    while (1) {
        clearScreen();
        printTitle("车辆管理");

        printf("1. 添加车辆\n");
        printf("2. 查询车辆\n");
        printf("3. 显示所有车辆\n");
        printf("4. 编辑车辆信息\n");
        printf("5. 删除车辆\n");
        printf("6. 统计信息\n");
        printf("7. 保存数据\n");
        printf("8. 加载数据\n");
        printf("9. 返回主菜单\n\n");
        printf("请选择操作 (1-9): ");

        char choice_str[10];
        fgets(choice_str, sizeof(choice_str), stdin);
        choice_str[strcspn(choice_str, "\n")] = '\0';

        if (strlen(choice_str) != 1 || choice_str[0] < '1' || choice_str[0] > '9') {
            printf("输入无效！请输入1-9之间的单个数字。\n");
            wait_for_enter();
            continue;
        }

        char choice = choice_str[0];

        if (choice == '1') {
            while (1) {
                clearScreen();
                printTitle("添加车辆");
                printf("按t键再按enter键返回主菜单\n");
                printf("直接按enter键继续添加车辆\n\n");

                char plate[32];
                printf("请输入车牌号: ");
                fgets(plate, sizeof(plate), stdin);
                plate[strcspn(plate, "\n")] = '\0';

                if (strlen(plate) == 0) continue;
                if (plate[0] == 't' || plate[0] == 'T') {
                    currentState = MAIN_MENU;
                    return;
                }

                printf("选择车型 (1-小汽车, 2-小卡, 3-中卡, 4-大卡): ");
                char type_choice[10];
                fgets(type_choice, sizeof(type_choice), stdin);
                type_choice[strcspn(type_choice, "\n")] = '\0';

                if (strlen(type_choice) == 1 && type_choice[0] >= '1' && type_choice[0] <= '4') {
                    add_vehicle(&parkingLot, plate, (VehicleType)(type_choice[0] - '1'));
                }
                else if (strlen(type_choice) > 0) {
                    printf("输入无效！请输入1-4之间的单个数字。\n");
                }
                printf("\n操作完成，按enter键继续添加，输入t返回主菜单...");
                char input[10];
                fgets(input, sizeof(input), stdin);
                if (strlen(input) > 0 && (input[0] == 't' || input[0] == 'T')) {
                    currentState = MAIN_MENU;
                    return;
                }
            }
        }
        else if (choice == '2') {
            while (1) {
                clearScreen();
                printTitle("查询车辆");
                printf("按t键再按enter键返回主菜单\n");
                printf("直接按enter键继续查询\n\n");

                printf("查询方式: 1-按车牌号, 2-按车型: ");
                char query_choice[10];
                fgets(query_choice, sizeof(query_choice), stdin);
                query_choice[strcspn(query_choice, "\n")] = '\0';

                if (strlen(query_choice) == 0) continue;
                if (strlen(query_choice) == 1 && (query_choice[0] == 't' || query_choice[0] == 'T')) {
                    currentState = MAIN_MENU;
                    return;
                }

                if (strlen(query_choice) == 1 && query_choice[0] == '1') {
                    char plate[32];
                    printf("请输入车牌号: ");
                    fgets(plate, sizeof(plate), stdin);
                    plate[strcspn(plate, "\n")] = '\0';

                    if (strlen(plate) == 0) continue;
                    if (strlen(plate) == 1 && (plate[0] == 't' || plate[0] == 'T')) {
                        currentState = MAIN_MENU;
                        return;
                    }
                    query_vehicle(&parkingLot, plate, -1);
                }
                else if (strlen(query_choice) == 1 && query_choice[0] == '2') {
                    printf("选择车型 (1-小汽车, 2-小卡, 3-中卡, 4-大卡): ");
                    char type_input[10];
                    fgets(type_input, sizeof(type_input), stdin);
                    type_input[strcspn(type_input, "\n")] = '\0';
                    if (strlen(type_input) == 1 && type_input[0] >= '1' && type_input[0] <= '4') {
                        int type_choice = type_input[0] - '1';
                        query_vehicle(&parkingLot, "", type_choice);
                    }
                    else if (strlen(type_input) > 0) {
                        printf("输入无效！请输入1-4之间的单个数字。\n");
                    }
                }
                else {
                    printf("输入无效！请输入1或2。\n");
                }
                printf("\n按enter键继续查询，输入t返回主菜单...");
                char input[10];
                fgets(input, sizeof(input), stdin);
                if (strlen(input) > 0 && (input[0] == 't' || input[0] == 'T')) {
                    currentState = MAIN_MENU;
                    return;
                }
            }
        }
        else if (choice == '3') {
            while (1) {
                display_all_vehicles(&parkingLot);
                printf("\n按enter键刷新，输入t返回主菜单...");
                char input[10];
                fgets(input, sizeof(input), stdin);
                if (strlen(input) > 0 && (input[0] == 't' || input[0] == 'T')) {
                    currentState = MAIN_MENU;
                    return;
                }
            }
        }
        else if (choice == '4') {
            while (1) {
                clearScreen();
                printTitle("编辑车辆信息");
                printf("按t键再按enter键返回主菜单\n");
                printf("直接按enter键继续编辑\n\n");

                char plate[32];
                printf("请输入要编辑的车牌号: ");
                fgets(plate, sizeof(plate), stdin);
                plate[strcspn(plate, "\n")] = '\0';

                if (strlen(plate) == 0) continue;
                if (strlen(plate) == 1 && (plate[0] == 't' || plate[0] == 'T')) {
                    currentState = MAIN_MENU;
                    return;
                }

                edit_vehicle(&parkingLot, plate);
                printf("\n操作完成，按enter键继续编辑，输入t返回主菜单...");
                char input[10];
                fgets(input, sizeof(input), stdin);
                if (strlen(input) > 0 && (input[0] == 't' || input[0] == 'T')) {
                    currentState = MAIN_MENU;
                    return;
                }
            }
        }
        else if (choice == '5') {
            while (1) {
                clearScreen();
                printTitle("删除车辆");
                printf("按t键再按enter键返回主菜单\n");
                printf("直接按enter键继续删除\n\n");

                char plate[32];
                printf("请输入要删除的车牌号: ");
                fgets(plate, sizeof(plate), stdin);
                plate[strcspn(plate, "\n")] = '\0';

                if (strlen(plate) == 0) continue;
                if (strlen(plate) == 1 && (plate[0] == 't' || plate[0] == 'T')) {
                    currentState = MAIN_MENU;
                    return;
                }

                delete_vehicle(&parkingLot, plate);
                printf("\n操作完成，按enter键继续删除，输入t返回主菜单...");
                char input[10];
                fgets(input, sizeof(input), stdin);
                if (strlen(input) > 0 && (input[0] == 't' || input[0] == 'T')) {
                    currentState = MAIN_MENU;
                    return;
                }
            }
        }
        else if (choice == '6') {
            while (1) {
                statistics(&parkingLot);
                printf("\n按enter键刷新，输入t返回主菜单...");
                char input[10];
                fgets(input, sizeof(input), stdin);
                if (strlen(input) > 0 && (input[0] == 't' || input[0] == 'T')) {
                    currentState = MAIN_MENU;
                    return;
                }
            }
        }
        else if (choice == '7') {
            while (1) {
                clearScreen();
                printTitle("保存数据");
                printf("按t键再按enter键返回主菜单\n");
                printf("直接按enter键保存数据\n\n");

                save_to_file(&parkingLot);
                printf("\n按enter键再次保存，输入t返回主菜单...");
                char input[10];
                fgets(input, sizeof(input), stdin);
                if (strlen(input) > 0 && (input[0] == 't' || input[0] == 'T')) {
                    currentState = MAIN_MENU;
                    return;
                }
            }
        }
        else if (choice == '8') {
            while (1) {
                clearScreen();
                printTitle("加载数据");
                printf("按t键再按enter键返回主菜单\n");
                printf("直接按enter键加载数据\n\n");

                load_from_file(&parkingLot);
                printf("\n按enter键再次加载，输入t返回主菜单...");
                char input[10];
                fgets(input, sizeof(input), stdin);
                if (strlen(input) > 0 && (input[0] == 't' || input[0] == 'T')) {
                    currentState = MAIN_MENU;
                    return;
                }
            }
        }
        else if (choice == '9') {
            currentState = MAIN_MENU;
            return;
        }
    }
}

void run_main_menu() {
    while (1) {
        clearScreen();
        printf("\n=================================\n");
        printf("       停车场管理系统主菜单       \n");
        printf("=================================\n\n");
        printf("停车场容量: %d辆\n", parkingLot.capacity);
        printf("当前停放: %d辆\n", parkingLot.current_count);
        printf("便道等待: %d辆\n", parkingLot.waiting_count);
        printf("\n计费规则：\n");
        printf("  1.停车不足30分钟不收费\n");
        printf("  2.超过30分钟按小时计费\n");
        printf("  3.每小时%.1f元\n", parkingLot.unit_price);
        printf("  4.不足1小时按1小时计算\n");
        printf("  5.时间加速：实际时间 x %d\n\n", TIME_MULTIPLIER);
        printf("=================================\n\n");

        printf("请选择操作：\n");
        printf("  1.进入图形界面\n");
        printf("  2.停车\n");
        printf("  3.取车\n");
        printf("  4.车辆管理\n");
        printf("  5.退出系统\n\n");
        printf("请输入选择 (1-5): ");

        char choice;
        int result = scanf_s(" %c", &choice, 1);
        if (result != 1) {
            wait_for_enter();
            continue;
        }
        wait_for_enter();

        if (choice < '1' || choice > '5') {
            printf("无效输入，请按enter键继续...");
            wait_for_enter();
            continue;
        }

        if (choice == '1') {
            showGraphicsControls();
            printf("\n按enter键进入图形界面\n");
            printf("按t键再按enter键返回主菜单\n\n");
            printf("请输入选择: ");
            char input[10];
            fgets(input, sizeof(input), stdin);
            if (strlen(input) > 0 && (input[0] == 't' || input[0] == 'T')) continue;
            resetGraphicsState();
            currentState = GRAPHICS_VIEW;
            return;
        }
        else if (choice == '2') {
            currentState = PARKING_INPUT;
            return;
        }
        else if (choice == '3') {
            currentState = UNPARKING_INPUT;
            return;
        }
        else if (choice == '4') {
            currentState = VEHICLE_MANAGEMENT;
            return;
        }
        else if (choice == '5') {
            currentState = EXIT_SYSTEM;
            return;
        }
    }
}

void run_parking_input() {
    char input[32];
    clearScreen();
    printTitle("停车操作");

    printf("按t键再按enter键返回主菜单\n\n");

    printf("可用车位: ");
    for (int i = 0; i < parkingLot.capacity; i++) {
        if (parkingLot.car_ports[i].state == Empty) printf("%s ", parkingLot.car_ports[i].id);
    }
    printf("\n\n");

    printf("请输入车牌号:\n");
    printf("> ");

    fgets(input, sizeof(input), stdin);
    input[strcspn(input, "\n")] = '\0';

    if (strlen(input) == 0 || (strlen(input) == 1 && (input[0] == 't' || input[0] == 'T'))) {
        currentState = MAIN_MENU;
        return;
    }

    printf("选择车型 (1-小汽车, 2-小卡, 3-中卡, 4-大卡): ");
    char type_choice[10];
    fgets(type_choice, sizeof(type_choice), stdin);
    type_choice[strcspn(type_choice, "\n")] = '\0';

    if (strlen(type_choice) == 1 && type_choice[0] >= '1' && type_choice[0] <= '4') {
        add_vehicle(&parkingLot, input, (VehicleType)(type_choice[0] - '1'));
    }
    else if (strlen(type_choice) > 0) {
        printf("输入无效！请输入1-4之间的单个数字。\n");
        printf("\n按enter键返回主菜单...");
        wait_for_enter();
        currentState = MAIN_MENU;
        return;
    }
    else {
        add_vehicle(&parkingLot, input, CAR_SMALL);
    }

    printf("\n操作完成，按enter键返回主菜单...");
    wait_for_enter();
    currentState = MAIN_MENU;
}

void run_unparking_input() {
    char input[32];
    clearScreen();
    printTitle("取车操作");

    printf("按t键再按enter键返回主菜单\n\n");

    printf("已停车车辆: ");
    for (int i = 0; i < parkingLot.capacity; i++) {
        if (parkingLot.car_ports[i].state == Full && parkingLot.vehicles_in_lot[i] != NULL) {
            printf("%s ", parkingLot.vehicles_in_lot[i]->plate_number);
        }
    }
    printf("\n\n");

    printf("请输入车牌号:\n");
    printf("> ");

    fgets(input, sizeof(input), stdin);
    input[strcspn(input, "\n")] = '\0';

    if (strlen(input) == 0 || (strlen(input) == 1 && (input[0] == 't' || input[0] == 'T'))) {
        currentState = MAIN_MENU;
        return;
    }

    if (!remove_vehicle(&parkingLot, input)) printf("\n取车失败\n");
    printf("\n操作完成，按enter键返回主菜单...");
    wait_for_enter();
    currentState = MAIN_MENU;
}

void updateCameraVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
    front.y = sin(glm::radians(cameraPitch));
    front.z = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
    cameraFront = glm::normalize(front);
}

unsigned int compileShader(const char* source, GLenum type) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    return shader;
}

unsigned int createShaderProgram() {
    unsigned int vertexShader = compileShader(vertexShaderSource, GL_VERTEX_SHADER);
    unsigned int fragmentShader = compileShader(fragmentShaderSource, GL_FRAGMENT_SHADER);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

unsigned int createCubeVAO() {
    float vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,  -0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  -0.5f, -0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f
    };

    unsigned int indices[] = {
        0,1,2,2,3,0, 4,5,6,6,7,4, 8,9,10,10,11,8, 12,13,14,14,15,12, 16,17,18,18,19,16, 20,21,22,22,23,20
    };

    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return VAO;
}

void drawCube(unsigned int shaderProgram, unsigned int cubeVAO, glm::vec3 position, glm::vec3 size, glm::vec3 color) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::scale(model, size);

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3f(glGetUniformLocation(shaderProgram, "ourColor"), color.r, color.g, color.b);

    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
}

void drawParkingLot(unsigned int shaderProgram, unsigned int cubeVAO) {
    int portsPerRow = parkingLot.capacity > 20 ? 10 : 8;
    if (parkingLot.capacity <= 10) portsPerRow = 5;
    else if (parkingLot.capacity <= 20) portsPerRow = 8;
    else portsPerRow = 10;

    int rows = (parkingLot.capacity + portsPerRow - 1) / portsPerRow;
    float groundWidth = portsPerRow * 10.0f + 10.0f;
    float groundLength = rows * 12.0f + 10.0f;

    drawCube(shaderProgram, cubeVAO,
        glm::vec3(0.0f, -0.55f, -groundLength / 2.0f + 6.0f),
        glm::vec3(groundWidth, 0.1f, groundLength),
        glm::vec3(0.4f, 0.4f, 0.4f));

    for (int i = 0; i < parkingLot.capacity; i++) {
        glm::vec3 color = (i == selectedCarPort) ? glm::vec3(1.0f, 1.0f, 0.0f) :
            (parkingLot.car_ports[i].state == Empty) ? glm::vec3(0.2f, 0.8f, 0.2f) : glm::vec3(0.8f, 0.2f, 0.2f);

        drawCube(shaderProgram, cubeVAO,
            parkingLot.car_ports[i].position + glm::vec3(0.0f, -0.5f, 0.0f),
            glm::vec3(parkingLot.car_ports[i].width, 0.05f, parkingLot.car_ports[i].length),
            color);
    }
}

void drawVehicle(unsigned int shaderProgram, unsigned int cubeVAO, glm::vec3 position, glm::vec3 color, VehicleType type) {
    float size_factor = 1.0f;
    switch (type) {
    case CAR_SMALL: size_factor = 0.7f; break;
    case CAR_TRUCK_SMALL: size_factor = 0.9f; break;
    case CAR_TRUCK_MEDIUM: size_factor = 1.1f; break;
    case CAR_TRUCK_LARGE: size_factor = 1.3f; break;
    }

    drawCube(shaderProgram, cubeVAO,
        position + glm::vec3(0.0f, 0.5f, 0.0f),
        glm::vec3(2.4f * size_factor, 0.8f * size_factor, 3.2f * size_factor),
        color);

    drawCube(shaderProgram, cubeVAO,
        position + glm::vec3(0.0f, 1.0f * size_factor, 0.2f * size_factor),
        glm::vec3(1.8f * size_factor, 0.3f * size_factor, 2.0f * size_factor),
        color);

    drawCube(shaderProgram, cubeVAO,
        position + glm::vec3(0.0f, 1.0f * size_factor, -1.0f * size_factor),
        glm::vec3(1.8f * size_factor, 0.3f * size_factor, 0.4f * size_factor),
        glm::vec3(0.8f, 0.9f, 1.0f));

    drawCube(shaderProgram, cubeVAO,
        position + glm::vec3(0.0f, 1.0f * size_factor, 1.2f * size_factor),
        glm::vec3(1.8f * size_factor, 0.3f * size_factor, 0.4f * size_factor),
        glm::vec3(0.8f, 0.9f, 1.0f));

    drawCube(shaderProgram, cubeVAO,
        position + glm::vec3(0.0f, 0.3f, -1.5f * size_factor),
        glm::vec3(2.2f * size_factor, 0.2f, 0.2f),
        glm::vec3(0.2f, 0.2f, 0.2f));

    drawCube(shaderProgram, cubeVAO,
        position + glm::vec3(0.0f, 0.3f, 1.5f * size_factor),
        glm::vec3(2.2f * size_factor, 0.2f, 0.2f),
        glm::vec3(0.2f, 0.2f, 0.2f));

    drawCube(shaderProgram, cubeVAO,
        position + glm::vec3(-0.9f * size_factor, 0.0f, -0.9f * size_factor),
        glm::vec3(0.5f, 0.3f, 0.5f),
        glm::vec3(0.1f, 0.1f, 0.1f));

    drawCube(shaderProgram, cubeVAO,
        position + glm::vec3(0.9f * size_factor, 0.0f, -0.9f * size_factor),
        glm::vec3(0.5f, 0.3f, 0.5f),
        glm::vec3(0.1f, 0.1f, 0.1f));

    drawCube(shaderProgram, cubeVAO,
        position + glm::vec3(-0.9f * size_factor, 0.0f, 0.9f * size_factor),
        glm::vec3(0.5f, 0.3f, 0.5f),
        glm::vec3(0.1f, 0.1f, 0.1f));

    drawCube(shaderProgram, cubeVAO,
        position + glm::vec3(0.9f * size_factor, 0.0f, 0.9f * size_factor),
        glm::vec3(0.5f, 0.3f, 0.5f),
        glm::vec3(0.1f, 0.1f, 0.1f));

    drawCube(shaderProgram, cubeVAO,
        position + glm::vec3(-0.6f * size_factor, 0.4f, -1.5f * size_factor),
        glm::vec3(0.2f, 0.1f, 0.1f),
        glm::vec3(1.0f, 1.0f, 0.8f));

    drawCube(shaderProgram, cubeVAO,
        position + glm::vec3(0.6f * size_factor, 0.4f, -1.5f * size_factor),
        glm::vec3(0.2f, 0.1f, 0.1f),
        glm::vec3(1.0f, 1.0f, 0.8f));

    drawCube(shaderProgram, cubeVAO,
        position + glm::vec3(-0.6f * size_factor, 0.4f, 1.5f * size_factor),
        glm::vec3(0.2f, 0.1f, 0.1f),
        glm::vec3(1.0f, 0.2f, 0.2f));

    drawCube(shaderProgram, cubeVAO,
        position + glm::vec3(0.6f * size_factor, 0.4f, 1.5f * size_factor),
        glm::vec3(0.2f, 0.1f, 0.1f),
        glm::vec3(1.0f, 0.2f, 0.2f));
}

void drawVehicles(unsigned int shaderProgram, unsigned int cubeVAO) {
    for (int i = 0; i < parkingLot.capacity; i++) {
        if (parkingLot.vehicles_in_lot[i] != NULL) {
            glm::vec3 carPosition = parkingLot.car_ports[i].position;
            carPosition.y = 0.0f;
            drawVehicle(shaderProgram, cubeVAO, carPosition, parkingLot.vehicles_in_lot[i]->color, parkingLot.vehicles_in_lot[i]->type);
        }
    }
}

int selectCarPort(double mouseX, double mouseY) {
    glm::mat4 projection = glm::perspective(glm::radians(60.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 200.0f);
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    glm::mat4 invVP = glm::inverse(projection * view);

    float ndcX = (2.0f * (float)mouseX) / (float)SCR_WIDTH - 1.0f;
    float ndcY = 1.0f - (2.0f * (float)mouseY) / (float)SCR_HEIGHT;

    glm::vec4 nearPoint = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    nearPoint /= nearPoint.w;

    glm::vec4 farPoint = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    farPoint /= farPoint.w;

    glm::vec3 rayOrigin = glm::vec3(nearPoint);
    glm::vec3 rayDirection = glm::normalize(glm::vec3(farPoint) - rayOrigin);

    if (fabs(rayDirection.y) > 0.0001f) {
        float t = (-0.5f - rayOrigin.y) / rayDirection.y;
        if (t > 0) {
            glm::vec3 intersection = rayOrigin + t * rayDirection;

            for (int i = 0; i < parkingLot.capacity; i++) {
                float halfWidth = parkingLot.car_ports[i].width / 2.0f + 0.5f;
                float halfLength = parkingLot.car_ports[i].length / 2.0f + 0.5f;
                glm::vec3 pos = parkingLot.car_ports[i].position;

                if (intersection.x >= pos.x - halfWidth && intersection.x <= pos.x + halfWidth &&
                    intersection.z >= pos.z - halfLength && intersection.z <= pos.z + halfLength) {
                    return i;
                }
            }
        }
    }
    return -1;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            rightMousePressed = true;
            glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else if (action == GLFW_RELEASE) {
            rightMousePressed = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
    else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        selectedCarPort = selectCarPort(x, y);

        if (selectedCarPort != -1) {
            printf("选中车位: %s", parkingLot.car_ports[selectedCarPort].id);
            if (parkingLot.car_ports[selectedCarPort].state == Empty) {
                printf(" (空位)\n");
            }
            else if (parkingLot.vehicles_in_lot[selectedCarPort] != NULL) {
                VehicleInfo* v = parkingLot.vehicles_in_lot[selectedCarPort];
                printf(" (占用 - 车辆: %s, 车型: %s)\n", v->plate_number, get_vehicle_type_name(v->type));
                time_t current_time = time(NULL);
                int duration = (int)difftime(current_time, v->arrival_time) * TIME_MULTIPLIER;
                int hours = duration / 3600;
                int minutes = (duration % 3600) / 60;
                printf("停车时长: %d小时%d分钟\n", hours, minutes);
            }
        }
    }
}

void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    if (rightMousePressed) {
        float xoffset = (float)(xpos - lastMouseX);
        float yoffset = (float)(lastMouseY - ypos);
        lastMouseX = xpos;
        lastMouseY = ypos;

        cameraYaw += xoffset * 0.1f;
        cameraPitch += yoffset * 0.1f;

        if (cameraPitch > 89.0f) cameraPitch = 89.0f;
        if (cameraPitch < -89.0f) cameraPitch = -89.0f;

        updateCameraVectors();
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_W: keys.w = true; break;
        case GLFW_KEY_A: keys.a = true; break;
        case GLFW_KEY_S: keys.s = true; break;
        case GLFW_KEY_D: keys.d = true; break;
        case GLFW_KEY_SPACE: keys.space = true; break;
        case GLFW_KEY_LEFT_CONTROL: keys.ctrl = true; break;
        case GLFW_KEY_R: keys.r = true; break;
        case GLFW_KEY_ESCAPE: currentState = MAIN_MENU; break;
        }
    }
    else if (action == GLFW_RELEASE) {
        switch (key) {
        case GLFW_KEY_W: keys.w = false; break;
        case GLFW_KEY_A: keys.a = false; break;
        case GLFW_KEY_S: keys.s = false; break;
        case GLFW_KEY_D: keys.d = false; break;
        case GLFW_KEY_SPACE: keys.space = false; break;
        case GLFW_KEY_LEFT_CONTROL: keys.ctrl = false; break;
        case GLFW_KEY_R: keys.r = false; break;
        }
    }
}

void processInput(float deltaTime) {
    float cameraSpeed = 8.0f * deltaTime;

    if (keys.w) cameraPos += cameraSpeed * cameraFront;
    if (keys.s) cameraPos -= cameraSpeed * cameraFront;
    if (keys.a) cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (keys.d) cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (keys.space) cameraPos += cameraSpeed * cameraUp;
    if (keys.ctrl) cameraPos -= cameraSpeed * cameraUp;

    if (keys.r) {
        cameraPos = glm::vec3(0.0f, 40.0f, 60.0f);
        cameraFront = glm::vec3(0.0f, -0.4f, -1.0f);
        cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
        cameraYaw = -90.0f;
        cameraPitch = -30.0f;
        updateCameraVectors();
        keys.r = false;
    }
}

void run_graphics_loop() {
    unsigned int shaderProgram = createShaderProgram();
    unsigned int cubeVAO = createCubeVAO();
    float lastFrame = 0.0f;

    glfwSetWindowShouldClose(window, GLFW_FALSE);
    glfwShowWindow(window);
    glfwFocusWindow(window);

    while (!glfwWindowShouldClose(window) && currentState == GRAPHICS_VIEW) {
        float currentFrame = (float)glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(deltaTime);

        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(60.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 200.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));

        drawParkingLot(shaderProgram, cubeVAO);
        drawVehicles(shaderProgram, cubeVAO);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteProgram(shaderProgram);
    glDeleteVertexArrays(1, &cubeVAO);

    if (currentState == GRAPHICS_VIEW) currentState = MAIN_MENU;
}

int main() {
    int capacity;
    printf("=== 3D停车场管理系统 ===\n");
    printf("请输入停车场容量 (n): ");
    int result = scanf_s("%d", &capacity);
    if (result != 1) {
        printf("输入无效\n");
        return 1;
    }
    wait_for_enter();

    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Parking Lot 3D View", NULL, NULL);
    if (window == NULL) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetKeyCallback(window, key_callback);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return -1;

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

    srand((unsigned int)time(NULL));
    initialize_parking_lot(&parkingLot, capacity);

    printf("系统启动成功！\n\n");

    glfwHideWindow(window);

    while (currentState != EXIT_SYSTEM) {
        switch (currentState) {
        case MAIN_MENU: glfwHideWindow(window); run_main_menu(); break;
        case GRAPHICS_VIEW: run_graphics_loop(); break;
        case PARKING_INPUT: glfwHideWindow(window); run_parking_input(); break;
        case UNPARKING_INPUT: glfwHideWindow(window); run_unparking_input(); break;
        case VEHICLE_MANAGEMENT: glfwHideWindow(window); vehicle_management_menu(); break;
        }
    }

    cleanup_parking_lot(&parkingLot);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}