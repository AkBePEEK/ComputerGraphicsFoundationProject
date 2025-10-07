#version 450 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler3D noiseTex;
uniform float time;
void main() {
    vec3 coord = vec3(TexCoords, time * 0.1);
    float n = texture(noiseTex, coord).r;
    vec3 fireColor = mix(vec3(1.0, 0.5, 0.0), vec3(1.0, 1.0, 1.0), n);
    float alpha = n * n;
    FragColor = vec4(fireColor, alpha);
}