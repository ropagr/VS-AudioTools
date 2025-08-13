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

target_compile_options(AudioTools
    PRIVATE
        # Debug flags for GNU C++
        $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:CXX,GNU>>:
            -g -O0 -Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-unused-variable
        >

        # Release flags for GNU C++
        $<$<AND:$<CONFIG:Release>,$<COMPILE_LANG_AND_ID:CXX,GNU>>:
            -O2 -Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-unused-variable
        >

        # Debug flags for MSVC C++
        $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:CXX,MSVC>>:
            /MDd /W4 /Zi /wd4100 /wd4702 /Zc:__cplusplus
        >

        # Release flags for MSVC C++
        $<$<AND:$<CONFIG:Release>,$<COMPILE_LANG_AND_ID:CXX,MSVC>>:
            /MD /O2 /W4 /wd4100 /wd4702 /Zc:__cplusplus
        >
)


if (WIN32)
    target_link_options(AudioTools
        PRIVATE $<$<LINK_LANG_AND_ID:CXX,GNU>:-fPIC -static -static-libgcc -static-libstdc++ -s>
    )
else()
    target_link_options(AudioTools
        PRIVATE $<$<LINK_LANG_AND_ID:CXX,GNU>:-s>
    )
endif()

set_target_properties(AudioTools PROPERTIES PREFIX "")
