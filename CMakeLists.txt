cmake_minimum_required(VERSION 3.20)

project(ImageTransient
    VERSION 0.2.0
    DESCRIPTION "Create short transition videos from two still images"
    HOMEPAGE_URL "https://github.com/yousefvand/ImageTransient"
    LANGUAGES CXX
)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

add_executable(imagetransient
    src/main.cpp
)

target_link_libraries(imagetransient PRIVATE Qt6::Widgets)
target_compile_definitions(imagetransient PRIVATE
    IMAGETRANSIENT_VERSION="${PROJECT_VERSION}"
    IMAGETRANSIENT_HOMEPAGE="${PROJECT_HOMEPAGE_URL}"
)

if(WIN32)
    set_target_properties(imagetransient PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

install(TARGETS imagetransient RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(UNIX AND NOT APPLE)
    install(FILES packaging/io.github.remisa.imagetransient.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES icons/io.github.remisa.imagetransient.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    install(FILES LICENSE
            DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/imagetransient)
endif()
