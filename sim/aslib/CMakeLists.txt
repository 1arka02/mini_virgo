add_library(AsLib STATIC lexer.cpp parser_utils.cpp parser.cpp data_reader.cpp emitter.cpp)

target_link_libraries(AsLib PUBLIC Sim GPU)

target_include_directories(AsLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
