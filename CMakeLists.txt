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
# Merge MuPDF archives into static extension using MRI script (Linux) or libtool (macOS)
set(MRI_SCRIPT ${CMAKE_BINARY_DIR}/merge_mupdf.mri)
if(APPLE)
    add_custom_command(TARGET ${EXTENSION_NAME} POST_BUILD
        COMMAND libtool -static -o $<TARGET_FILE:${EXTENSION_NAME}>
            $<TARGET_FILE:${EXTENSION_NAME}>
            ${MUPDF_BUILD_DIR}/libmupdf.a
            ${MUPDF_BUILD_DIR}/libmupdf-third.a
        COMMENT "Merging MuPDF into pdfmd_extension (macOS)"
    )
else()
    file(WRITE ${MRI_SCRIPT}
"create __OUTPUT__\n\
addlib __EXT_LIB__\n\
addlib __MUPDF__\n\
addlib __MUPDF_THIRD__\n\
save\n\
end\n")
    add_custom_command(TARGET ${EXTENSION_NAME} POST_BUILD
        COMMAND sed -e "s|__OUTPUT__|$<TARGET_FILE:${EXTENSION_NAME}>|"
                    -e "s|__EXT_LIB__|$<TARGET_FILE:${EXTENSION_NAME}>|"
                    -e "s|__MUPDF__|${MUPDF_BUILD_DIR}/libmupdf.a|"
                    -e "s|__MUPDF_THIRD__|${MUPDF_BUILD_DIR}/libmupdf-third.a|"
                    ${MRI_SCRIPT} | ${CMAKE_AR} -M
        COMMENT "Merging MuPDF into pdfmd_extension (Linux MRI)"
    )
endif()

# Loadable extension links MuPDF with whole-archive to resolve circular deps
if(APPLE)
    target_link_libraries(${LOADABLE_EXTENSION_NAME}
        -Wl,-force_load,${MUPDF_BUILD_DIR}/libmupdf.a
        -Wl,-force_load,${MUPDF_BUILD_DIR}/libmupdf-third.a
    )
else()
    target_link_libraries(${LOADABLE_EXTENSION_NAME}
        -Wl,--whole-archive
        ${MUPDF_BUILD_DIR}/libmupdf.a
        ${MUPDF_BUILD_DIR}/libmupdf-third.a
        -Wl,--no-whole-archive
    )
endif()

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
