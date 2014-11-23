file(GLOB sources "*.hpp" "*.cpp")
add_library(lua_wrapper ${sources})
target_link_libraries(lua_wrapper ${LUA_LIBRARIES})
