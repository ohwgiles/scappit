cmake_minimum_required(VERSION 3.0)
project(scappit)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Werror")
find_package(Qt5Widgets REQUIRED)
find_package(Qt5X11Extras REQUIRED)
set(CMAKE_AUTOMOC TRUE)
qt5_add_resources(RES icons.qrc)

add_executable(scappit scappit.cpp ${RES})
set_target_properties(scappit PROPERTIES COMPILE_FLAGS "-std=c++11")
qt5_use_modules(scappit Widgets X11Extras)
target_link_libraries(scappit X11)

install(TARGETS scappit RUNTIME DESTINATION bin)
