#version 450

// Vertex attributes
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;

// Output to fragment shader
layout(location = 0) out vec3 frag_color;

// Uniforms
layout(set = 0, binding = 0) uniform Uniforms {
    mat4 model;
    mat4 view;
    mat4 projection;
} uniforms;

void main() {
    gl_Position = uniforms.projection * uniforms.view * uniforms.model * vec4(in_position, 1.0);
    frag_color = in_color;
}
