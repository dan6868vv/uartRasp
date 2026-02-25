cmake_minimum_required(VERSION 3.28)
project(DataShare)

set(CMAKE_CXX_STANDARD 17)

add_executable(DataShare main.cpp
        UARThead.h)

add_custom_target(project_files SOURCES
        uart_png.cpp          # твой файл для форматирования
        background.png        # ресурсы
        needle.png
        needle_config.txt
        # можно добавить и другие файлы проекта
)