add_executable(livemusicengine_test WIN32
    ../common/resource_paths.h
    livemusicengine_test.cpp livemusicengine_test.h
)
target_include_directories(livemusicengine_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(livemusicengine_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
