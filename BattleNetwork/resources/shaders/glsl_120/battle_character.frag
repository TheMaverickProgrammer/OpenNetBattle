#version 120

const int WHITEOUT  = 0;
const int BLACKOUT  = 1;
const int HIGHLIGHT = 2;
const int ICEOUT    = 3;

uniform sampler2D texture;
uniform sampler2D palette;
uniform bool swapPalette;
uniform bool additiveMode;
uniform float states[4];

vec4 colorize(in vec4 pixel) {
    vec4 color = gl_Color * vec4((pixel.r+pixel.g+pixel.b)/3.0);
    color.a = pixel.a;
    return color;
}

vec4 additive(in vec4 mix_color, in vec4 pixel) {
    vec4 color = (mix_color*mix_color.a) * (vec4(1.0)-pixel);
    color = color + pixel;
    color.a = pixel.a;
    return color;
}

vec4 whiteout(in vec4 pixel) {
    return vec4(255, 255, 255, pixel.a);
}

vec4 blackout(in vec4 pixel) {
    return vec4(0, 0, 0, pixel.a);
}

vec4 highlight(in vec4 pixel) {
    vec3 luminances = vec3(0.2126, 0.7152, 0.0722);
    float luminance = dot(luminances, pixel.rgb);
    float threshold = 0.5;

    luminance = max(0.0, luminance - threshold);

    vec4 color = pixel;
    color.rgb *= sign(luminance);

    if(color.a > 0.0) {
        if(color.rgb == vec3(0,0,0)) { color.rgb = vec3(1,1,0); }
        else { color.rgb = vec3(1,1,1); }
    }

    return color;
}

vec4 palette_swap(in vec4 pixel) {
    vec4 color = texture2D(palette, vec2(pixel.r,0));
    color.a = pixel.a * gl_Color.a;
    return color;
}

void main()
{
    vec4 pixel = texture2D(texture, vec2(gl_TexCoord[0].x, gl_TexCoord[0].y));
    pixel.a *= gl_Color.a;

    if(states[WHITEOUT] > 0.01) {
        gl_FragColor = whiteout(pixel);
        return;
    }

    if(states[BLACKOUT] > 0.01) {
        gl_FragColor = blackout(pixel);
        return;
    }

    if(swapPalette) {
        pixel = palette_swap(pixel);
    }

    if(states[ICEOUT] > 0.01) {
        vec4 bright_blue = vec4(0.8, 0.8, 1.0, 1.0);
        gl_FragColor = additive(bright_blue, pixel);
        return;
    }

    if(states[HIGHLIGHT] > 0.01) {
        gl_FragColor = highlight(pixel);
        return;
    }
    
    if(additiveMode) {
        gl_FragColor = additive(gl_Color, pixel);
        return;
    }

    pixel = colorize(pixel);
    
    gl_FragColor = pixel;
}