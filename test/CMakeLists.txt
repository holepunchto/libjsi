list(APPEND tests
  basic
)

foreach(test IN LISTS tests)
  add_executable(${test} ${test}.cc jsi/stubs.cc)

  set_target_properties(
    ${test}
    PROPERTIES
    C_STANDARD 99
    CXX_STANDARD 17
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
