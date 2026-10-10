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

find_library(MATH_LIB m)

# Server: no external dependencies
add_executable(SpaceWarServer src/SpaceWarServer.c src/server.c src/sdata.c src/physics.c)

# AI client: headless, no external dependencies
add_executable(SpaceWarAI src/SpaceWarAI.c src/ai.c src/client.c src/cdata.c)

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
    add_executable(SpaceWarClient src/SpaceWarClient.c src/client.c src/cdata.c src/gui.c)
    target_link_libraries(SpaceWarClient PRIVATE SDL3::SDL3)
  else()
    message(WARNING "SDL3 not found and NSW_FETCH_SDL is OFF: skipping SpaceWarClient")
  endif()
endif()

foreach(target SpaceWarServer SpaceWarAI SpaceWarClient)
  if(TARGET ${target} AND MATH_LIB)
    target_link_libraries(${target} PRIVATE ${MATH_LIB})
  endif()
endforeach()
