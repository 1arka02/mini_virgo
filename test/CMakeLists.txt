set(TESTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gpu/full_system_tests")

function(create_test test_name source_file)
  add_executable(${test_name} ${source_file})
  target_link_libraries(${test_name} doctest)
  foreach(lib IN LISTS ARGN)
    target_link_libraries(${test_name} ${lib})
  endforeach()
  target_compile_definitions(${test_name} PRIVATE TESTS_DIR="${TESTS_DIR}")
  add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_subdirectory(assembler)
add_subdirectory(gpu)

