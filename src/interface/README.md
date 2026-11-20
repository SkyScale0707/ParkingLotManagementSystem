# src/interface — 停车场进出口外设模拟层

本目录模拟停车场进出口的**外部设备**：地感、车牌识别相机、道闸、LED 屏、票据打印机，
以及一个“通过计数器”。目标是在没有真实硬件的情况下，完整跑通：

```text
车辆到达 → 车牌识别（含置信度/故障） → 道闸抬起 → 车辆通过 → 道闸落下 → 出入记录
```

这一层只依赖 **QtCore**，不引用 `domain/`、`persistence/`、`ui/`，也不修改仓库其它文件；
下一轮再把 `carIn` / `carOut` 信号适配到 `AppContext`。

## 文件

| 文件 | 作用 |
|---|---|
| `device_types.h/.cpp` | 车道/状态/错误枚举、`PlateReading`、`PassageEvent`、车牌归一化与演示车牌生成 |
| `clock.h/.cpp` | 可注入时钟：`SystemClock` / `ManualClock`（测试与回放用） |
| `devices.h` | 设备抽象接口：`ILoopDetector`、`IPlateRecognizer`、`IBarrier`、`IDisplayBoard`、`ITicketPrinter` |
| `simulated_devices.h/.cpp` | 上述接口的模拟实现，带识别延时/抖动、置信度、故障注入、道闸卡死 |
| `lane_controller.h/.cpp` | 单条车道的设备状态机（入口/出口），只依赖抽象接口 |
| `extra_equipment_state.h/.cpp` | 通过计数器 `CounterDevice` 与 `CounterState` / `CarIOState`（对应原草稿） |
| `std_car_io.h/.cpp` | 对外监控接口：`CarIOError`、`CarIOLog`、`StdCarInMonitor`、`StdCarOutMonitor`（对应原草稿） |
| `entry_exit_simulator.h/.cpp` | 一进一出两条车道的组装器，自动驱动车辆通过 |
| `CMakeLists.txt` | 供下一轮 `add_subdirectory(src/interface)` 的库定义（当前根工程未引用） |
| `selftest/` | 独立自测工程，不依赖根 CMake |

## 设备接口

```cpp
// 地感：车辆到达/离开
ILoopDetector::vehicleArrived() / vehicleLeft()

// 相机：请求识别 → recognized(PlateReading) 或 failed(DeviceError)
IPlateRecognizer::requestRecognition(LaneKind)

// 道闸：open()/close() → opened()/closed()/jammed()
IBarrier

// 显示屏：showMessage(QString) → messageChanged(QString)
IDisplayBoard

// 打印机：print(ticket, plate, atMs) → printed()/failed()
ITicketPrinter
```

模拟器通过 `SimulationProfile` 配置时序：

```cpp
SimulationProfile p;
p.recognitionDelayMs = 350;     // 识别耗时
p.recognitionJitterMs = 150;    // 抖动
p.recognitionFailureRate = 0.02;
p.minimumConfidence = 0.75;
p.barrierOpenMs = 450;
p.barrierCloseMs = 400;
p.vehiclePassMs = 900;          // 抬杆后多久让地感复位
```

## 车道状态机

```text
Idle
 └─ 地感到达 → VehicleDetected → Recognizing
      ├─ 识别失败/超时 → Fault ──(车辆离开地感)──> Idle
      ├─ 置信度低     → Rejected ─(车辆离开地感)──> Idle
      └─ 识别成功
           ├─ 出口且需要确认 → AwaitingApproval
           │     ├─ approveExit() → OpeningBarrier
           │     └─ rejectExit()  → Rejected
           └─ 入口 / 自动放行 → OpeningBarrier
                └─ 道闸抬起 → VehiclePassing
                     └─ 地感离开 → ClosingBarrier → Idle
```

- 入口事件：`LaneController::entered(PassageEvent)`
- 出口事件：`LaneController::exited(PassageEvent)`
- 出口待确认：`LaneController::waitingForApproval(PlateReading)`

`PassageEvent` 同时带 `recognizedAtMs`（识别完成）与 `passedAtMs`（真正通过），
上层可以按需要选择计费/流水时间。

## 快速使用

