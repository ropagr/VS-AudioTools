cmake_minimum_required(VERSION 3.18)

project(
    VS-AudioTools
    DESCRIPTION "basic audio functions for VapourSynth"
    VERSION 0.1.0
    LANGUAGES CXX
)

add_library(AudioTools SHARED
    ${CMAKE_SOURCE_DIR}/src/plugin.cpp
    ${CMAKE_SOURCE_DIR}/src/config.hpp
    ${CMAKE_SOURCE_DIR}/src/convert.cpp
    ${CMAKE_SOURCE_DIR}/src/convert.hpp
    ${CMAKE_SOURCE_DIR}/src/crossfade.cpp
    ${CMAKE_SOURCE_DIR}/src/crossfade.hpp
    ${CMAKE_SOURCE_DIR}/src/delay.cpp
    ${CMAKE_SOURCE_DIR}/src/delay.hpp
    ${CMAKE_SOURCE_DIR}/src/fade.cpp
    ${CMAKE_SOURCE_DIR}/src/fade.hpp
    ${CMAKE_SOURCE_DIR}/src/fadein.cpp
    ${CMAKE_SOURCE_DIR}/src/fadein.hpp
    ${CMAKE_SOURCE_DIR}/src/fadeout.cpp
    ${CMAKE_SOURCE_DIR}/src/fadeout.hpp
    ${CMAKE_SOURCE_DIR}/src/findpeak.cpp
    ${CMAKE_SOURCE_DIR}/src/findpeak.hpp
    ${CMAKE_SOURCE_DIR}/src/mix.cpp
    ${CMAKE_SOURCE_DIR}/src/mix.hpp
    ${CMAKE_SOURCE_DIR}/src/normalize.cpp
    ${CMAKE_SOURCE_DIR}/src/normalize.hpp
    ${CMAKE_SOURCE_DIR}/src/setsamples.cpp
    ${CMAKE_SOURCE_DIR}/src/setsamples.hpp
    ${CMAKE_SOURCE_DIR}/src/sinetone.cpp
    ${CMAKE_SOURCE_DIR}/src/sinetone.hpp
    ${CMAKE_SOURCE_DIR}/src/common/offset.cpp
    ${CMAKE_SOURCE_DIR}/src/common/offset.hpp
    ${CMAKE_SOURCE_DIR}/src/common/overflow.cpp
    ${CMAKE_SOURCE_DIR}/src/common/overflow.hpp
    ${CMAKE_SOURCE_DIR}/src/common/peak.cpp
    ${CMAKE_SOURCE_DIR}/src/common/peak.hpp
    ${CMAKE_SOURCE_DIR}/src/common/sampletype.cpp
    ${CMAKE_SOURCE_DIR}/src/common/sampletype.hpp
    ${CMAKE_SOURCE_DIR}/src/common/transition.cpp
    ${CMAKE_SOURCE_DIR}/src/common/transition.hpp
    ${CMAKE_SOURCE_DIR}/src/utils/array.hpp
    ${CMAKE_SOURCE_DIR}/src/utils/debug.hpp
    ${CMAKE_SOURCE_DIR}/src/utils/map.hpp
    ${CMAKE_SOURCE_DIR}/src/utils/number.hpp
    ${CMAKE_SOURCE_DIR}/src/utils/sample.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/sample.hpp
    ${CMAKE_SOURCE_DIR}/src/utils/string.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/string.hpp
    ${CMAKE_SOURCE_DIR}/src/utils/vector.hpp
    ${CMAKE_SOURCE_DIR}/src/vsmap/vsmap.cpp
    ${CMAKE_SOURCE_DIR}/src/vsmap/vsmap.hpp
    ${CMAKE_SOURCE_DIR}/src/vsmap/vsmap_common.cpp
    ${CMAKE_SOURCE_DIR}/src/vsmap/vsmap_common.hpp
    ${CMAKE_SOURCE_DIR}/src/vsutils/audio.cpp
    ${CMAKE_SOURCE_DIR}/src/vsutils/audio.hpp
    ${CMAKE_SOURCE_DIR}/src/vsutils/bitshift.hpp
)

target_include_directories(AudioTools
    PRIVATE ${CMAKE_SOURCE_DIR}/include/vapoursynth
            ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(AudioTools PRIVATE cxx_std_20)

target_link_libraries(AudioTools)


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        set(IS_CLANG_GCC OFF)
        set(IS_CLANG_MSVC ON)
    elseif (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
        set(IS_CLANG_GCC ON)
        set(IS_CLANG_MSVC OFF)
    endif()
endif()


target_compile_options(AudioTools
    PRIVATE
        # C++ debug flags for GCC and Clang (GCC-style)
        $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:CXX>,$<OR:$<CXX_COMPILER_ID:GNU>,$<BOOL:${IS_CLANG_GCC}>>>:
            -g -O0 -Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-unused-variable>

        # C++ release flags for GCC and Clang (GCC-style)
        $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:CXX>,$<OR:$<CXX_COMPILER_ID:GNU>,$<BOOL:${IS_CLANG_GCC}>>>:
            -O2 -Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-unused-variable>

        # C++ debug flags for MSVC and Clang (MSVC-style)
        $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:CXX>,$<OR:$<CXX_COMPILER_ID:MSVC>,$<BOOL:${IS_CLANG_MSVC}>>>:
            /W4 /wd4100 /wd4702 /Zc:__cplusplus>

        # C++ release flags for MSVC and Clang (MSVC-style)
        $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:CXX>,$<OR:$<CXX_COMPILER_ID:MSVC>,$<BOOL:${IS_CLANG_MSVC}>>>:
            /W4 /wd4100 /wd4702 /Zc:__cplusplus>
)


if (WIN32)
    target_link_options(AudioTools
        PRIVATE
            # C++ flags for GCC and Clang (GCC-style) on Windows
            $<$<AND:$<LINK_LANGUAGE:CXX>,$<OR:$<CXX_COMPILER_ID:GNU>,$<BOOL:${IS_CLANG_GCC}>>>:
                -static -static-libstdc++ -s>
    )
else()
    target_link_options(AudioTools
        PRIVATE
            # C++ flags for GCC on non-Windows systems
            $<$<LINK_LANG_AND_ID:CXX,GNU>:-s>
    )
endif()


set_target_properties(AudioTools PROPERTIES PREFIX "")
