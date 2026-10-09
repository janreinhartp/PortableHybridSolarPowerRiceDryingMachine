idf_component_register(
    SRCS
        "main.cpp"
        "board/board_hal.c"
        "app/dryer_controller/dryer_controller.cpp"
        "services/rtc_manager/rtc_manager.cpp"
        "ui/ui_manager/ui_manager.cpp"
    INCLUDE_DIRS
        "."
        "board"
        "app/dryer_controller"
        "services/rtc_manager"
        "ui/ui_manager"
    REQUIRES
        nvs_flash
        esp_timer
        rgb_lcd_port
        touch
        lvgl_port
        sd
        rtc
)

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
if(lvgl_lib)
    target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
endif()
