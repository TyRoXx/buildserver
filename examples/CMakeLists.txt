add_executable(buildonpush build_on_push.cpp)
target_link_libraries(buildonpush buildserver ${SILICIUM_LIBRARIES} ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${URIPARSER_LIBRARY})

add_executable(luaservices lua_services.cpp)
target_link_libraries(luaservices buildserver ${SILICIUM_LIBRARIES} ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${URIPARSER_LIBRARY} ${LUA_LIBRARIES})

add_executable(graph graph.cpp)
target_link_libraries(graph buildserver ${SILICIUM_LIBRARIES} ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${URIPARSER_LIBRARY})
