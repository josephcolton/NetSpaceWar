cmake_minimum_required(VERSION 3.16)
project(NetSpaceWar C)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug)
endif()

option(NSW_BUILD_CLIENT "Build the SDL3 game client" ON)
option(NSW_FETCH_SDL "Download and build SDL3 if it is not installed" ON)

# OS_Linux / OS_Darwin / OS_Windows, used by #ifdef blocks in the clients
add_compile_definitions(OS_${CMAKE_SYSTEM_NAME})

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall)
endif()

if(MSVC)
  # M_PI, and quiet warnings about the classic C and socket calls this code uses
  add_compile_definitions(_USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_library(MATH_LIB m)

# Server: no external dependencies
add_executable(SpaceWarServer src/SpaceWarServer.c src/server.c src/sdata.c src/physics.c src/platform.c)

# AI client: headless, no external dependencies
add_executable(SpaceWarAI src/SpaceWarAI.c src/ai.c src/client.c src/cdata.c src/platform.c)

# Human client: SDL3. Uses an installed SDL3 if there is one, otherwise
# downloads it and links it statically so players need nothing installed.
if(NSW_BUILD_CLIENT)
  find_package(SDL3 CONFIG QUIET)
  if(NOT SDL3_FOUND AND NSW_FETCH_SDL)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
      URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz
    )
    FetchContent_MakeAvailable(SDL3)
  endif()

  if(TARGET SDL3::SDL3)
    add_executable(SpaceWarClient src/SpaceWarClient.c src/client.c src/cdata.c src/gui.c src/platform.c)
    target_link_libraries(SpaceWarClient PRIVATE SDL3::SDL3)
  else()
    message(WARNING "SDL3 not found and NSW_FETCH_SDL is OFF: skipping SpaceWarClient")
  endif()
endif()

foreach(target SpaceWarServer SpaceWarAI SpaceWarClient)
  if(TARGET ${target})
    if(MATH_LIB)
      target_link_libraries(${target} PRIVATE ${MATH_LIB})
    endif()
    if(WIN32)
      target_link_libraries(${target} PRIVATE ws2_32)
    endif()
  endif()
endforeach()
