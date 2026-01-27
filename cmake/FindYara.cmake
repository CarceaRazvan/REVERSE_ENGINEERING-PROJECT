# FindYara.cmake

if(WIN32)
    set(YARA_LIB "${CMAKE_SOURCE_DIR}/3rdPartyLibs/Yara/lib/libyara64.lib")
else()
    set(YARA_LIB "${CMAKE_SOURCE_DIR}/3rdPartyLibs/Yara/lib/libyara.a")
endif()

add_library(YARA::YARA STATIC IMPORTED)
set_target_properties(YARA::YARA PROPERTIES
    IMPORTED_LOCATION "${YARA_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/3rdPartyLibs/Yara/libyara/include"
)
