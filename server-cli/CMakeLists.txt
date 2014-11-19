file(GLOB sources "*.hpp" "*.cpp")
add_executable(buildserver-cli ${sources})
target_link_libraries(buildserver-cli buildserver ${SILICIUM_LIBRARIES} ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
