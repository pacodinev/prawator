add_executable(parwatorCG "parwatorCG.cpp")
target_link_libraries(parwatorCG PRIVATE execution_planner wator_gamecg project_config)

if(WATOR_BUILD_MAP_READER)
    add_executable(parwatorMapReader "parwatorMapReader.cpp")
    target_link_libraries(parwatorMapReader PRIVATE project_config)
    #target_link_libraries(parwatorMapReader PRIVATE -lturbojpeg)
    target_link_libraries(parwatorMapReader PRIVATE png++)
endif(WATOR_BUILD_MAP_READER)
