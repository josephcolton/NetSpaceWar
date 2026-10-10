cmake_minimum_required(VERSION 3.16)
project(NetSpaceWar VERSION 1.1.0
        DESCRIPTION "A multiplayer network clone of Spacewar!"
        HOMEPAGE_URL "https://github.com/josephcolton/NetSpaceWar"
        LANGUAGES C)
set(NSW_VENDOR "Joseph Colton")

if(WIN32)
  # windres (MinGW) or rc (MSVC), for the game's icon and version resource
  enable_language(RC)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug)
endif()

option(NSW_BUILD_CLIENT "Build the SDL3 game client" ON)
option(NSW_FETCH_SDL "Download and build SDL3 if it is not installed" ON)

# OS_Linux / OS_Darwin / OS_Windows, used by #ifdef blocks in the clients
add_compile_definitions(OS_${CMAKE_SYSTEM_NAME})
# Players can only join a server with the same version (checked at login)
add_compile_definitions(NSW_VERSION="${PROJECT_VERSION}")

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall)
endif()

# Strip symbols from release builds
add_link_options($<$<AND:$<CONFIG:Release>,$<C_COMPILER_ID:GNU,Clang>>:-s>)

if(MSVC)
  # M_PI, and quiet warnings about the classic C and socket calls this code uses
  add_compile_definitions(_USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_library(MATH_LIB m)

# Server: no external dependencies
add_executable(SpaceWarServer src/SpaceWarServer.c src/server.c src/sdata.c src/physics.c src/asteroids.c
               src/options.c src/platform.c)

# AI client: headless, no external dependencies
add_executable(SpaceWarAI src/SpaceWarAI.c src/ai.c src/client.c src/cdata.c src/options.c src/platform.c)

# The game (menu + client): SDL3. Uses an installed SDL3 if there is one, otherwise
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
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(SDL3)
  endif()

  if(TARGET SDL3::SDL3)
    add_executable(NetSpaceWar src/NetSpaceWar.c src/launcher.c src/game.c src/client.c src/cdata.c
                   src/gui.c src/options.c src/platform.c)
    if(WIN32)
      # The icon (also used by SDL for the window) and version info
      configure_file(res/netspacewar.rc.in netspacewar.rc @ONLY)
      target_sources(NetSpaceWar PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/netspacewar.rc)
    endif()
    target_link_libraries(NetSpaceWar PRIVATE SDL3::SDL3)
    # A windowed program on Windows: no console window when double-clicked
    set_target_properties(NetSpaceWar PROPERTIES WIN32_EXECUTABLE TRUE)
    # The menu starts the server and bots from the game's own folder
    add_dependencies(NetSpaceWar SpaceWarServer SpaceWarAI)
  else()
    message(WARNING "SDL3 not found and NSW_FETCH_SDL is OFF: skipping NetSpaceWar")
  endif()
endif()

foreach(target SpaceWarServer SpaceWarAI NetSpaceWar)
  if(TARGET ${target})
    if(MATH_LIB)
      target_link_libraries(${target} PRIVATE ${MATH_LIB})
    endif()
    if(WIN32)
      target_link_libraries(${target} PRIVATE ws2_32)
    endif()
  endif()
endforeach()

# Installing and packaging. All three programs go in one folder, because the
# game starts the server and bots from its own directory.
#   cmake --workflow --preset windows-release    (see CMakePresets.json)
if(WIN32)
  set(NSW_BIN_DIR .)
  set(NSW_DOC_DIR .)
else()
  include(GNUInstallDirs)
  set(NSW_BIN_DIR ${CMAKE_INSTALL_BINDIR})
  set(NSW_DOC_DIR ${CMAKE_INSTALL_DOCDIR})
endif()
foreach(target SpaceWarServer SpaceWarAI NetSpaceWar)
  if(TARGET ${target})
    install(TARGETS ${target} RUNTIME DESTINATION ${NSW_BIN_DIR})
  endif()
endforeach()
install(FILES README.md DESTINATION ${NSW_DOC_DIR} RENAME README.txt)
install(FILES LICENSE DESTINATION ${NSW_DOC_DIR} RENAME LICENSE.txt)

set(CPACK_VERBATIM_VARIABLES ON)
set(CPACK_PACKAGE_NAME "NetSpaceWar")
set(CPACK_PACKAGE_VENDOR "${NSW_VENDOR}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "NetSpaceWar")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
if(WIN32)
  # Inno Setup installer: Program Files, Start menu shortcut, optional desktop
  # shortcut and firewall rule (res/installer.iss), and an uninstaller.
  set(CPACK_GENERATOR INNOSETUP)
  set(CPACK_PACKAGE_FILE_NAME "NetSpaceWar-${PROJECT_VERSION}-setup")
  set(CPACK_PACKAGE_EXECUTABLES "NetSpaceWar" "NetSpaceWar")
  set(CPACK_CREATE_DESKTOP_LINKS "NetSpaceWar")
  set(CPACK_INNOSETUP_RUN_EXECUTABLES "NetSpaceWar")
  set(CPACK_INNOSETUP_PROGRAM_MENU_FOLDER ".")
  set(CPACK_INNOSETUP_ARCHITECTURE x64)
  set(CPACK_INNOSETUP_USE_MODERN_WIZARD ON)
  set(CPACK_INNOSETUP_ICON_FILE "${PROJECT_SOURCE_DIR}/res/netspacewar.ico")
  set(CPACK_INNOSETUP_EXTRA_SCRIPTS "${PROJECT_SOURCE_DIR}/res/installer.iss")
  # Keep the AppId fixed forever: it is how a new version finds and replaces the old one
  set(CPACK_INNOSETUP_SETUP_AppId "{{5B1E8D52-3C7A-4F0E-9A61-2D8C4B7E90F3}")
  set(CPACK_INNOSETUP_SETUP_UninstallDisplayIcon "{app}\\NetSpaceWar.exe")
  set(CPACK_INNOSETUP_SETUP_UninstallDisplayName "NetSpaceWar")
  set(CPACK_INNOSETUP_SETUP_ArchitecturesAllowed x64compatible)
  set(CPACK_INNOSETUP_SETUP_ArchitecturesInstallIn64BitMode x64compatible)
  # ISCC isn't usually on PATH
  find_program(NSW_ISCC ISCC PATHS "$ENV{ProgramFiles\(x86\)}/Inno Setup 6" "$ENV{ProgramFiles}/Inno Setup 6"
               "$ENV{LOCALAPPDATA}/Programs/Inno Setup 6")
  if(NSW_ISCC)
    set(CPACK_INNOSETUP_EXECUTABLE "${NSW_ISCC}")
  endif()
else()
  set(CPACK_GENERATOR TGZ)
endif()
include(CPack)
