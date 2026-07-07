cmake_minimum_required(VERSION 3.13)
project(tiger-compiler CXX)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include_directories(${PROJECT_SOURCE_DIR}/src)

# ---------------------------------------------------------------------------
# Lab 1: Straight-line program interpreter
# ---------------------------------------------------------------------------
file(GLOB SLP_SOURCES "src/straightline/*.cc")
add_executable(test_slp ${SLP_SOURCES})

# ---------------------------------------------------------------------------
# Labs 2-5: the Tiger compiler (lexer / parser / semant / IR translation)
#
# Unlike the Docker-only flexc++/bisonc++ course framework, this repository
# uses the standard, widely-available flex + bison so it builds on any Linux
# box. The emitted token stream and AST match the course reference output.
# ---------------------------------------------------------------------------
find_package(FLEX)
find_package(BISON)

set(TIGER_LEX ${PROJECT_SOURCE_DIR}/src/tiger/lex/tiger.lex)
set(TIGER_Y   ${PROJECT_SOURCE_DIR}/src/tiger/parse/tiger.y)

if(FLEX_FOUND AND BISON_FOUND AND EXISTS ${TIGER_LEX} AND EXISTS ${TIGER_Y})
  set(LEX_OUT   ${PROJECT_SOURCE_DIR}/src/tiger/lex/scanner.cc)
  set(PARSE_OUT ${PROJECT_SOURCE_DIR}/src/tiger/parse/parser.cc)
  set(PARSE_HDR ${PROJECT_SOURCE_DIR}/src/tiger/parse/parser.h)

  BISON_TARGET(TigerParser ${TIGER_Y} ${PARSE_OUT}
               DEFINES_FILE ${PARSE_HDR})
  FLEX_TARGET(TigerLexer ${TIGER_LEX} ${LEX_OUT})
  ADD_FLEX_BISON_DEPENDENCY(TigerLexer TigerParser)

  file(GLOB TIGER_CORE
    "src/tiger/symbol/*.cc"
    "src/tiger/absyn/*.cc"
    "src/tiger/errormsg/*.cc"
    "src/tiger/env/*.cc"
    "src/tiger/escape/*.cc"
    "src/tiger/semant/*.cc"
    "src/tiger/types/*.cc"
    "src/tiger/frame/*.cc"
    "src/tiger/translate/*.cc"
  )

  set(TIGER_GEN ${FLEX_TigerLexer_OUTPUTS} ${BISON_TigerParser_OUTPUTS})

  add_executable(test_lex
    src/tiger/main/test_lex.cc ${TIGER_CORE} ${TIGER_GEN})

  add_executable(test_parse
    src/tiger/main/test_parse.cc ${TIGER_CORE} ${TIGER_GEN})

  add_executable(test_semant
    src/tiger/main/test_semant.cc ${TIGER_CORE} ${TIGER_GEN})

  add_executable(tiger-compiler
    src/tiger/main/main.cc ${TIGER_CORE} ${TIGER_GEN})
endif()
