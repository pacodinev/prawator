# Testing library
FetchContent_Declare(
  catch
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG  v2.13.6)
FetchContent_MakeAvailable(catch)
# Adds Catch2::Catch2

# compile the catch_main as separate target because it is so slow to compile
add_library(catch_main STATIC catch_main.cpp)
target_link_libraries(catch_main PUBLIC Catch2::Catch2)

add_executable(test_execution_plannner test_execution_planner.cpp)
target_link_libraries(test_execution_plannner PRIVATE catch_main
    execution_planner project_config)
add_test(NAME test_execution_plannner COMMAND test_execution_plannner)

#add_executable(LZW_test LZW_test.cpp)
#target_link_libraries(LZW_test PRIVATE catch_main
#    LZW project_config)
#add_test(NAME LZW_test COMMAND LZW_test)
#
#add_executable(archive_parser_test archive_parser_test.cpp)
#target_link_libraries(archive_parser_test PRIVATE catch_main
#    archive_parser project_config)
#add_test(NAME archive_parser_test COMMAND archive_parser_test)

#add_executable(tree_test tree_test.cpp)
#target_link_libraries(tree_test PRIVATE catch_main
#    tree project_config)
#add_test(NAME tree_test COMMAND tree_test)
