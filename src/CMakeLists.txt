# src/CMakeLists.txt
# RealCraft main executable

# Add platform subdirectory first (builds realcraft::platform library)
add_subdirectory(platform)

# Add graphics subdirectory (builds realcraft::graphics library)
add_subdirectory(graphics)

# Add core subdirectory (builds realcraft::core library)
add_subdirectory(core)

add_executable(realcraft
    main.cpp
)

# Apply compiler settings
realcraft_configure_target(realcraft)

# Link dependencies
realcraft_link_dependencies(realcraft)

# Link libraries
# realcraft::core includes platform and graphics as public dependencies
target_link_libraries(realcraft
    PRIVATE
        realcraft::core
)

# Include directories
target_include_directories(realcraft
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# Set output directory
set_target_properties(realcraft PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install target
install(TARGETS realcraft
    RUNTIME DESTINATION bin
)
