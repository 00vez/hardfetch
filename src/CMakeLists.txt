cmake_minimum_required(VERSION 3.20)

project(hardfetch
    VERSION 3.5
    LANGUAGES C CXX
    DESCRIPTION "Compact system info tool"
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
    if(HF_ASAN)
        add_compile_options(/fsanitize=address /Zi)
        add_link_options(/fsanitize=address)
    endif()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(WIN32)
    add_executable(hardfetch
        main.c
        output.c
        render.c
        fmt.c
        debug.c
        edid.c
        hf_state.c
        os_info.c
        host.c
        kernel.c
        uptime.c
        shell.c
        terminal.c
        ip_addr.c
        public_ip.c
        display.c
        edid.c
        gpu.cpp
        cpu.c
        cpu_temp.cpp
        memory.c
        memory_spd.cpp
        battery.c
        storage.c
    )
    target_compile_definitions(hardfetch PRIVATE HF_VERSION="${PROJECT_VERSION}")
    target_include_directories(hardfetch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
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
        shell32
    )
    set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
    set(CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES "")
    set_target_properties(hardfetch PROPERTIES LINKER_LANGUAGE C)
else()
    # Linux, WSL2 and macOS share the v0.3.0 layout and the same renderer.
    # On macOS _POSIX_C_SOURCE alone hides the BSD types the system headers
    # use themselves, so the Darwin namespace is asked for explicitly.
    if(APPLE)
        add_compile_definitions(_DARWIN_C_SOURCE _POSIX_C_SOURCE=200809L)
        # A fixed target so the build does not depend on the SDK default.
        # kIOMainPortDefault needs macOS 12, which is the floor.
        set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")
    else()
        add_compile_definitions(_POSIX_C_SOURCE=200809L)
    endif()
    add_compile_options(-Wall -Werror=implicit-function-declaration)
    add_executable(hardfetch
        main.c
        output.c
        render.c
        fmt.c
        debug.c
        edid.c
        hf_state.c
        posix/terminal_posix.c
        posix/display_posix.c
        posix/os_info_posix.c
        posix/host_posix.c
        posix/kernel_posix.c
        posix/uptime_posix.c
        posix/thread_posix.c
        posix/shell_posix.c
        posix/gpu_posix.c
        posix/cpu_posix.c
        posix/cpu_temp_posix.c
        posix/memory_posix.c
        posix/memory_spd_posix.c
        posix/apple_pmgr.c
        posix/apple_smc.c
        posix/battery_posix.c
        posix/storage_posix.c
        posix/ip_addr_posix.c
        posix/public_ip_posix.c
    )
    target_compile_definitions(hardfetch PRIVATE HF_VERSION="${PROJECT_VERSION}")
    target_link_libraries(hardfetch PRIVATE pthread dl)
    if(APPLE)
        target_link_libraries(hardfetch PRIVATE
            "-framework CoreFoundation"
            "-framework CoreGraphics"
            "-framework IOKit")
    endif()
endif()
