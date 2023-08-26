add_executable(parwator "parwator.cpp")
target_link_libraries(parwator PRIVATE execution_planner wator project_config argparse)

if(WATOR_BUILD_MAP_READER)
    add_executable(parwatorMapReader "parwatorMapReader.cpp")
    target_link_libraries(parwatorMapReader PRIVATE project_config)
    #target_link_libraries(parwatorMapReader PRIVATE -lturbojpeg)
    target_link_libraries(parwatorMapReader PRIVATE png++)
endif(WATOR_BUILD_MAP_READER)
