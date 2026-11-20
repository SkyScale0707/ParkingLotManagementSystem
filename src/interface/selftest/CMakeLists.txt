# 独立自测工程：不依赖仓库根 CMake，也不影响现有构建
# 用法（构建目录建议放在仓库外或 build/ 下）：
#   cmake -S src/interface/selftest -B build/interface-selftest `
#         -G "Visual Studio 17 2022" -A x64 `
#         -DCMAKE_PREFIX_PATH=D:/Qt/6.10.3/msvc2022_64
#   cmake --build build/interface-selftest --config Release
#   build/interface-selftest/Release/t_interface.exe
cmake_minimum_required(VERSION 3.24)

project(pl_interface_selftest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(MSVC)
    add_compile_options(/utf-8 /permissive-)
endif()

find_package(Qt6 6.10 REQUIRED COMPONENTS Core Test)
qt_standard_project_setup()

add_executable(t_interface
    test_interface.cpp
    ../device_types.h
    ../clock.h
    ../devices.h
    ../simulated_devices.h
    ../lane_controller.h
    ../extra_equipment_state.h
    ../std_car_io.h
    ../entry_exit_simulator.h
    ../device_types.cpp
    ../clock.cpp
    ../simulated_devices.cpp
    ../lane_controller.cpp
    ../extra_equipment_state.cpp
    ../std_car_io.cpp
    ../entry_exit_simulator.cpp
)

target_include_directories(t_interface PRIVATE ..)
target_link_libraries(t_interface PRIVATE Qt6::Core Qt6::Test)
