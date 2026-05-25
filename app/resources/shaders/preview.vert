#version 330 core

// Fullscreen triangle attribute (clip space [-1,1]^2).
layout(location = 0) in vec2 in_pos;

// Position passed through to the fragment shader for screen-space sampling.
out vec2 v_clip;

void main()
{
    v_clip = in_pos;
    gl_Position = vec4(in_pos, 0.0, 1.0);
}
