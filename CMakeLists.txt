CMAKE_MINIMUM_REQUIRED(VERSION 3.23)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

project(xlink2 CXX)

add_executable(xlink_tool)

if (MSVC)
    target_compile_options(xlink_tool PRIVATE /Wall)
else()
    target_compile_options(xlink_tool PRIVATE -fno-plt -Wall -Wextra)
endif()

add_subdirectory(lib/zstd/build/cmake zstd)

target_include_directories(xlink_tool PRIVATE include/)
target_include_directories(xlink_tool PRIVATE lib/zstd/lib)

target_link_libraries(xlink_tool PRIVATE libzstd_static)

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
    src/util/crc32.cpp
    src/util/file.cpp
    src/util/sarc.cpp

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