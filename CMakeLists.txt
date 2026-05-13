cmake_minimum_required(VERSION 3.16)
project(FixedPointCalc VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_path(READLINE_INCLUDE_DIR readline/readline.h REQUIRED)
find_library(READLINE_LIBRARY readline REQUIRED)
find_library(HISTORY_LIBRARY history)
find_library(TINFO_LIBRARY tinfo)
find_library(NCURSES_LIBRARY ncurses)

add_executable(fpc
  src/main.cpp
  src/fixed.cpp
  src/session.cpp
  src/parser.cpp
  src/repl.cpp
)

target_include_directories(fpc PRIVATE src ${READLINE_INCLUDE_DIR})
target_compile_definitions(fpc PRIVATE FPC_VERSION="${PROJECT_VERSION}")
target_link_libraries(fpc PRIVATE ${READLINE_LIBRARY})
if(HISTORY_LIBRARY)
  target_link_libraries(fpc PRIVATE ${HISTORY_LIBRARY})
endif()
if(TINFO_LIBRARY)
  target_link_libraries(fpc PRIVATE ${TINFO_LIBRARY})
elseif(NCURSES_LIBRARY)
  target_link_libraries(fpc PRIVATE ${NCURSES_LIBRARY})
endif()

enable_testing()
add_subdirectory(tests)

install(TARGETS fpc RUNTIME DESTINATION bin)
