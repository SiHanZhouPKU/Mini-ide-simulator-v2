cmake_minimum_required(VERSION 3.16)
project(MiniIdeSimulatorV2 VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/codeeditor.cpp
    src/lexer.cpp
    src/parser.cpp
    src/executor.cpp
    src/snapshot.cpp
    src/memoryview.cpp
)

set(HEADERS
    src/mainwindow.h
    src/codeeditor.h
    src/lexer.h
    src/parser.h
    src/ast.h
    src/executor.h
    src/snapshot.h
    src/variant_value.h
    src/memoryview.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)

target_include_directories(${PROJECT_NAME} PRIVATE src)
