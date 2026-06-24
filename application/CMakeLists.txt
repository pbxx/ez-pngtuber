cmake_minimum_required(VERSION 3.24)

project(ez_pngtuber
    VERSION 0.1.0
    DESCRIPTION "EZ PNGTuber desktop application"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(wxWidgets CONFIG QUIET COMPONENTS core base)

if(wxWidgets_FOUND)
    set(EZ_PNGTUBER_WX_LIBRARIES wx::core wx::base)
else()
    find_package(wxWidgets REQUIRED COMPONENTS core base)
    include(${wxWidgets_USE_FILE})
    set(EZ_PNGTUBER_WX_LIBRARIES ${wxWidgets_LIBRARIES})
endif()

add_executable(ez-pngtuber
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
)

target_link_libraries(ez-pngtuber
    PRIVATE
        ${EZ_PNGTUBER_WX_LIBRARIES}
)

set_target_properties(ez-pngtuber PROPERTIES
    OUTPUT_NAME "ez-pngtuber"
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

install(TARGETS ez-pngtuber
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
