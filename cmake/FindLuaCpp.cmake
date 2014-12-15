find_path(LUACPP_INCLUDE_DIR "luacpp/lua.hpp" HINTS "/usr/local/include" "/usr/include")
set(LUACPP_INCLUDE_DIRS ${LUACPP_INCLUDE_DIR})
mark_as_advanced(SILICIUM_INCLUDE_DIR)