```cpp
using namespace pl::devices;

EntryExitSimulator sim;                       // 默认：出口自动放行
connect(&sim, &EntryExitSimulator::entered, this, [](const PassageEvent& e) {
    // 下一轮：调用 AppContext::enterVehicle(e.plate, 车型)
    // 拿到票据号后 sim.entryMonitor()->printTicket(ticket);
});
connect(&sim, &EntryExitSimulator::exited, this, [](const PassageEvent& e) {
    // 下一轮：调用 AppContext::leaveVehicle(e.plate)
});

sim.simulateEntry(QStringLiteral("鄂A12345"), 0.98, 0);
sim.simulateExit(QStringLiteral("鄂A12345"));
```

出口需要“先算费、再放行”时：

```cpp
EntryExitSimulator::Options options;
options.requireExitApproval = true;
EntryExitSimulator sim(options);

connect(&sim, &EntryExitSimulator::waitingForExitApproval, this,
        [&](const PlateReading& reading) {
            // 算费……
            sim.approveExit();          // 或 sim.rejectExit("余额不足")
        });
```

### 低层接口（只接一条车道）

```cpp
LaneController::DeviceSet devices;
devices.loop = &loop; devices.camera = &camera;
devices.barrier = &barrier; devices.display = &display;
devices.printer = &printer;

LaneController lane(LaneKind::Entry, devices);
connect(&lane, &LaneController::entered, ...);
lane.start();
```

## 记录与计数器

- `CarIOLog`：一条出入记录（车牌、入口/出口、In/Out、时间、置信度、车型、票据号）。
- `CarIOError`：错误码 + 消息 + 时间；`timeLog()` 返回
  `[年, 月, 日, 时, 分, 秒]`，对应原草稿里的 `time_log`。
- `CounterDevice`：累计入场/出场与净在场数；故障或未连接时 `record()` 返回 `false`。
- `StdCarInMonitor` / `StdCarOutMonitor` 负责把车道事件转成上述记录并累计计数。

## 与原草稿的对应

| 原草稿 | 现在 |
|---|---|
| `extra_equipment_state::counterState` | `CounterState`（Fine/Error/Disconnected） |
| `extra_equipment_state::carIOState` | `CarIOState`（In/Out） |
| `extra_equipment_state::counterMake` | `CounterDevice` |
| `CarIO::error` | `CarIOError` |
| `CarIO::logs` | `CarIOLog` |
| `CarIO::std_car_in` | `StdCarInMonitor` |
| `CarIO::std_car_out` | `StdCarOutMonitor` |

原草稿存在语法错误（枚举用分号、`if()` 为空、`#include "extra_equippment_state.cpp"` 拼写错误等），
已整体替换为可编译的 Qt 实现，功能意图保持不变。

## 自测

`selftest/` 是一个独立 CMake 工程（不会被根工程自动构建）：

```powershell
cmake -S src/interface/selftest -B build/interface-selftest `
      -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_PREFIX_PATH=D:/Qt/6.10.3/msvc2022_64
cmake --build build/interface-selftest --config Release
$env:PATH = "D:\Qt\6.10.3\msvc2022_64\bin;$env:PATH"
.\build\interface-selftest\Release\t_interface.exe
```

测试覆盖：车牌归一化、完整入库流程、低置信度拒绝、道闸卡死、出口待确认/自动放行、
注入时钟的时间戳、计数器、车道忙拒绝。

## 下一轮适配清单（不在本轮范围）

1. 根 `CMakeLists.txt` 增加 `add_subdirectory(src/interface)`，或把 `.cpp` 加进 `pl_core`。
2. 写一个 `DeviceBridge`（可放在 `src/app/`）：把 `PassageEvent.plate` 映射到
   `AppContext::enterVehicle()` / `leaveVehicle()`；`vehicleCategory` 映射到 `VehicleType`。
3. 出口在 `waitingForExitApproval` 里调用 `feeCentsFor()` 展示金额，再 `approveExit()`。
4. 把 `LaneController::state()`、`displayChanged()` 接到总览页，显示车道实时状态。
5. 需要真机时，用串口/网口网关实现 `devices.h` 的接口，替换 `Simulated*` 即可。

## 约定

- 时间：`qint64` 毫秒；`ManualClock` 只影响记录时间，设备延时仍由 `QTimer` 驱动。
- 车牌：`normalizePlate()` 统一大写、去空格与分隔符，全角转半角。
- 置信度：`0.0 ~ 1.0`；低于 `LaneController::minimumConfidence()` 视为识别失败。
- 车型：`vehicleCategory` 0~3 对应小汽车/小卡/中卡/大卡，`-1` 表示相机未给出。
