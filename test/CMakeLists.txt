list(APPEND tests
  bigint-to-string
  host-function
  host-function-throw
  host-object
  host-object-throw
  prop-name
  symbol-to-string
)

foreach(test IN LISTS tests)
  add_executable(${test} ${test}.cc jsi/jsi.cc)

  set_target_properties(
    ${test}
    PROPERTIES
    C_STANDARD 11
    CXX_STANDARD 20
  )

  target_link_libraries(
    ${test}
    PRIVATE
      jsi_shared
  )

  target_include_directories(
    ${test}
    PRIVATE
      $<TARGET_PROPERTY:jsi,INCLUDE_DIRECTORIES>
  )

  target_compile_definitions(
    ${test}
    PUBLIC
      $<TARGET_PROPERTY:jsi,INTERFACE_COMPILE_DEFINITIONS>
  )

  target_compile_options(
    ${test}
    PUBLIC
      $<TARGET_PROPERTY:jsi,INTERFACE_COMPILE_OPTIONS>
  )

  add_test(
    NAME ${test}
    COMMAND ${test}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )

  set_tests_properties(
    ${test}
    PROPERTIES
    TIMEOUT 30
  )
endforeach()
