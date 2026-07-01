cmake_minimum_required(VERSION 3.29)

project (MeshChecker LANGUAGES CXX VERSION 0.7.19)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_SCAN_FOR_MODULES OFF)

file(STRINGS "/etc/os-release" RESULTS REGEX "^ID=(.*)")
set (DISTRO "${CMAKE_MATCH_1}")
message (STATUS "Linux distro: ${DISTRO}")

if ("${DISTRO}" STREQUAL "linuxmint")
    set(CMAKE_INSTALL_RPATH "/usr/local/qt6.11.0/lib")
endif ()

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    message (STATUS "Using ccache")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(PkgConfig REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Concurrent)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    option(ENABLE_SANITIZER "Enable sanitizer" ON)
else()
    option(ENABLE_SANITIZER "Enable sanitizer" OFF)
endif()
if (ENABLE_SANITIZER AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    message (STATUS "Adress sanitizer enabled")
    add_compile_options(-fsanitize=address -fsanitize=leak -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address -fsanitize=leak -lresolv)
    #set (CMAKE_CXX_FLAGS "-O0 -ggdb3")
endif()

message (STATUS "CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}")
if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    # So valgrind can read our symbols
    set (CMAKE_CXX_FLAGS "-gdwarf-4")
endif ()

message (STATUS "CMAKE_CXX_COMPILER_ID ${CMAKE_CXX_COMPILER_ID}")
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    #add_compile_options (-Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic)
    add_compile_options (-Wall -Wextra-semi -Wextra-semi-stmt -Wmissing-variable-declarations)
    add_compile_options (-Wmissing-prototypes -Wimplicit-float-conversion)
    add_compile_options (-Wdouble-promotion -Wimplicit-int-float-conversion -Wunused-parameter)
    add_compile_options (-Wno-newline-eof)
    #add_compile_options (-Wpadded  -Wshadow)
    add_compile_options (-Werror=return-type)
    add_compile_options (-ffp-contract=fast)
else()
    #add_compile_options (-Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic)
    add_compile_options (-Wall -Wextra-semi -Wmissing-declarations)
    add_compile_options (-Wfloat-conversion)
    add_compile_options (-Wdouble-promotion -Wunused-parameter)
    #add_compile_options (-Wpadded -Wshadow)
    add_compile_options (-Werror=return-type)
    add_compile_options (-lm -mavx)
endif()

#add_compile_options (-gsplit-dwarf)
add_link_options($<$<CONFIG:RELEASE>:-s>)

find_program (MOLD "mold")
if (MOLD)
    add_link_options ("-fuse-ld=mold")
    message (STATUS "Using mold linker")
else ()
    find_program (LLD "lld")
    if (LLD)
        add_link_options ("-fuse-ld=lld")
        message (STATUS "Using lld linker")
    endif ()
endif ()

set (INSTALL_LIB OFF)

add_subdirectory (libSculpt)
add_subdirectory (BadMeshes)
add_subdirectory (Tester)
add_subdirectory (libMeshChecker)
add_subdirectory (libTest)

add_executable (${PROJECT_NAME}
    main.cpp
    README.md
    )

set_target_properties (${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

target_link_libraries (${PROJECT_NAME}
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Concurrent
        libSculpt
        libMeshChecker)

set_property (SOURCE main.cpp PROPERTY COMPILE_DEFINITIONS VERSION="${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

# Install app
install (
    TARGETS ${PROJECT_NAME}
    CONFIGURATIONS Release
    RUNTIME
    DESTINATION /usr/bin)

#cpack stuff
include(InstallRequiredSystemLibraries)

if ("${DISTRO}" STREQUAL "fedora")
    set (CPACK_GENERATOR "RPM")
else()
    set (CPACK_GENERATOR "DEB")
endif ()

#set (CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_NAME} - 3d triangular mesh checker")
set (CPACK_PACKAGE_VENDOR "David Netherwood")
set (CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set (CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set (CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set (CPACK_PACKAGE_CONTACT "david_netherwood@hotmail.com")

#set (CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6gui6, libqt6core6, libqt6concurrent6, libqt6xml6, libzip4,  libqt6svg6, ${SCULPTLIB_DEP_NAME} (>= 0.16.07)")
set (CPACK_DEBIAN_PACKAGE_DEPENDS "homebrewqtcore6.11.0, ${SCULPTLIB_DEP_NAME} (>=0.16.1)")

#set (CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6gui6, libqt6core6, libqt6concurrent6, libqt6xml6, libzip4,  libqt6svg6, ${SCULPTLIB_DEP_NAME} (>= 0.16.07)")
set (CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME})

set (CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
set (CPACK_RPM_PACKAGE_REQUIRES "${SCULPTLIB_DEP_NAME} >= 0.16.07")
set (CPACK_RPM_PACKAGE_GROUP "Applications/Engineering")
set (CPACK_RPM_PACKAGE_RELEASE "1")

include(CPack)
