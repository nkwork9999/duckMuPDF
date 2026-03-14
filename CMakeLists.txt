cmake_minimum_required(VERSION 3.5...3.29)

set(TARGET_NAME pdfmd)
set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# MuPDF をソースからビルド
set(MUPDF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mupdf)
set(MUPDF_BUILD_DIR ${MUPDF_DIR}/build/release)

include(ExternalProject)
ExternalProject_Add(mupdf_build
    SOURCE_DIR ${MUPDF_DIR}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND make -C ${MUPDF_DIR} libs
        HAVE_X11=no HAVE_GLUT=no HAVE_CURL=no
        HAVE_TESSERACT=no HAVE_LEPTONICA=no
        build=release "XCFLAGS=-fPIC"
    BUILD_IN_SOURCE TRUE
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS
        ${MUPDF_BUILD_DIR}/libmupdf.a
        ${MUPDF_BUILD_DIR}/libmupdf-third.a
)

add_library(mupdf_lib STATIC IMPORTED)
set_target_properties(mupdf_lib PROPERTIES IMPORTED_LOCATION ${MUPDF_BUILD_DIR}/libmupdf.a)
add_dependencies(mupdf_lib mupdf_build)

add_library(mupdf_third_lib STATIC IMPORTED)
set_target_properties(mupdf_third_lib PROPERTIES IMPORTED_LOCATION ${MUPDF_BUILD_DIR}/libmupdf-third.a)
add_dependencies(mupdf_third_lib mupdf_build)

set(EXTENSION_SOURCES
    src/pdfmd_extension.cpp
    src/mupdf_wrapper.c
    src/pdf_to_markdown.cpp
)

include_directories(src/include ${MUPDF_DIR}/include)

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

add_dependencies(${EXTENSION_NAME} mupdf_build)
add_dependencies(${LOADABLE_EXTENSION_NAME} mupdf_build)

target_link_libraries(${EXTENSION_NAME} mupdf_lib mupdf_third_lib)
target_link_libraries(${LOADABLE_EXTENSION_NAME} mupdf_lib mupdf_third_lib)

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
