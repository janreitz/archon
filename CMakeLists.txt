cmake_minimum_required(VERSION 3.10)
project(archon VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add options
option(ENABLE_PROFILING_FLAGS "Enable profiling-friendly build flags" OFF)
option(ENABLE_TRACY "Enable Tracy instrumentation" OFF)
option(BUILD_TESTS "Build Archon library tests" OFF)
option(BUILD_BENCHMARKS "Build Archon library benchmarks" OFF)
option(ENABLE_STATIC_ANALYSIS "Enable clang-static-analyzer" OFF)

# Include FetchContent for dependencies
include(FetchContent)

# Auto-detect if Tracy should be enabled based on available target or parent definition
if(TARGET Tracy::TracyClient OR DEFINED TRACY_ENABLE)
    set(ENABLE_TRACY ON)
    message(STATUS "Archon: Tracy detected from parent project, enabling profiling")
endif()

# Fetch Tracy if profiling is enabled
if(ENABLE_TRACY)
    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG        v0.11.1
    )
    FetchContent_MakeAvailable(tracy)
    add_definitions(-DTRACY_ENABLE)
endif()

# Fetch Catch2 if tests or benchmarks are enabled
if(BUILD_TESTS OR BUILD_BENCHMARKS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
    )
    FetchContent_MakeAvailable(Catch2)
endif()

