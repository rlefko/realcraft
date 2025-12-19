# src/CMakeLists.txt
# RealCraft main executable

# Add platform subdirectory first (builds realcraft::platform library)
add_subdirectory(platform)

# Add graphics subdirectory (builds realcraft::graphics library)
add_subdirectory(graphics)

add_executable(realcraft
    main.cpp
)

# Apply compiler settings
realcraft_configure_target(realcraft)

# Link dependencies
realcraft_link_dependencies(realcraft)

# Link libraries
target_link_libraries(realcraft
    PRIVATE
        realcraft::platform
        realcraft::graphics
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
