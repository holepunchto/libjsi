cmake_minimum_required(VERSION 3.25)

find_package(cmake-fetch REQUIRED PATHS node_modules/cmake-fetch)

project(jsi C CXX)

fetch_package("github:holepunchto/libjs")

add_library(jsi OBJECT)

set_target_properties(
  jsi
  PROPERTIES
  C_STANDARD 11
  CXX_STANDARD 20
  POSITION_INDEPENDENT_CODE ON
)

target_sources(
  jsi
  INTERFACE
    include/jsi.h
  PRIVATE
    src/jsi.cc
)

target_include_directories(
  jsi
  PUBLIC
    include
    $<TARGET_PROPERTY:js,INTERFACE_INCLUDE_DIRECTORIES>
  PRIVATE
    src
)

target_compile_definitions(
  jsi
  PUBLIC
    $<TARGET_PROPERTY:js,INTERFACE_COMPILE_DEFINITIONS>
)

target_compile_options(
  jsi
  PUBLIC
    $<TARGET_PROPERTY:js,INTERFACE_COMPILE_OPTIONS>
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

install(FILES include/js.h DESTINATION include)

if(PROJECT_IS_TOP_LEVEL)
  enable_testing()

  add_subdirectory(test)
endif()
