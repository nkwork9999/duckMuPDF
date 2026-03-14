cmake_minimum_required(VERSION 3.5...3.29)

set(TARGET_NAME pdfmd)
set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# MuPDF をソースからビルド
set(MUPDF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mupdf)
set(MUPDF_BUILD_DIR ${MUPDF_DIR}/build/release)

# Pass CMake's C compiler and architecture flags to MuPDF's make
set(MUPDF_XCFLAGS "-fPIC ${CMAKE_C_FLAGS}")

include(ExternalProject)
ExternalProject_Add(mupdf_build
    SOURCE_DIR ${MUPDF_DIR}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND make -C ${MUPDF_DIR} libs
        CC=${CMAKE_C_COMPILER}
        HAVE_X11=no HAVE_GLUT=no HAVE_CURL=no
        HAVE_TESSERACT=no HAVE_LEPTONICA=no
        build=release "XCFLAGS=${MUPDF_XCFLAGS}"
    BUILD_IN_SOURCE TRUE
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS
        ${MUPDF_BUILD_DIR}/libmupdf.a
        ${MUPDF_BUILD_DIR}/libmupdf-third.a
)

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

# Merge MuPDF static libraries into the extension static library
# so that DuckDB's build system can link everything in one step
set(MUPDF_MERGE_DIR ${CMAKE_BINARY_DIR}/mupdf_objs)
if(APPLE)
    add_custom_command(TARGET ${EXTENSION_NAME} POST_BUILD
        COMMAND libtool -static -o $<TARGET_FILE:${EXTENSION_NAME}>
            $<TARGET_FILE:${EXTENSION_NAME}>
            ${MUPDF_BUILD_DIR}/libmupdf.a
            ${MUPDF_BUILD_DIR}/libmupdf-third.a
        COMMENT "Merging MuPDF into pdfmd_extension (macOS)"
    )
else()
    add_custom_command(TARGET ${EXTENSION_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MUPDF_MERGE_DIR}
        COMMAND ${CMAKE_COMMAND} -E chdir ${MUPDF_MERGE_DIR} ${CMAKE_AR} x ${MUPDF_BUILD_DIR}/libmupdf.a
        COMMAND ${CMAKE_COMMAND} -E chdir ${MUPDF_MERGE_DIR} ${CMAKE_AR} x ${MUPDF_BUILD_DIR}/libmupdf-third.a
        COMMAND ${CMAKE_AR} qcs $<TARGET_FILE:${EXTENSION_NAME}> ${MUPDF_MERGE_DIR}/*.o
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${MUPDF_MERGE_DIR}
        COMMENT "Merging MuPDF into pdfmd_extension (Linux)"
    )
endif()

# Loadable extension links MuPDF directly
target_link_libraries(${LOADABLE_EXTENSION_NAME} ${MUPDF_BUILD_DIR}/libmupdf.a ${MUPDF_BUILD_DIR}/libmupdf-third.a)

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
