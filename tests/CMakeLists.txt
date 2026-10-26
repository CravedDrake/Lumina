find_package(Qt6 REQUIRED COMPONENTS Test)

# ---- test media (generated, never committed) -------------------------------
# Needs ffmpeg on PATH. Without it, tests that need real media QSKIP.
set(LUMINA_TESTDATA "${CMAKE_BINARY_DIR}/testdata")
file(MAKE_DIRECTORY "${LUMINA_TESTDATA}")
find_program(FFMPEG_EXECUTABLE ffmpeg)
if(FFMPEG_EXECUTABLE)
    if(NOT EXISTS "${LUMINA_TESTDATA}/sample.mp4")
        execute_process(COMMAND "${FFMPEG_EXECUTABLE}" -loglevel error -y
            -f lavfi -i testsrc2=size=640x360:rate=30:duration=20
            -f lavfi -i sine=frequency=440:duration=20
            -c:v libx264 -preset ultrafast -pix_fmt yuv420p -c:a aac -shortest
            "${LUMINA_TESTDATA}/sample.mp4")
    endif()
    if(NOT EXISTS "${LUMINA_TESTDATA}/black.mp4")   # plain black: lets tests measure subtitle pixels
        execute_process(COMMAND "${FFMPEG_EXECUTABLE}" -loglevel error -y
            -f lavfi -i color=c=black:size=640x360:rate=25:duration=12
            -f lavfi -i sine=frequency=330:duration=12
            -c:v libx264 -preset ultrafast -pix_fmt yuv420p -c:a aac -shortest
            "${LUMINA_TESTDATA}/black.mp4")
    endif()
    if(NOT EXISTS "${LUMINA_TESTDATA}/long.mp4")    # >20 s from the end so resume points are meaningful
        execute_process(COMMAND "${FFMPEG_EXECUTABLE}" -loglevel error -y
            -f lavfi -i testsrc2=size=320x180:rate=15:duration=120
            -f lavfi -i sine=frequency=220:duration=120
            -c:v libx264 -preset ultrafast -pix_fmt yuv420p -c:a aac -shortest
            "${LUMINA_TESTDATA}/long.mp4")
    endif()
    if(NOT EXISTS "${LUMINA_TESTDATA}/multi.mkv")   # 2 audio tracks, embedded subtitle, 3 chapters
        file(WRITE "${LUMINA_TESTDATA}/chapters.txt"
";FFMETADATA1
[CHAPTER]
TIMEBASE=1/1000
START=0
END=20000
title=Intro
[CHAPTER]
TIMEBASE=1/1000
START=20000
END=40000
title=Middle
[CHAPTER]
TIMEBASE=1/1000
START=40000
END=60000
title=End
")
        file(WRITE "${LUMINA_TESTDATA}/embedded.srt"
"1
00:00:01,000 --> 00:00:50,000
Embedded subtitle WWWW

")
        execute_process(COMMAND "${FFMPEG_EXECUTABLE}" -loglevel error -y
            -f lavfi -i testsrc2=size=320x180:rate=15:duration=60
            -f lavfi -i sine=frequency=440:duration=60
            -f lavfi -i sine=frequency=880:duration=60
            -i "${LUMINA_TESTDATA}/embedded.srt" -i "${LUMINA_TESTDATA}/chapters.txt"
            -map 0:v -map 1:a -map 2:a -map 3:s -map_metadata 4
            -metadata:s:a:0 language=eng -metadata:s:a:1 language=spa -metadata:s:s:0 language=eng
            -c:v libx264 -preset ultrafast -pix_fmt yuv420p -c:a aac -c:s srt
            "${LUMINA_TESTDATA}/multi.mkv")
    endif()
    file(WRITE "${LUMINA_TESTDATA}/black.srt"
"1
00:00:01,000 --> 00:00:09,000
Hello subtitle WWWW

")
    file(WRITE "${LUMINA_TESTDATA}/notmedia.mp4" "this is not a video file")
else()
    message(WARNING "ffmpeg not found: media-dependent tests will be skipped")
endif()

function(lumina_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE lumina_core Qt6::Test)
    target_compile_definitions(${name} PRIVATE LUMINA_TESTDATA="${LUMINA_TESTDATA}")
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;LUMINA_AUDIO_DUMMY=1"
        TIMEOUT 180)
endfunction()

file(GLOB LUMINA_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tst_*.cpp")
foreach(src IN LISTS LUMINA_TEST_SOURCES)
    get_filename_component(n "${src}" NAME_WE)
    lumina_add_test(${n})
endforeach()

# End-to-end: the real window, real key events, real playback (needs the generated media).
if(EXISTS "${LUMINA_TESTDATA}/long.mp4")
    set(LUMINA_SELFTEST_DIR "${CMAKE_BINARY_DIR}/selftest-data")
    add_test(NAME ui_selftest
        COMMAND ${CMAKE_COMMAND} -E env "LUMINA_DATA_DIR=${LUMINA_SELFTEST_DIR}"
                $<TARGET_FILE:lumina> "${LUMINA_TESTDATA}/long.mp4" --ui-selftest)
    set_tests_properties(ui_selftest PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;LUMINA_AUDIO_DUMMY=1" TIMEOUT 120)
endif()
