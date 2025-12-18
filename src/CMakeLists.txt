# src/CMakeLists.txt
# RealCraft main executable

add_executable(realcraft
    main.cpp
)

# Apply compiler settings
realcraft_configure_target(realcraft)

# Link dependencies
realcraft_link_dependencies(realcraft)

# Set output directory
set_target_properties(realcraft PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install target
install(TARGETS realcraft
    RUNTIME DESTINATION bin
)
