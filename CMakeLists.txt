cmake_minimum_required(VERSION 3.20)

project(hardfetch
    VERSION 0.2.1
    LANGUAGES C CXX
    DESCRIPTION "Compact system info tool"
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(HF_COMMON src/main.c src/output.c)

if(WIN32)
    add_executable(hardfetch
        ${HF_COMMON}
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
        src/public_ip.c
    )
    target_include_directories(hardfetch PRIVATE include)
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
    set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
    set(CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES "")
    set_target_properties(hardfetch PROPERTIES LINKER_LANGUAGE C)
else()
    add_compile_definitions(_POSIX_C_SOURCE=200809L)
    add_compile_options(-Wall -Werror=implicit-function-declaration)
    add_executable(hardfetch
        ${HF_COMMON}
        src/posix/os_info_posix.c
        src/posix/host_posix.c
        src/posix/kernel_posix.c
        src/posix/uptime_posix.c
        src/posix/shell_posix.c
        src/posix/gpu_posix.c
        src/posix/cpu_posix.c
        src/posix/cpu_temp_posix.c
        src/posix/memory_posix.c
        src/posix/memory_spd_posix.c
        src/posix/storage_posix.c
        src/posix/ip_addr_posix.c
        src/posix/public_ip_posix.c
    )
    target_link_libraries(hardfetch PRIVATE pthread dl)
    if(APPLE)
        target_link_libraries(hardfetch PRIVATE "-framework CoreFoundation" "-framework IOKit")
    endif()
endif()
