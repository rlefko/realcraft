# CompilerSettings.cmake
# Configures compiler flags for C++20 with platform-specific optimizations

# Identify compiler
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(REALCRAFT_COMPILER_CLANG TRUE)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(REALCRAFT_COMPILER_GCC TRUE)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(REALCRAFT_COMPILER_MSVC TRUE)
endif()

# macOS-specific settings
if(APPLE)
    # Metal ray tracing requires macOS 13.0+
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")
endif()

# Function to apply warning flags to a target
function(realcraft_set_target_warnings target)
    if(REALCRAFT_COMPILER_CLANG OR REALCRAFT_COMPILER_GCC)
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wdouble-promotion
            -Wformat=2
            -Wnull-dereference
            -Wimplicit-fallthrough
            $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>
            $<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>
        )
    elseif(REALCRAFT_COMPILER_MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /Zc:__cplusplus
            /Zc:preprocessor
        )
    endif()
endfunction()

# Function to apply optimization flags to a target
function(realcraft_set_target_compile_options target)
    if(REALCRAFT_COMPILER_CLANG)
        target_compile_options(${target} PRIVATE
            # Debug configuration
            $<$<CONFIG:Debug>:
                -g3
                -O0
                -fno-omit-frame-pointer
            >
            # Release configuration - optimized for ray tracing/physics performance
            $<$<CONFIG:Release>:
                -O3
                -DNDEBUG
                -ffast-math
                -flto=thin
            >
            # RelWithDebInfo for profiling
            $<$<CONFIG:RelWithDebInfo>:
                -O2
                -g
                -DNDEBUG
                -fno-omit-frame-pointer
            >
        )
        # Link options
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Release>:-flto=thin>
        )
        # Add sanitizers in Debug if enabled
        if(REALCRAFT_ENABLE_ASAN)
            target_compile_options(${target} PRIVATE
                $<$<CONFIG:Debug>:-fsanitize=address,undefined>
                $<$<CONFIG:Debug>:-fno-sanitize-recover=all>
            )
            target_link_options(${target} PRIVATE
                $<$<CONFIG:Debug>:-fsanitize=address,undefined>
            )
        endif()
    elseif(REALCRAFT_COMPILER_GCC)
        target_compile_options(${target} PRIVATE
            $<$<CONFIG:Debug>:
                -g3
                -Og
                -fno-omit-frame-pointer
            >
            $<$<CONFIG:Release>:
                -O3
                -DNDEBUG
                -ffast-math
                -flto
            >
            $<$<CONFIG:RelWithDebInfo>:
                -O2
                -g
                -DNDEBUG
                -fno-omit-frame-pointer
            >
        )
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Release>:-flto>
        )
        if(REALCRAFT_ENABLE_ASAN)
            target_compile_options(${target} PRIVATE
                $<$<CONFIG:Debug>:-fsanitize=address,undefined>
                $<$<CONFIG:Debug>:-fno-sanitize-recover=all>
            )
            target_link_options(${target} PRIVATE
                $<$<CONFIG:Debug>:-fsanitize=address,undefined>
            )
        endif()
    elseif(REALCRAFT_COMPILER_MSVC)
        target_compile_options(${target} PRIVATE
            $<$<CONFIG:Debug>:
                /Od
                /Zi
                /RTC1
                /GS
                /sdl
            >
            $<$<CONFIG:Release>:
                /O2
                /Ob3
                /DNDEBUG
                /GL
                /fp:fast
                /GS-
                /Gw
            >
            $<$<CONFIG:RelWithDebInfo>:
                /O2
                /Zi
                /DNDEBUG
            >
        )
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Release>:/LTCG>
        )
    endif()
endfunction()

# Combined function to apply all compiler settings to a target
function(realcraft_configure_target target)
    realcraft_set_target_warnings(${target})
    realcraft_set_target_compile_options(${target})
endfunction()
