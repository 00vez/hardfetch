cmake_minimum_required(VERSION 3.20)

project(hardfetch
    VERSION 0.1.0
    LANGUAGES C CXX
    DESCRIPTION "Compact Windows system info tool"
)

if(NOT WIN32)
    message(FATAL_ERROR "hardfetch requires Windows")
endif()

# --- Languages ---
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- MSVC Flags ---
if(MSVC)
    add_compile_options(/W4)
    # Static runtime (/MT)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# --- Source files ---
add_executable(hardfetch
    src/main.c
    src/output.c
    src/os_info.c
    src/host.c
    src/kernel.c
    src/uptime.c
    src/shell.c
    src/ip_addr.c
    src/gpu.cpp
    src/cpu.c
    src/cpu_temp.cpp
    src/memory.c
    src/memory_spd.cpp
    src/storage.c
    src/network.c
)

# --- Include dirs ---
target_include_directories(hardfetch PRIVATE include)

# --- Windows system libraries ---
target_link_libraries(hardfetch PRIVATE
    iphlpapi
    ws2_32
    pdh
    wbemuuid
    ole32
    oleaut32
    uuid
    ntdll
    advapi32
    version
    winhttp
)

# --- Prevent linking to libstdc++ when C++ is mixed in ---
set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
set(CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES "")
set_target_properties(hardfetch PROPERTIES LINKER_LANGUAGE C)