# Library source files
file(GLOB_RECURSE ARCHON_LIB_SOURCES
    src/*.cpp
)

# Create the Archon library
add_library(archon ${ARCHON_LIB_SOURCES})

# Configure include directories for the library
target_include_directories(archon 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compiler-specific warning flags
if(MSVC)
    target_compile_options(archon PRIVATE
        /W4          # High warning level
        /WX          # Treat warnings as errors
        /permissive- # Disable non-conforming code
        /w14242      # Conversion possible loss of data
        /w14254      # Operator conversion possible loss of data
        /w14263      # Member function does not override base class virtual member function
        /w14265      # Class has virtual functions but destructor is not virtual
        /w14287      # Unsigned/negative constant mismatch
        /we4289      # Loop control variable used outside loop
        /w14296      # Expression is always false/true
        /w14311      # Pointer truncation
        /w14545      # Expression before comma has no effect
        /w14546      # Function call before comma missing argument list
        /w14547      # Operator before comma has no effect
        /w14549      # Operator before comma has no effect
        /w14555      # Expression has no effect
        /w14619      # Pragma warning: there is no warning number
        /w14640      # Thread unsafe static member initialization
        /w14826      # Conversion is sign-extended
        /w14905      # Wide string literal cast
        /w14906      # String literal cast
        /w14928      # Illegal copy-initialization
    )
else()
    target_compile_options(archon PRIVATE
        -Wall                    # Enable most warning messages
        -Wextra                  # Enable extra warning messages
        -Werror                  # Treat warnings as errors
        -Wpedantic               # Strict ISO C++ compliance
        -Wshadow                 # Warn about variable shadowing
        -Wnon-virtual-dtor       # Warn about non-virtual destructors
        -Wold-style-cast         # Warn about C-style casts
        -Wcast-align             # Warn about pointer casts that increase alignment
        -Wunused                 # Warn about unused variables/functions
        -Woverloaded-virtual     # Warn about overloaded virtual functions
        -Wconversion             # Warn about type conversions
        -Wsign-conversion        # Warn about sign conversions
        -Wmisleading-indentation # Warn about misleading indentation
        -Wnull-dereference       # Warn about null dereferences
        -Wdouble-promotion       # Warn about double promotions
        -Wformat=2               # Extra format string checking
        # GCC-specific warnings
        $<$<CXX_COMPILER_ID:GNU>:-Wduplicated-cond>
        $<$<CXX_COMPILER_ID:GNU>:-Wduplicated-branches>
        $<$<CXX_COMPILER_ID:GNU>:-Wlogical-op>
        $<$<CXX_COMPILER_ID:GNU>:-Wuseless-cast>
    )
    
    # Clang-specific warnings
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(archon PRIVATE
            -Wno-c++98-compat       # We use C++20, disable C++98 compat warnings
            -Wno-c++98-compat-pedantic
        )
    endif()
endif()

if(ENABLE_PROFILING_FLAGS)
    target_compile_options(archon PRIVATE
        $<$<CONFIG:Release>:-O3 -g -DNDEBUG -fno-omit-frame-pointer -ginline-points>
        $<$<CONFIG:RelWithDebInfo>:-O3 -g -DNDEBUG -fno-omit-frame-pointer -ginline-points>
    )
    message(STATUS "Archon: Profiling-friendly build flags enabled")
endif()

# Static analysis with clang-static-analyzer
if(ENABLE_STATIC_ANALYSIS)
    # Try different scan-build variants
    find_program(CLANG_ANALYZER 
        NAMES scan-build scan-build-14 scan-build-13 scan-build-12
        PATHS /usr/share/clang/scan-build-14/bin /usr/bin /usr/local/bin
    )
    
    if(CLANG_ANALYZER)
        message(STATUS "Archon: clang-static-analyzer found at ${CLANG_ANALYZER}")
        
        # Add custom target for static analysis
        add_custom_target(static-analysis
            COMMAND ${CLANG_ANALYZER} --use-analyzer=${CMAKE_CXX_COMPILER} -o ${CMAKE_BINARY_DIR}/static-analysis-report ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running clang-static-analyzer on the codebase"
            VERBATIM
        )
        
        # Add a clean target for analysis reports
        add_custom_target(clean-static-analysis
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/static-analysis-report
            COMMENT "Cleaning static analysis reports"
        )
    else()
        message(WARNING "scan-build not found. Install clang-tools or specify path manually.")
    endif()
endif()


# Link Tracy if profiling is enabled
if(ENABLE_TRACY)
    target_link_libraries(archon PUBLIC Tracy::TracyClient)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()

    # Unit tests
    file(GLOB_RECURSE ARCHON_UNIT_TEST_SOURCES
        tests/*.cpp
    )

    if(ARCHON_UNIT_TEST_SOURCES)
        add_executable(archon_unit_tests ${ARCHON_UNIT_TEST_SOURCES})
        target_link_libraries(archon_unit_tests PRIVATE archon Catch2::Catch2WithMain)
        
        target_include_directories(archon_unit_tests 
            PRIVATE 
                ${CMAKE_CURRENT_SOURCE_DIR}/include
        )

        # Apply same warning flags to tests
        if(MSVC)
            target_compile_options(archon_unit_tests PRIVATE /W4 /WX /permissive-)
        else()
            target_compile_options(archon_unit_tests PRIVATE -Wall -Wextra -Werror -Wpedantic)
        endif()

        include(Catch)
        catch_discover_tests(archon_unit_tests)
    endif()
endif()

# Benchmarks
if(BUILD_BENCHMARKS)
    add_executable(archon_benchmarks benchmark/ecs_benchmarks.cpp)
    
    target_link_libraries(archon_benchmarks 
        PRIVATE 
            archon 
            Catch2::Catch2WithMain 
    )
    
    target_include_directories(archon_benchmarks
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include     # For ecs.h
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark  # For BenchmarkComponents.h
    )

    # Apply warning flags to benchmarks
    if(MSVC)
        target_compile_options(archon_benchmarks PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(archon_benchmarks PRIVATE -Wall -Wextra -Werror -Wpedantic)
    endif()

    if(ENABLE_TRACY)
        target_link_libraries(archon_benchmarks PRIVATE Tracy::TracyClient)
    endif()
    
    # Profile benchmark executable (simplified for perf analysis)
    add_executable(archon_profile_benchmark benchmark/profile_benchmark.cpp)
    target_link_libraries(archon_profile_benchmark 
        PRIVATE 
            archon 
    )
    target_include_directories(archon_profile_benchmark
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark
    )
    
    # Apply warning flags to profile benchmark
    if(MSVC)
        target_compile_options(archon_profile_benchmark PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(archon_profile_benchmark PRIVATE -Wall -Wextra -Werror -Wpedantic)
    endif()
    
    if(ENABLE_TRACY)
        target_link_libraries(archon_profile_benchmark PRIVATE Tracy::TracyClient)
    endif()
    
    # Overhead analysis benchmark
    add_executable(archon_overhead_analysis benchmark/overhead_analysis.cpp)
    target_link_libraries(archon_overhead_analysis 
        PRIVATE 
            archon 
    )
    target_include_directories(archon_overhead_analysis
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark
    )
    
    # Apply warning flags to overhead analysis
    if(MSVC)
        target_compile_options(archon_overhead_analysis PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(archon_overhead_analysis PRIVATE -Wall -Wextra -Werror -Wpedantic)
    endif()
    
    if(ENABLE_TRACY)
        target_link_libraries(archon_overhead_analysis PRIVATE Tracy::TracyClient)
    endif()
endif()

# Installation and packaging
include(GNUInstallDirs)

# Install the library
install(TARGETS archon
    EXPORT archonTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# Install export targets
install(EXPORT archonTargets
    FILE archonTargets.cmake
    NAMESPACE archon::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/archon
)

# Create and install package config files
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/archonConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/archonConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/archon
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/archonConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/archonConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/archonConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/archon
) 