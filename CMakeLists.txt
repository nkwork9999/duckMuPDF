cmake_minimum_required(VERSION 3.5...3.29)

set(TARGET_NAME pdfmd)
set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# MuPDF ライブラリを検索
find_library(MUPDF_LIB mupdf HINTS /usr/local/lib /opt/homebrew/lib)
find_path(MUPDF_INCLUDE mupdf/fitz.h HINTS /usr/local/include /opt/homebrew/include)

if(NOT MUPDF_LIB OR NOT MUPDF_INCLUDE)
    message(FATAL_ERROR "MuPDF not found. Install with: brew install mupdf (macOS) or apt-get install libmupdf-dev (Linux)")
endif()

message(STATUS "MuPDF lib: ${MUPDF_LIB}")
message(STATUS "MuPDF include: ${MUPDF_INCLUDE}")

set(EXTENSION_SOURCES
    src/pdfmd_extension.cpp
    src/mupdf_wrapper.c
    src/pdf_to_markdown.cpp
)

include_directories(src/include ${MUPDF_INCLUDE})

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

# MuPDF をリンク
target_link_libraries(${EXTENSION_NAME} ${MUPDF_LIB})
target_link_libraries(${LOADABLE_EXTENSION_NAME} ${MUPDF_LIB})

# macOS では追加フレームワークが必要な場合
if(APPLE)
    find_library(MUPDF_THIRD mupdf-third HINTS /usr/local/lib /opt/homebrew/lib)
    if(MUPDF_THIRD)
        target_link_libraries(${EXTENSION_NAME} ${MUPDF_THIRD})
        target_link_libraries(${LOADABLE_EXTENSION_NAME} ${MUPDF_THIRD})
    endif()
endif()

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
