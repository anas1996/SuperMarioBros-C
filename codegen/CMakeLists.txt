cmake_minimum_required(VERSION 3.5)

find_package(BISON)
find_package(FLEX)

BISON_TARGET(parser asm6502.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp)
FLEX_TARGET(scanner asm6502.l ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp)
ADD_FLEX_BISON_DEPENDENCY(scanner parser)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

add_executable(codegen
    ${BISON_parser_OUTPUTS}
    ${FLEX_scanner_OUTPUTS}
    ast.cpp
    main.cpp
    translator.cpp
)
set_target_properties(codegen PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

add_custom_command(TARGET codegen
                   POST_BUILD
                   COMMAND $<TARGET_FILE:codegen> ${PROJECT_SOURCE_DIR}/docs/smbdis.asm ${PROJECT_SOURCE_DIR}/source/SMB)
