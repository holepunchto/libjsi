cmake_minimum_required(VERSION 3.25)

project(jsi C CXX)

if(NOT TARGET js)
  add_subdirectory(vendor/libjs EXCLUDE_FROM_ALL)
endif()

add_library(jsi OBJECT)

set_target_properties(
  jsi
  PROPERTIES
  C_STANDARD 99
  CXX_STANDARD 17
  POSITION_INDEPENDENT_CODE ON
)

target_sources(
  jsi
  INTERFACE
    include/jsi.h
  PRIVATE
    src/jsi.cc
    src/jsi/jsi.h
    src/jsi/jsi-inl.h
)

target_include_directories(
  jsi
  PUBLIC
    include
    $<TARGET_PROPERTY:js,INTERFACE_INCLUDE_DIRECTORIES>
  PRIVATE
    src
)

add_library(jsi_shared SHARED $<TARGET_OBJECTS:jsi>)

set_target_properties(
  jsi_shared
  PROPERTIES
  OUTPUT_NAME jsi
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_link_libraries(
  jsi_shared
  PUBLIC
    js_shared
)

add_library(jsi_static STATIC $<TARGET_OBJECTS:jsi>)

set_target_properties(
  jsi_static
  PROPERTIES
  OUTPUT_NAME jsi
  PREFIX lib
)

target_link_libraries(
  jsi_static
  PUBLIC
    js_static
)

install(TARGETS jsi_shared jsi_static)

install(DIRECTORY include/jsi DESTINATION include)

if(PROJECT_IS_TOP_LEVEL)
  enable_testing()
  add_subdirectory(test)
endif()
