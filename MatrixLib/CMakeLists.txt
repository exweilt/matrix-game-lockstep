add_library(MatrixLib STATIC)

set(3G_SOURCES
    3G/3g.cpp
    3G/Cache.cpp
    3G/CBillboard.cpp
    3G/DeviceState.cpp
    3G/Form.cpp
    3G/Helper.cpp
    3G/Math3D.cpp
    3G/ShadowProj.cpp
    3G/ShadowStencil.cpp
    3G/Texture.cpp
    3G/VectorObject.cpp
)
set(3G_HEADERS
    3G/3g.hpp
    3G/BigIB.hpp
    3G/BigVB.hpp
    3G/Cache.hpp
    3G/CBillboard.hpp
    3G/D3DControl.hpp
    3G/DeviceState.hpp
    3G/Form.hpp
    3G/Helper.hpp
    3G/Math3D.hpp
    3G/ShadowProj.hpp
    3G/ShadowStencil.hpp
    3G/Texture.hpp
    3G/VectorObject.hpp
)

set(BASE_SOURCES
    Base/CBlockPar.cpp
    Base/CBuf.cpp
    Base/CException.cpp
    Base/CFile.cpp
    Base/CHeap.cpp
    Base/CMain.cpp
    Base/CRC32.cpp
    Base/CReminder.cpp
    Base/CStorage.cpp
    Base/Pack.cpp
    Base/Registry.cpp
    Base/Tracer.cpp
    Base/random.cpp
)
set(BASE_HEADERS
    Base/BaseDef.hpp
    Base/CBlockPar.hpp
    Base/CBuf.hpp
    Base/CException.hpp
    Base/CFile.hpp
    Base/CHeap.hpp
    Base/CMain.hpp
    Base/CRC32.hpp
    Base/CReminder.hpp
    Base/CStorage.hpp
    Base/Mem.hpp
    Base/Pack.hpp
    Base/Registry.hpp
    Base/Tracer.hpp
    Base/Types.hpp
)

set(BITMAP_SOURCES
    Bitmap/CBitmap.cpp
    Bitmap/FilePNG.cpp
)
set(BITMAP_HEADERS
    Bitmap/CBitmap.hpp
    Bitmap/FilePNG.hpp
)

# TODO: remove unused files and replace set with globs
# file(GLOB 3G_SOURCES 3G/*.cpp)
# file(GLOB 3G_HEADERS 3G/*.hpp 3G/*.pch)

# file(GLOB BASE_SOURCES Base/*.cpp)
# file(GLOB BASE_HEADERS Base/*.hpp Base/*.pch)

# file(GLOB BITMAP_SOURCES Bitmap/*.cpp)
# file(GLOB BITMAP_HEADERS Bitmap/*.hpp Bitmap/*.pch)

target_sources(
    MatrixLib
    PRIVATE
        ${3G_SOURCES} ${3G_HEADERS}
        ${BASE_SOURCES} ${BASE_HEADERS}
        ${BITMAP_SOURCES} ${BITMAP_HEADERS}
)

target_include_directories(
    MatrixLib
    PUBLIC
        3G
        Base
        Bitmap
)

target_link_libraries(
    MatrixLib
    PUBLIC
        LIBPNG
        ${DIRECTX9_LIBRARIES}
)

if(MSVC)
    source_group("3G\\Source Files" FILES ${3G_SOURCES})
    source_group("3G\\Header Files" FILES ${3G_HEADERS})
    source_group("Base\\Source Files" FILES ${BASE_SOURCES})
    source_group("Base\\Header Files" FILES ${BASE_HEADERS})
    source_group("Bitmap\\Source Files" FILES ${BITMAP_SOURCES})
    source_group("Bitmap\\Header Files" FILES ${BITMAP_HEADERS})
else()
    list(APPEND COMPILE_DEFINITIONS $<$<CONFIG:Debug>:_DEBUG>)
endif()

# The -include winsock2.h is needed here because for networking winsock2.h has to be included before "windows.h",
#   while there are too many entries of #include "windows.h" to do it in place and it is also included by third parties.
target_compile_options(MatrixLib PRIVATE -include winsock2.h ${COMPILE_OPTIONS})
target_compile_definitions(MatrixLib PRIVATE ${COMPILE_DEFINITIONS} _ALLOW_RTCc_IN_STL)
target_link_options(MatrixLib PUBLIC "$<$<C_COMPILER_ID:MSVC>:-SAFESEH:NO>")