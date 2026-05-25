#version 330 core

// -----------------------------------------------------------------------------
// Texture Channel Packer — preview fragment shader.
//
// Mirrors the channel routing performed by tcp::pack() in core/src/pack_job.cpp.
// If you change the CPU pack semantics, change this shader to match — preview
// drift from export is a serious bug.
//
// Routing rules (must match extract_channel in pack_job.cpp):
//   - SourceChannel R/G/B/A → texture sample's r/g/b/a.
//   - SourceChannel::Luminance (sentinel value 4) → Rec.709 (0.2126, 0.7152, 0.0722).
//   - Asking for A on a 1-3 channel source returns 1.0.
//   - Asking for a missing color channel returns 0.0.
//   - An unset (-1) slot index → 0 for RGB, 1 for A.
// -----------------------------------------------------------------------------

in vec2 v_clip;
out vec4 frag_color;

// One sampler per InputSlot. Empty slots are bound to a 1x1 transparent
// texture so sampling is always safe.
uniform sampler2D u_slot_0;
uniform sampler2D u_slot_1;
uniform sampler2D u_slot_2;
uniform sampler2D u_slot_3;

// Per-slot channel-count (1..4). Used to apply the "missing channel" defaults
// without reading garbage from the texture.
uniform int u_slot_channels[4];

// For each output channel: slot index and source channel.
// u_channel_map_slot[d] < 0 means "leave at default" (0 for RGB, 1 for A).
// u_channel_map_source[d]: 0=R, 1=G, 2=B, 3=A, 4=Luminance.
//
// Split into two parallel int arrays (rather than ivec2[4]) so the host can
// upload via Qt's setUniformValueArray for ints, which doesn't support a
// tuple size argument.
uniform int u_channel_map_slot[4];
uniform int u_channel_map_source[4];

// Viewport / image geometry, in widget pixels.
uniform vec2 u_widget_size;
uniform vec2 u_image_size;

// Camera state.
//   u_zoom is widget-pixels-per-image-pixel; 1.0 is 1:1.
//   u_pan_px is where the image center is, relative to widget center.
uniform float u_zoom;
uniform vec2 u_pan_px;

// 0 = RGB, 1 = R, 2 = G, 3 = B, 4 = A only.
uniform int u_view_mode;

// 1 = show checkerboard outside the image and behind alpha; 0 = solid black.
uniform int u_checkerboard;

vec4 sample_slot(int slot, vec2 uv)
{
    if (slot == 0) return texture(u_slot_0, uv);
    if (slot == 1) return texture(u_slot_1, uv);
    if (slot == 2) return texture(u_slot_2, uv);
    return texture(u_slot_3, uv);
}

float extract_channel(int slot_idx, int src_ch, vec2 uv)
{
    int chans = u_slot_channels[slot_idx];
    vec4 px = sample_slot(slot_idx, uv);

    if (src_ch == 4) {
        // Luminance: at least 1 channel; emulate the CPU path which replicates
        // R into G and B when those channels are absent.
        float r = px.r;
        float g = (chans > 1) ? px.g : r;
        float b = (chans > 2) ? px.b : r;
        return 0.2126 * r + 0.7152 * g + 0.0722 * b;
    }

    if (src_ch >= chans) {
        // Missing channel → alpha defaults to 1, color to 0 (matches CPU).
        return (src_ch == 3) ? 1.0 : 0.0;
    }

    if (src_ch == 0) return px.r;
    if (src_ch == 1) return px.g;
    if (src_ch == 2) return px.b;
    return px.a;
}

float default_for_destination(int dst)
{
    return (dst == 3) ? 1.0 : 0.0;
}

vec3 checker_color(vec2 frag_xy)
{
    const float cell = 12.0;
    vec2 c = floor(frag_xy / cell);
    bool on = mod(c.x + c.y, 2.0) < 0.5;
    return on ? vec3(0.32) : vec3(0.22);
}

void main()
{
    // Convert from clip space to widget pixels.
    vec2 widget_px = (v_clip * 0.5 + 0.5) * u_widget_size;

    // Image position: where on the (centered, zoomed, panned) image is this pixel?
    vec2 rel = widget_px - u_widget_size * 0.5 - u_pan_px;
    vec2 image_px = rel / u_zoom + u_image_size * 0.5;
    vec2 uv = image_px / u_image_size;

    // Y flip: GL textures are bottom-up, our images are top-down.
    uv.y = 1.0 - uv.y;

    vec3 bg = (u_checkerboard == 1) ? checker_color(widget_px) : vec3(0.05);

    // Outside the image: just background.
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        frag_color = vec4(bg, 1.0);
        return;
    }

    // Apply the channel map exactly like pack().
    //
    // Note: the variable is called `dst`, not `packed`, because `packed` is
    // a reserved keyword in GLSL (layout qualifier for uniform blocks).
    // Using it as a variable name compiles silently in some drivers and
    // explodes with a "syntax error, unexpected '='" in others. Don't.
    vec4 dst = vec4(0.0, 0.0, 0.0, 1.0);
    for (int d = 0; d < 4; ++d) {
        int slot = u_channel_map_slot[d];
        int src  = u_channel_map_source[d];
        if (slot < 0 || slot > 3) {
            dst[d] = default_for_destination(d);
        } else {
            dst[d] = extract_channel(slot, src, uv);
        }
    }

    // View-mode isolation. The default RGB view composites the packed color
    // over the checkerboard so the user can read the alpha channel directly.
    if (u_view_mode == 1) { frag_color = vec4(vec3(dst.r), 1.0); return; }
    if (u_view_mode == 2) { frag_color = vec4(vec3(dst.g), 1.0); return; }
    if (u_view_mode == 3) { frag_color = vec4(vec3(dst.b), 1.0); return; }
    if (u_view_mode == 4) { frag_color = vec4(vec3(dst.a), 1.0); return; }

    vec3 over_bg = mix(bg, dst.rgb, dst.a);
    frag_color = vec4(over_bg, 1.0);
}
