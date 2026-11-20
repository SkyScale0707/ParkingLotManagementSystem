# t_domain：领域层纯逻辑测试（不需要 GUI）
qt_add_executable(t_domain test_domain.cpp)
set_target_properties(t_domain PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(t_domain PRIVATE pl_core Qt6::Test)
add_test(NAME domain COMMAND t_domain)

# t_persistence：多文件 JSON / 崩溃恢复测试
qt_add_executable(t_persistence test_persistence.cpp)
set_target_properties(t_persistence PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(t_persistence PRIVATE pl_core Qt6::Test)
add_test(NAME persistence COMMAND t_persistence)

# t_app：应用上下文（业务操作 + 落盘 + 重启恢复）
qt_add_executable(t_app test_app.cpp)
set_target_properties(t_app PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(t_app PRIVATE pl_core Qt6::Test)
add_test(NAME app COMMAND t_app)

# t_ui：界面冒烟测试（offscreen，无需显示器）
qt_add_executable(t_ui test_ui.cpp ${CMAKE_SOURCE_DIR}/resources/resources.qrc)
set_target_properties(t_ui PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(t_ui PRIVATE pl_ui Qt6::Test)
add_test(NAME ui COMMAND t_ui)
set_tests_properties(ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# t_bridge：自动识别设备 ↔ 业务层集成（人工 + 自动共用 AppContext）
qt_add_executable(t_bridge test_bridge.cpp)
set_target_properties(t_bridge PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(t_bridge PRIVATE pl_core Qt6::Test)
add_test(NAME bridge COMMAND t_bridge)
