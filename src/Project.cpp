#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <numeric>
#include <random>
#include <cmath>

#define GLSL(src) "#version 330 core\n" #src


// ---------- Perlin Noise ----------
float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
float lerp(float a, float b, float t) { return a + t * (b - a); }
float grad(int hash, float x, float y, float z) {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

class PerlinNoise3D {
public:
    PerlinNoise3D(unsigned int seed = 237) {
        p.resize(256);
        std::iota(p.begin(), p.end(), 0);
        std::default_random_engine engine(seed);
        std::shuffle(p.begin(), p.end(), engine);
        p.insert(p.end(), p.begin(), p.end());
    }

    float noise(float x, float y, float z) const {
        int X = (int)floor(x) & 255;
        int Y = (int)floor(y) & 255;
        int Z = (int)floor(z) & 255;

        x -= floor(x);
        y -= floor(y);
        z -= floor(z);

        float u = fade(x), v = fade(y), w = fade(z);
        int A = p[X] + Y, AA = p[A] + Z, AB = p[A + 1] + Z;
        int B = p[X + 1] + Y, BA = p[B] + Z, BB = p[B + 1] + Z;

        return lerp(
            lerp(
                lerp(grad(p[AA], x, y, z), grad(p[BA], x - 1, y, z), u),
                lerp(grad(p[AB], x, y - 1, z), grad(p[BB], x - 1, y - 1, z), u),
                v),
            lerp(
                lerp(grad(p[AA + 1], x, y, z - 1), grad(p[BA + 1], x - 1, y, z - 1), u),
                lerp(grad(p[AB + 1], x, y - 1, z - 1), grad(p[BB + 1], x - 1, y - 1, z - 1), u),
                v),
            w);
    }

private:
    std::vector<int> p;
};

GLuint create3DNoiseTexture(int size, float frequency) {
    std::vector<float> data(size * size * size);
    PerlinNoise3D perlin;
    int idx = 0;
    for (int z = 0; z < size; ++z)
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                data[idx++] = 0.5f + 0.5f * perlin.noise(x * frequency, y * frequency, z * frequency);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_3D, tex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, size, size, size, 0, GL_RED, GL_FLOAT, data.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return tex;
}

// ---------- Shaders ----------
const char* vertexShaderSrc = GLSL(
    layout(location = 0) in vec2 pos;
out vec2 uv;
void main() {
    uv = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
);

// --- Fragment shader: fire + smoke composition ---
const char* fragmentShaderSrc = GLSL(
    out vec4 FragColor;
in vec2 uv;
uniform sampler3D noiseTex;
uniform float time;

float fbm(vec3 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 5; i++) {
        v += a * texture(noiseTex, p).r;
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

void main() {
    // Scroll noise upward over time
    float t = time * 0.2;
    vec3 p = vec3(uv.x * 1.5, uv.y * 2.5 + t, t * 0.5);

    // Base fire turbulence
    float fire = fbm(p);
    fire = pow(fire, 3.0);

    // Smoke turbulence (offset + slower)
    float smoke = fbm(p + vec3(0.0, 1.0, -t * 0.2));
    smoke = smoothstep(0.4, 0.9, smoke);

    // Fire color gradient
    vec3 colFire = mix(vec3(1.0, 0.4, 0.0), vec3(1.0, 1.0, 0.2), fire * 2.0);
    vec3 colSmoke = mix(vec3(0.1, 0.1, 0.1), vec3(0.4, 0.4, 0.4), smoke);

    // Vertical fade-out for top smoke
    float heightMask = smoothstep(0.2, 1.0, uv.y);

    // Mix smoke and fire based on height
    vec3 finalColor = mix(colFire, colSmoke, heightMask);
    FragColor = vec4(finalColor, 1.0);
}
);

// ---------- Shader Setup ----------
GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(shader, 512, nullptr, info);
        std::cerr << "Shader error:\n" << info << std::endl;
    }
    return shader;
}

GLuint createShaderProgram() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ---------- Main ----------
int main() {
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(800, 600, "Fire & Smoke", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    float quadVertices[] = {
        -1, -1,  1, -1,  -1, 1,
         1, -1,  1,  1,  -1, 1
    };
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

    GLuint noiseTex = create3DNoiseTexture(64, 0.08f);
    GLuint shader = createShaderProgram();

    glUseProgram(shader);
    glUniform1i(glGetUniformLocation(shader, "noiseTex"), 0);

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);
        float time = (float)glfwGetTime();
        glUseProgram(shader);
        glUniform1f(glGetUniformLocation(shader, "time"), time);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, noiseTex);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
