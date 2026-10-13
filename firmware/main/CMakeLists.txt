idf_component_register(
    SRCS
        "main.cpp"
        "board/board_hal.c"
        "app/dryer_controller/dryer_controller.cpp"
        "services/rtc_manager/rtc_manager.cpp"
        "services/th_sensor/th_sensor.cpp"
        "services/sensor_manager/sensor_manager.cpp"
        "services/actuator_manager/actuator_manager.cpp"
        "ui/ui_manager/ui_manager.cpp"
    INCLUDE_DIRS
        "."
        "board"
        "app/dryer_controller"
        "services/rtc_manager"
        "services/th_sensor"
        "services/sensor_manager"
        "services/actuator_manager"
        "ui/ui_manager"
    REQUIRES
        nvs_flash
        esp_timer
        rgb_lcd_port
        touch
        lvgl_port
        sd
        rtc
        rs485
        modbus
        relay
        ads1115
)

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
if(lvgl_lib)
    target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
endif()
