cmake_minimum_required(VERSION 3.16)
project(QK4FrequencyEntryTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui GuiPrivate ShaderTools Widgets Test Network)
add_subdirectory(../src/ft8 ${CMAKE_CURRENT_BINARY_DIR}/ft8)
qt_add_executable(test_ft8 test_ft8.cpp
    ../src/ui/frequencydisplaywidget.cpp ../src/ui/frequencydisplaywidget.h
    ../src/ui/frequencyentryparser.cpp ../src/ui/frequencyentryparser.h
    ../src/hardware/midimapping.cpp ../src/hardware/midiinputrouter.cpp
    ../src/dsp/minipan_rhi.cpp ../src/dsp/minipan_rhi.h
    ../src/ui/wheelaccumulator.cpp ../src/ui/wheelaccumulator.h
    ../src/ui/inwindowdialog.cpp ../src/ui/inwindowdialog.h
    ../src/ui/k4styles.cpp ../src/ui/k4styles.h
    ../src/ui/fnpopupwidget.cpp ../src/ui/fnpopupwidget.h
    ../src/ui/k4popupbase.cpp ../src/ui/k4popupbase.h
    ../src/settings/radiosettings.cpp ../src/settings/radiosettings.h)
target_link_libraries(test_ft8 PRIVATE qk4_ft8 Qt6::Test)
qt_add_resources(test_ft8 "ft8_test_icons"
    PREFIX "/"
    BASE "../resources"
    FILES ../resources/icons/check.svg)
qt6_add_shaders(test_ft8 "ft8_test_shaders"
    PREFIX "/shaders"
    BASE ".."
    FILES
        ../src/dsp/shaders/spectrum.vert
        ../src/dsp/shaders/spectrum.frag
        ../src/dsp/shaders/spectrum_blue.vert
        ../src/dsp/shaders/spectrum_blue_amp.frag
        ../src/dsp/shaders/waterfall.vert
        ../src/dsp/shaders/waterfall.frag
        ../src/dsp/shaders/overlay.vert
        ../src/dsp/shaders/overlay.frag)

qt_add_executable(test_frequencyentryparser
    test_frequencyentryparser.cpp
    ../src/ui/frequencyentryparser.cpp
    ../src/ui/frequencyentryparser.h
)
target_include_directories(test_frequencyentryparser PRIVATE ../src)
target_link_libraries(test_frequencyentryparser PRIVATE Qt6::Core Qt6::Test)

enable_testing()
qt_add_executable(test_overlayback test_overlayback.cpp
    ../src/ui/inwindowdialog.cpp ../src/ui/k4styles.cpp)
target_include_directories(test_overlayback PRIVATE ../src)
target_link_libraries(test_overlayback PRIVATE Qt6::Widgets Qt6::Test)
add_test(NAME overlayback COMMAND test_overlayback)
qt_add_executable(test_logbook test_logbook.cpp
    ../src/ui/inwindowdialog.cpp ../src/ui/k4styles.cpp)
target_link_libraries(test_logbook PRIVATE qk4_ft8 Qt6::Test)
add_test(NAME logbook COMMAND test_logbook -o logbook-results.txt,txt -o -,txt)
qt_add_executable(test_qrzlogbook test_qrzlogbook.cpp)
target_link_libraries(test_qrzlogbook PRIVATE qk4_ft8 Qt6::Test Qt6::Network)
add_test(NAME qrzlogbook COMMAND test_qrzlogbook -o qrzlogbook-results.txt,txt -o -,txt)
# Optional, explicit local source checkout; never downloads or changes Android's codec.
if(DEFINED ENV{QK4_TEST_OPUS_SOURCE})
    enable_language(C)
    set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    add_subdirectory("$ENV{QK4_TEST_OPUS_SOURCE}" ${CMAKE_CURRENT_BINARY_DIR}/test-opus EXCLUDE_FROM_ALL)
    qt_add_executable(test_digitalcodec test_digitalcodec.cpp ../src/audio/opusencoder.cpp ../src/audio/opusencoder.h)
    target_include_directories(test_digitalcodec PRIVATE ../src ../third_party/android/opus/include)
    target_link_libraries(test_digitalcodec PRIVATE opus Qt6::Core Qt6::Test)
    add_test(NAME digitalcodec COMMAND test_digitalcodec -o digitalcodec-results.txt,txt -o -,txt)
    set(digital_codec_test digitalcodec)
endif()
qt_add_executable(test_digitaltx test_digitaltx.cpp
    ../src/audio/digitaltxguard.cpp ../src/audio/digitaltxguard.h
    ../src/network/tcpclient.cpp ../src/network/tcpclient.h
    ../src/network/protocol.cpp ../src/network/protocol.h)
target_include_directories(test_digitaltx PRIVATE ../src)
target_link_libraries(test_digitaltx PRIVATE Qt6::Core Qt6::Network Qt6::Test)
add_test(NAME digitaltx COMMAND test_digitaltx -o digitaltx-results.txt,txt -o -,txt)
qt_add_executable(test_digitalui test_digitalui.cpp
    ../src/ui/digitalaudiosetupdialog.cpp ../src/ui/digitalaudiosetupdialog.h
    ../src/ui/inwindowdialog.cpp ../src/ui/k4styles.cpp
    ../src/ui/sstvscreen.cpp ../src/ui/sstvcomposercanvas.cpp
    ../src/sstv/sstvencoder.cpp ../src/sstv/sstvmoderegistry.cpp ../src/sstv/sstvstorage.cpp
    ../src/android/sstvmedia.cpp)
