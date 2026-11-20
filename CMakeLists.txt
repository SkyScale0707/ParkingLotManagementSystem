cmake_minimum_required(VERSION 3.24)

project(ParkingLotManagementSystem
    VERSION 1.0.0
    DESCRIPTION "停车场管理系统（纯 Qt6，无 VS 扩展依赖）"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /permissive- /W4 /wd4100)
endif()

find_package(Qt6 6.10 REQUIRED COMPONENTS Core Gui Widgets Test)

qt_standard_project_setup()

# ---------------------------------------------------------------------------
# pl_interface：外部设备模拟层（地感 / 相机 / 道闸 / 显示屏 / 打印机）
# ---------------------------------------------------------------------------
add_subdirectory(src/interface)

# ---------------------------------------------------------------------------
# pl_core：领域层 + 持久化层 + 设备桥（不依赖 Widgets，便于单元测试）
# ---------------------------------------------------------------------------
add_library(pl_core STATIC
    src/domain/types.cpp
    src/domain/billing.cpp
    src/domain/lot.cpp
    src/domain/layout.cpp
    src/persistence/lotstore.cpp
    src/app/appcontext.cpp
    src/app/devicebridge.h
    src/app/devicebridge.cpp
)

target_include_directories(pl_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(pl_core PUBLIC Qt6::Core pl_interface)

# ---------------------------------------------------------------------------
# pl_ui：界面层（导航 / 页面 / 2D 视图 / 对话框）
# ---------------------------------------------------------------------------
add_library(pl_ui STATIC
    src/ui/mainwindow.cpp
    src/ui/theme.cpp
    src/ui/vehiclelistmodel.cpp
    src/ui/lotview.cpp
    src/ui/pages/overviewpage.cpp
    src/ui/pages/operationspage.cpp
    src/ui/pages/devicepage.cpp
    src/ui/pages/searchpage.cpp
    src/ui/pages/statspage.cpp
    src/ui/pages/settingspage.cpp
    src/ui/dialogs/enterdialog.cpp
    src/ui/dialogs/exitdialog.cpp
)

target_link_libraries(pl_ui PUBLIC pl_core Qt6::Gui Qt6::Widgets)

# ---------------------------------------------------------------------------
# parking_lot：GUI 可执行文件
# ---------------------------------------------------------------------------
qt_add_executable(parking_lot
    src/main.cpp
    resources/resources.qrc
)

if(WIN32)
    target_sources(parking_lot PRIVATE resources/app.rc)
    set_target_properties(parking_lot PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_link_libraries(parking_lot PRIVATE pl_ui)

# ---------------------------------------------------------------------------
# 测试
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
