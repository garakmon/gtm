qt_add_library(orderedjson STATIC
    orderedjson.cpp
)

target_link_libraries(orderedjson PUBLIC
    Qt6::Core
)

target_include_directories(orderedjson PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
