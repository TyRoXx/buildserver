file(GLOB nanowebSources "../nanoweb/*.hpp")
add_executable(buildonpush build_on_push.cpp ${nanowebSources})
target_link_libraries(buildonpush buildserver ${SILICIUM_LIBRARIES} ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${URIPARSER_LIBRARY})

if(Lua51_FOUND)
	add_executable(luaservices lua_services.cpp)
	target_link_libraries(luaservices buildserver ${SILICIUM_LIBRARIES} ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${URIPARSER_LIBRARY} ${LUA_LIBRARIES})
endif()

add_executable(graph graph.cpp)
target_link_libraries(graph buildserver ${SILICIUM_LIBRARIES} ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${URIPARSER_LIBRARY})
