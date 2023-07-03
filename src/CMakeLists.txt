#find_package(Boost 1.63.0 REQUIRED COMPONENTS "filesystem")

#add_library(archive_parser STATIC "archive_parser.cpp")
#target_compile_features(archive_parser PUBLIC cxx_rvalue_references)
#target_include_directories(archive_parser PUBLIC "../include" ${Boost_INCLUDE_DIR})
#target_link_libraries(archive_parser PRIVATE LZW project_config ${Boost_FILESYSTEM_LIBRARY})
#
#add_library(LZW STATIC "LZW.cpp")
#target_compile_features(LZW PUBLIC cxx_rvalue_references cxx_std_11)
#target_include_directories(LZW PUBLIC "../include" ${Boost_INCLUDE_DIR})
#target_link_libraries(LZW PRIVATE compressor_base project_config)

add_executable(test_LFSR test_LFSR.cpp)
target_link_libraries(test_LFSR PRIVATE catch_main project_config)
add_test(NAME test_LFSR COMMAND test_LFSR)

add_library(execution_planner STATIC "execution_planner.cpp")
#target_compile_features(LZW PUBLIC cxx_rvalue_references cxx_std_11)
target_include_directories(execution_planner PUBLIC "../include")
target_link_libraries(execution_planner PRIVATE project_config)
if(WATOR_NUMA)
    target_link_libraries(execution_planner PRIVATE -lnuma)
endif(WATOR_NUMA)

add_library(wator_gamecg STATIC "wator_gamecg.cpp")
#target_compile_features(LZW PUBLIC cxx_rvalue_references cxx_std_11)
target_include_directories(wator_gamecg PUBLIC "../include")
target_link_libraries(wator_gamecg PRIVATE project_config)
