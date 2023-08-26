# Testing library
FetchContent_Declare(
  catch
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG  v2.13.10)
FetchContent_MakeAvailable(catch)
# Adds Catch2::Catch2

# compile the catch_main as separate target because it is so slow to compile
add_library(catch_main STATIC catch_main.cpp)
target_link_libraries(catch_main PUBLIC Catch2::Catch2)

set(COVERAGE_EXCLUDES ${PROJECT_SOURCE_DIR}/tests/* ${CMAKE_BINARY_DIR}/*)

add_code_coverage_all_targets(EXCLUDE ${COVERAGE_EXCLUDES})

add_executable(test_execution_planner execution_planner.cpp)
target_link_libraries(test_execution_planner PRIVATE catch_main
    execution_planner project_config)
add_test(NAME test_execution_planner COMMAND test_execution_planner)
target_code_coverage(test_execution_planner AUTO ALL EXCLUDE ${COVERAGE_EXCLUDES})

add_executable(test_wator wator_tile.cpp wator_line.cpp wator_map_numa.cpp wator_map.cpp)
target_link_libraries(test_wator PRIVATE catch_main
    wator project_config)
add_test(NAME test_wator COMMAND test_wator)
target_code_coverage(test_wator AUTO ALL EXCLUDE ${COVERAGE_EXCLUDES})

