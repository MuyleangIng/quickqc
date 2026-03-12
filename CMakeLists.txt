cmake_minimum_required(VERSION 3.21)

project(QuickQC VERSION 0.1.6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Gui OpenGLWidgets Network)

qt_standard_project_setup()

qt_add_executable(quickqc
    src/main.cpp
    src/clipitem.h
    src/clipstorage.h
    src/clipstorage.cpp
    src/clipboardwatcher.h
    src/clipboardwatcher.cpp
    src/mainwindow.h
    src/mainwindow.cpp
)

target_link_libraries(quickqc
    PRIVATE
        Qt6::Widgets
        Qt6::Sql
        Qt6::Gui
        Qt6::OpenGLWidgets
        Qt6::Network
)

target_compile_definitions(quickqc
    PRIVATE
        QUICKQC_VERSION=\"${PROJECT_VERSION}\"
)

set_target_properties(quickqc PROPERTIES
    OUTPUT_NAME "quickqc"
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

install(TARGETS quickqc
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

if(APPLE)
    # Ensure macOS bundles include Qt frameworks/plugins and use bundle-relative rpaths.
    qt_generate_deploy_app_script(
        TARGET quickqc
        OUTPUT_SCRIPT quickqc_deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${quickqc_deploy_script})
endif()