target_include_directories(test_digitalui PRIVATE ../src)
target_link_libraries(test_digitalui PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME digitalui COMMAND test_digitalui -o digitalui-results.txt,txt -o -,txt)
add_test(NAME ft8 COMMAND test_ft8 -o ft8-results.txt,txt -o -,txt)
add_test(NAME frequencyentryparser COMMAND test_frequencyentryparser)

qt_add_executable(test_radiostate
    test_radiostate.cpp
    ../src/models/radiostate.cpp
    ../src/models/radiostate.h
)
target_include_directories(test_radiostate PRIVATE ../src)
target_link_libraries(test_radiostate PRIVATE Qt6::Core Qt6::Test)
add_test(NAME radiostate COMMAND test_radiostate)

qt_add_executable(test_macros test_macros.cpp
    ../src/network/tcpclient.cpp ../src/network/tcpclient.h
    ../src/network/protocol.cpp ../src/network/protocol.h
    ../src/audio/digitaltxguard.cpp ../src/audio/digitaltxguard.h
    ../src/models/radiostate.cpp ../src/models/radiostate.h
    ../src/models/menumodel.cpp ../src/models/menumodel.h)
target_include_directories(test_macros PRIVATE ../src)
target_link_libraries(test_macros PRIVATE Qt6::Core Qt6::Network Qt6::Test)
add_test(NAME macros COMMAND test_macros -o macros-results.txt,txt -o -,txt)

qt_add_executable(test_midimapping test_midimapping.cpp
    ../src/hardware/midimapping.cpp ../src/hardware/midimapping.h
    ../src/hardware/midiinputrouter.cpp ../src/hardware/midiinputrouter.h)
target_include_directories(test_midimapping PRIVATE ../src)
target_link_libraries(test_midimapping PRIVATE Qt6::Core Qt6::Test)
add_test(NAME midimapping COMMAND test_midimapping)

qt_add_executable(test_fnkeymemory test_fnkeymemory.cpp
    ../src/settings/fnkeymemory.cpp ../src/settings/fnkeymemory.h)
target_include_directories(test_fnkeymemory PRIVATE ../src)
target_link_libraries(test_fnkeymemory PRIVATE Qt6::Core Qt6::Test)
add_test(NAME fnkeymemory COMMAND test_fnkeymemory)

qt_add_executable(test_modepopupwidget
    test_modepopupwidget.cpp
    ../src/ui/modepopupwidget.cpp
    ../src/ui/modepopupwidget.h
    ../src/ui/k4popupbase.cpp
    ../src/ui/k4popupbase.h
    ../src/ui/k4styles.cpp
    ../src/ui/k4styles.h
)
target_include_directories(test_modepopupwidget PRIVATE ../src)
target_link_libraries(test_modepopupwidget PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME modepopupwidget COMMAND test_modepopupwidget)

qt_add_executable(test_frequencydisplaywidget
    test_frequencydisplaywidget.cpp
    ../src/ui/frequencydisplaywidget.cpp
    ../src/ui/frequencydisplaywidget.h
    ../src/ui/wheelaccumulator.cpp
    ../src/ui/wheelaccumulator.h
    ../src/ui/k4styles.cpp
    ../src/ui/k4styles.h
)
target_include_directories(test_frequencydisplaywidget PRIVATE ../src)
target_link_libraries(test_frequencydisplaywidget PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME frequencydisplaywidget COMMAND test_frequencydisplaywidget)

qt_add_executable(test_sstvencoder
    test_sstvencoder.cpp
    ../src/sstv/sstvmoderegistry.cpp
    ../src/sstv/sstvmoderegistry.h
    ../src/sstv/sstvencoder.cpp
    ../src/sstv/sstvencoder.h
    ../src/sstv/sstvdecoder.cpp
    ../src/sstv/sstvdecoder.h
)
target_include_directories(test_sstvencoder PRIVATE ../src)
target_link_libraries(test_sstvencoder PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME sstvencoder COMMAND test_sstvencoder)

qt_add_executable(test_sstvcomposer
    test_sstvcomposer.cpp
    ../src/ui/sstvcomposercanvas.cpp
    ../src/ui/sstvcomposercanvas.h
)
target_include_directories(test_sstvcomposer PRIVATE ../src)
target_link_libraries(test_sstvcomposer PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME sstvcomposer COMMAND test_sstvcomposer)

qt_add_executable(test_sstvstorage
    test_sstvstorage.cpp
    ../src/sstv/sstvstorage.cpp
    ../src/sstv/sstvstorage.h
)
target_include_directories(test_sstvstorage PRIVATE ../src)
target_link_libraries(test_sstvstorage PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME sstvstorage COMMAND test_sstvstorage)

cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH compiler_runtime_dir)
set_tests_properties(${digital_codec_test} overlayback logbook qrzlogbook digitalui digitaltx ft8 midimapping fnkeymemory frequencyentryparser frequencydisplaywidget radiostate macros modepopupwidget sstvencoder sstvcomposer sstvstorage PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        "PATH=path_list_prepend:${compiler_runtime_dir}"
        "QT_QPA_PLATFORM=set:offscreen"
)
