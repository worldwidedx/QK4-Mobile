enable_language(C)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../third_party/ft8_lib ${CMAKE_CURRENT_BINARY_DIR}/codec)
add_library(qk4_ft8 STATIC
    ft8types.cpp ft8types.h ft8session.cpp ft8session.h
    ft8logbook.cpp ft8logbook.h ft8receiver.cpp ft8receiver.h
    qrzlogbook.cpp qrzlogbook.h
    ft8transmitter.cpp ft8transmitter.h ft8radiomode.h
    ft8snr.cpp ft8snr.h
    ../ui/logbookdialog.cpp ../ui/logbookdialog.h
    ../ui/ft8screen.cpp ../ui/ft8screen.h ../ui/ft8waterfall.cpp ../ui/ft8waterfall.h
    ../dsp/panadapter_rhi.cpp ../dsp/panadapter_rhi.h)
target_include_directories(qk4_ft8 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
target_link_libraries(qk4_ft8 PUBLIC qk4_ft8_codec Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)
if(TARGET Qt6::GuiPrivate)
    target_link_libraries(qk4_ft8 PUBLIC Qt6::GuiPrivate)
else()
    # Preserve the main application's fallback for Qt kits without a private CMake config.
    get_target_property(_ft8_gui_includes Qt6::Gui INTERFACE_INCLUDE_DIRECTORIES)
    foreach(dir ${_ft8_gui_includes})
        target_include_directories(qk4_ft8 PUBLIC "${dir}/${Qt6_VERSION}" "${dir}/${Qt6_VERSION}/QtGui")
    endforeach()
endif()
set_target_properties(qk4_ft8 PROPERTIES POSITION_INDEPENDENT_CODE ON AUTOMOC ON)
