cmake_minimum_required(VERSION 3.20)

project(ImageTransient VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

add_executable(imagetransient
    src/main.cpp
)

target_link_libraries(imagetransient PRIVATE Qt6::Widgets)

install(TARGETS imagetransient RUNTIME DESTINATION bin)
install(FILES packaging/io.github.remisa.imagetransient.desktop DESTINATION share/applications)
install(FILES icons/io.github.remisa.imagetransient.svg DESTINATION share/icons/hicolor/scalable/apps)
