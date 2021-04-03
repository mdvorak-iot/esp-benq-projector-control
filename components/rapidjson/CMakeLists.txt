cmake_minimum_required(VERSION 3.15.0)

# IDF component
idf_component_register()

# Library (must be after idf_component_register)
add_library(rapidjson INTERFACE)
target_include_directories(rapidjson INTERFACE ${CMAKE_CURRENT_LIST_DIR}/rapidjson/include)
target_compile_definitions(rapidjson INTERFACE
        RAPIDJSON_HAS_STDSTRING=1
        RAPIDJSON_ALLOCATOR_DEFAULT_CHUNK_CAPACITY=${CONFIG_RAPIDJSON_ALLOCATOR_DEFAULT_CHUNK_CAPACITY}
        RAPIDJSON_VALUE_DEFAULT_ARRAY_CAPACITY=${CONFIG_RAPIDJSON_VALUE_DEFAULT_ARRAY_CAPACITY}
        RAPIDJSON_VALUE_DEFAULT_OBJECT_CAPACITY=${CONFIG_RAPIDJSON_VALUE_DEFAULT_OBJECT_CAPACITY})

# Link
target_link_libraries(${COMPONENT_LIB} INTERFACE rapidjson)
