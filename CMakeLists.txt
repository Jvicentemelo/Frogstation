cmake_minimum_required(VERSION 3.21)
project(Frogstation LANGUAGES CXX)

if(WIN32)
    enable_language(RC)  # compilador de recursos (ícone do .exe)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Network Multimedia)

# --- SDL2 (gamepad) -----------------------------------------------------------
# 1) usa um SDL2 já instalado (vcpkg, -DSDL2_DIR=..., CMAKE_PREFIX_PATH);
# 2) senão, no MSVC baixa o pacote oficial "SDL2-devel-*-VC.zip" automaticamente.
find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND AND MSVC)
    include(FetchContent)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    set(FROG_SDL2_VERSION "2.30.9" CACHE STRING "Versão do SDL2 a baixar")
    FetchContent_Declare(sdl2_devel
        URL https://github.com/libsdl-org/SDL/releases/download/release-${FROG_SDL2_VERSION}/SDL2-devel-${FROG_SDL2_VERSION}-VC.zip)
    FetchContent_MakeAvailable(sdl2_devel)
    set(SDL2_DIR "${sdl2_devel_SOURCE_DIR}/cmake" CACHE PATH "" FORCE)
    find_package(SDL2 CONFIG REQUIRED)
endif()
if(NOT TARGET SDL2::SDL2)
    message(FATAL_ERROR "SDL2 não encontrado. Defina -DSDL2_DIR=<pasta com sdl2-config.cmake>.")
endif()

add_executable(Frogstation WIN32
    src/main.cpp
    src/Core.cpp
    src/Core.h
    src/GameWindow.cpp
    src/GameWindow.h
    src/Gamepad.cpp
    src/Gamepad.h
    src/InputConfig.cpp
    src/InputConfig.h
    src/InputDialog.cpp
    src/InputDialog.h
    src/Library.cpp
    src/Library.h
    src/NoIntroDat.cpp
    src/NoIntroDat.h
    src/CoverManager.cpp
    src/CoverManager.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/Crc32.h
    src/libretro.h
)
# Ícone do executável: Frogstation.rc referencia Frogstation.ico (na raiz do projeto).
if(WIN32)
    target_sources(Frogstation PRIVATE Frogstation.rc)
    # recompila o recurso quando o .ico mudar
    set_source_files_properties(Frogstation.rc PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Frogstation.ico")
endif()

target_include_directories(Frogstation PRIVATE src ${CMAKE_CURRENT_SOURCE_DIR})
# SDL_MAIN_HANDLED: o Qt já fornece o ponto de entrada; o SDL não deve redefinir main().
target_compile_definitions(Frogstation PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(Frogstation PRIVATE
    Qt6::Widgets Qt6::Sql Qt6::Network Qt6::Multimedia SDL2::SDL2 ${CMAKE_DL_LIBS})
if(MSVC)
    target_compile_options(Frogstation PRIVATE /W4 /utf-8)
endif()

# Copia a SDL2.dll para ao lado do executável.
if(WIN32)
    add_custom_command(TARGET Frogstation POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:Frogstation>)
endif()
