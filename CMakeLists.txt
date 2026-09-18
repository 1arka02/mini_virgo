cmake_minimum_required(VERSION 3.10)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(BBGPU VERSION 0.1)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)

  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set(COMPILE_FLAGS -Wall -Wextra -Wpedantic -Wconversion)
endif()

if(MSVC)
  add_compile_options(/W4)
  add_compile_options(/permissive-)
endif()

option(ENABLE_SANITIZERS "Enable sanitizers." OFF)
if(ENABLE_SANITIZER)
  message("- SANITIZERS ENABLED")
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(SANITIZER_AVAILABLE_AND_SET
        TRUE
        CACHE BOOL "" FORCE)
    mark_as_advanced(SANITIZER_AVAILABLE_AND_SET)
  endif()

  if(NOT SANITIZER_AVAILABLE_AND_SET)
    message(VERBOSE
            "The option ENABLE_SANITIZER is set but sanitizers not found.")
  else()
    set(SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
  endif()
endif()

option(ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
if(ENABLE_CLANG_TIDY)
  message("- CLANG-TIDY ENABLED")
  find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
  if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY
        "${CLANG_TIDY_EXE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy;--use-color"
    )
  else()
    message(
      WARNING
        "The option ENABLE_CLANG_TIDY is set but clang-tidy executable is not found."
    )
  endif()
endif()

set(RESOURCES_DIR "${CMAKE_SOURCE_DIR}/resources")

add_subdirectory(external)
add_subdirectory(src)
add_subdirectory(sim)

# option(ENABLE_TESTBENCHES "Enables testbenches" ON)
# if(ENABLE_TESTBENCHES)
#   add_subdirectory(testbenches)
# endif()

option(ENABLE_TESTS "Enables tests" ON)
if(ENABLE_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(test)
endif()

# option(ENABLE_EXAMPLES "Enables examples" ON)
# if(ENABLE_EXAMPLES)
#   add_subdirectory(examples)
# endif()
