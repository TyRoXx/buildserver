file(GLOB sources "*.hpp" "*.cpp")
add_executable(unit_test ${sources})
target_link_libraries(unit_test buildserver lua_wrapper ${Boost_LIBRARIES})
