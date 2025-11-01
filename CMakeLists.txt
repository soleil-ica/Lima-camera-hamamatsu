# --------------------------------------------------------------------------
# Dependencies
# --------------------------------------------------------------------------

find_package(yat CONFIG REQUIRED)

# --------------------------------------------------------------------------
# Collect sources and includes
# --------------------------------------------------------------------------

file(GLOB_RECURSE sources src/*.cpp)
set(includedirs
    include/
    dcamsdk4/inc/
)

# --------------------------------------------------------------------------
# Target
# --------------------------------------------------------------------------

add_library(limahamamatsu ${sources})

target_include_directories(limahamamatsu
    PUBLIC 
        ${includedirs}
)

target_link_directories(limahamamatsu
    PRIVATE
        dcamsdk4/lib/win64/
)

target_link_libraries(limahamamatsu 
    PUBLIC
        limacore
        yat::yat
    PRIVATE
        dcamapi
)

limatools_set_library_soversion(limahamamatsu "VERSION")

# --------------------------------------------------------------------------
# Compile definitions
# --------------------------------------------------------------------------

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" HAMAMATSU_VERSION_RAW)
string(STRIP "${HAMAMATSU_VERSION_RAW}" HAMAMATSU_VERSION)

target_compile_definitions(limahamamatsu
    PUBLIC
        HAMAMATSU_NAME=Hamamatsu
        HAMAMATSU_VERSION=${HAMAMATSU_VERSION}
        HAMAMATSU_ENABLED
)

message(STATUS "Camera enabled: Hamamatsu ${HAMAMATSU_VERSION}")

# Windows DLL export
if (WIN32 AND BUILD_SHARED_LIBS)
    target_compile_definitions(limahamamatsu
        PRIVATE
            LIBHAMAMATSU_EXPORTS
    )
endif()

# --------------------------------------------------------------------------
# Install
# --------------------------------------------------------------------------

install(
    TARGETS limahamamatsu 
    LIBRARY DESTINATION lib
)

install(
    DIRECTORY dcamsdk4/dll/win64/
    DESTINATION bin 
    FILES_MATCHING PATTERN "*.dll"
)