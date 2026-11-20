# 本目录的库定义：下一轮在根 CMakeLists 中 add_subdirectory(src/interface) 即可
# 当前根工程未引用本文件，因此不影响现有构建。
qt_add_library(pl_interface STATIC
    device_types.h
    clock.h
    devices.h
    simulated_devices.h
    lane_controller.h
    extra_equipment_state.h
    std_car_io.h
    entry_exit_simulator.h
    device_types.cpp
    clock.cpp
    simulated_devices.cpp
    lane_controller.cpp
    extra_equipment_state.cpp
    std_car_io.cpp
    entry_exit_simulator.cpp
)

target_include_directories(pl_interface PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(pl_interface PUBLIC Qt6::Core)
