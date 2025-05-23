add_library(MatrixGameInternal STATIC)

macro(add_matrix_directory DIRECTORY)
    file(GLOB DIR_SOURCES ${DIRECTORY}/*.cpp ${DIRECTORY}/*.c)
    file(GLOB DIR_HEADERS ${DIRECTORY}/*.hpp ${DIRECTORY}/*.h)
    target_sources(MatrixGameInternal PRIVATE ${DIR_SOURCES} ${DIR_HEADERS})
    get_filename_component (GROUP ${DIRECTORY} NAME)
    source_group ("${GROUP}\\Source Files" FILES ${DIR_SOURCES})
    source_group ("${GROUP}\\Header Files" FILES ${DIR_HEADERS})
endmacro()

file(GLOB MATRIX_SOURCES src/*.cpp src/*.c)
file(GLOB MATRIX_HEADERS src/*.hpp src/*.h)

target_sources(
    MatrixGameInternal
    PRIVATE
        ${MATRIX_SOURCES}
        ${MATRIX_HEADERS})

target_include_directories(
    MatrixGameInternal
    PUBLIC
        .
        src
        "$<$<C_COMPILER_ID:MSVC>:${DIRECTX9_INCLUDE_DIRS}>")

# The -include winsock2.h is needed here because for networking winsock2.h has to be included before "windows.h",
#   while there are too many entries of #include "windows.h" to do it in place and it is also included by third parties.
target_compile_options(
    MatrixGameInternal
    PUBLIC
        -include winsock2.h ${COMPILE_OPTIONS})

target_compile_definitions(
    MatrixGameInternal
    PUBLIC
        MATRIXGAME_EXPORTS
        $<$<CONFIG:Release>:ASSERT_OFF>
        _ALLOW_RTCc_IN_STL
        WINDOWS_IGNORE_PACKING_MISMATCH
        ${COMPILE_DEFINITIONS})

target_link_options(
    MatrixGameInternal
    PUBLIC
        "$<$<C_COMPILER_ID:MSVC>:-SAFESEH:NO>"
        ${LINK_OPTIONS})

target_link_libraries(
    MatrixGameInternal
    PUBLIC
        winmm
        ${DIRECTX9_LIBRARIES}
        MatrixLib
        "$<$<C_COMPILER_ID:MSVC>:legacy_stdio_definitions>"
)

if(MATRIXGAME_CHEATS)
    set_source_files_properties(
        src/MatrixFormGame.cpp
        PROPERTIES
            COMPILE_DEFINITIONS CHEATS_ON)
endif()

add_matrix_directory(src/Effects)
add_matrix_directory(src/Interface)
add_matrix_directory(src/Logic)
add_matrix_directory(src/Text)
add_matrix_directory(src/Network)

if(MATRIXGAME_BUILD_DLL)
    add_library(MatrixGame SHARED)
    # remove lib prefix from the DLL name (for GCC build)
    set_target_properties(MatrixGame PROPERTIES PREFIX "")
else()
    set_source_files_properties(
            src/MatrixFormGame.cpp
            src/MatrixGame.cpp
            PROPERTIES
            COMPILE_DEFINITIONS BUILD_EXE
    )
    add_executable(MatrixGame)
endif()

set(MATRIX_RESOURCES
    MatrixGame.ico
    MatrixGame.rc
    Resource.h)

source_group("Resources" FILES ${MATRIX_RESOURCES})

target_sources(MatrixGame PRIVATE
    src/MatrixGame.cpp
    ${MATRIX_RESOURCES}
)

target_link_libraries(MatrixGame MatrixGameInternal ws2_32)

target_link_options(
    MatrixGame
    PRIVATE
        ${LINK_OPTIONS}
)

# Copy the CFG directory from the MatrixGame/
if (NOT MATRIXGAME_BUILD_DLL)
    if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/CFG)
        message(STATUS "Copying the CFG/ directory to ${CMAKE_CURRENT_BINARY_DIR}")
        file(COPY CFG DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    endif ()
endif ()

# Copy the DATA/robots.pkg file from the game
if (NOT MATRIXGAME_BUILD_DLL)
    if (NOT MATRIXGAME_PKG_SPACE_RANGERS)
        set(MATRIXGAME_PKG_SPACE_RANGERS "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Space Rangers HD A War Apart" CACHE PATH "Space Rangers 2 path to copy robots.pkg from.")
    endif ()
    option(MATRIXGAME_PKG_BRING_FROM_GAME "Get the robots.pkg file from the game." TRUE)
    option(MATRIXGAME_PKG_SYMLINK "Create symlink instead of copying(requires admin)." FALSE) # Requires admin privileges on Windows but frees from duplicating 200+ MB.

    # Create DATA directory
    if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/DATA)
        message(STATUS "Creating directory ${CMAKE_CURRENT_BINARY_DIR}/DATA")
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/DATA)
    endif ()

    if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/DATA/robots.pkg AND MATRIXGAME_PKG_BRING_FROM_GAME)
        if (EXISTS ${MATRIXGAME_PKG_SPACE_RANGERS}/DATA/robots.pkg)
            if(MATRIXGAME_PKG_SYMLINK)
                message(STATUS "Symlinking DATA/robots.pkg file.")
                file(CREATE_LINK
                        ${MATRIXGAME_PKG_SPACE_RANGERS}/DATA/robots.pkg
                        ${CMAKE_CURRENT_BINARY_DIR}/DATA/robots.pkg
                        SYMBOLIC
                )
            else ()
                message(STATUS "Copying DATA/robots.pkg file.")
                file(COPY
                        ${MATRIXGAME_PKG_SPACE_RANGERS}/DATA/robots.pkg
                        DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/DATA
                )
            endif ()
        else()
            message(STATUS "Space Rangers 2 not found at `${MATRIXGAME_PKG_SPACE_RANGERS}`")
            message(STATUS "Skipping copying DATA/robots.pkg file.")
            message(STATUS "You'll need to bring the DATA/robots.pkg file yourself in order to start MatrixGame.exe!")
        endif()
    endif ()
endif ()
