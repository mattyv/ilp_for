cmake_minimum_required(VERSION 3.16)
project(ilp_for
    VERSION 1.0.0
    DESCRIPTION "Compile-time loop unrolling with full control flow support"
    LANGUAGES CXX
)

# Header-only interface library
add_library(ilp_for INTERFACE)
add_library(ilp_for::ilp_for ALIAS ilp_for)

target_include_directories(ilp_for INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(ilp_for INTERFACE cxx_std_20)

# Installation
include(GNUInstallDirs)

install(TARGETS ilp_for
    EXPORT ilp_for-targets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES ilp_for.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY ilp_for/cpu_profiles DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ilp_for)
install(DIRECTORY ilp_for/detail DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ilp_for)

install(EXPORT ilp_for-targets
    FILE ilp_for-targets.cmake
    NAMESPACE ilp_for::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ilp_for
)

# Package config
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ilp_for-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ilp_for-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ilp_for
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ilp_for-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ilp_for-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ilp_for-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ilp_for
)
