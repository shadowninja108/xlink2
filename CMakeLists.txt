CMAKE_MINIMUM_REQUIRED(VERSION 3.23)

project(xlink2 CXX)

include(CheckIPOSupported)
check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT ERROR)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_executable(xlink_tool)

if(LTO_SUPPORTED)
    message(STATUS "Enabling LTO")
    set_property(TARGET xlink_tool PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "LTO not supported: <${ERROR}>")
endif()

if (MSVC)
    target_compile_options(xlink_tool PRIVATE /Wall)
else()
    target_compile_options(xlink_tool PRIVATE -fno-plt -Wall -Wextra)
endif()

set(BUILD_TESTING OFF)

add_subdirectory(lib/zstd/build/cmake)
add_subdirectory(lib/libyaml)
add_subdirectory(lib/ryml)

target_include_directories(xlink_tool PRIVATE include/)
target_include_directories(xlink_tool PRIVATE lib/zstd/lib)
target_include_directories(xlink_tool PRIVATE lib/libyaml/include)
target_include_directories(xlink_tool PRIVATE lib/ryml/src)

target_link_libraries(xlink_tool PRIVATE libzstd_static yaml ryml)

target_sources(xlink_tool PRIVATE
    include/res/action.h
    include/res/arrangeParam.h
    include/res/assetCallTable.h
    include/res/condition.h
    include/res/containerParam.h
    include/res/curve.h
    include/res/param.h
    include/res/paramDefineTable.h
    include/res/property.h
    include/res/random.h
    include/res/trigger.h
    include/res/user.h

    include/util/common.h
    include/util/crc32.h
    include/util/file.h
    include/util/sarc.h
    include/util/types.h
    include/util/error.h
    include/util/yaml.h
    src/util/crc32.cpp
    src/util/file.cpp
    src/util/sarc.cpp
    src/util/yaml.cpp

    include/resource.h

    include/accessor.h
    include/act.h
    include/action.h
    include/arrange.h
    include/condition.h
    include/container.h
    include/param.h
    include/pdt.h
    include/property.h
    include/pdt.h
    include/serializer.h
    include/system.h
    include/trigger.h
    include/user.h
    include/value.h

    src/accessor.cpp
    src/pdt.cpp
    src/serializer.cpp
    src/system.cpp
    src/user.cpp

    src/main.cpp
)