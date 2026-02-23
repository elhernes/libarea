cmake_minimum_required (VERSION 3.24)

project (area-tests)

set (CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(../src libarea)

add_executable(area-test
  GCode.cpp
  test.cpp
  dxfwriter.cpp
)

target_link_libraries(area-test area)

add_executable(visual-ref
  visual_ref.cpp
)

target_link_libraries(visual-ref area)
