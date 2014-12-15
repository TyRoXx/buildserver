file(GLOB sources "*.hpp" "*.cpp")
add_executable(buildonpush ${sources})
target_link_libraries(buildonpush buildserver ${SILICIUM_LIBRARIES} ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
