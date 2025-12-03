cmake_minimum_required(VERSION 3.20)
project(MagazynSkaner)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(WIN32)
    set(CMAKE_PREFIX_PATH "C:/Qt/6.10.1/mingw_64")
    set(OpenCV_DIR "C:/tools/opencvmingw/x64/mingw/lib")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets SerialPort Network Concurrent)
find_package(OpenCV REQUIRED)

add_executable(MagazynSkaner
        main.cpp
        MainWindow.h
        MainWindow.cpp
        SettingDialog.h
        SettingDialog.cpp
        resources.qrc
)

target_link_libraries(MagazynSkaner PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::SerialPort
        Qt6::Network
        Qt6::Concurrent
        ${OpenCV_LIBS}
        pq
)

if(UNIX AND NOT APPLE)
    target_link_libraries(MagazynSkaner PRIVATE pq)
endif()

if(WIN32)
    add_custom_command(TARGET MagazynSkaner POST_BUILD
            COMMAND "${CMAKE_PREFIX_PATH}/bin/windeployqt.exe"
            "$<TARGET_FILE:MagazynSkaner>"
            --dir "$<TARGET_FILE_DIR:MagazynSkaner>"
            --no-translations
            --compiler-runtime
            --no-opengl-sw
            --plugindir "$<TARGET_FILE_DIR:MagazynSkaner>/plugins"
    )
endif()
