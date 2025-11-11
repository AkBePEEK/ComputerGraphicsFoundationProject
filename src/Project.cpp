/**
 * @file Project.cpp
 * @author Akhbergen Berdiyar and Mulkulanova Lada
 * @brief Fire & Smoke Interactive Shader — курсовой проект по дисциплине
 *        "Computer Graphics Fundamentals".
 *
 * @details
 * Этот проект демонстрирует реализацию процедурного огня и дыма
 * с использованием 3D-шума Перлина и фрактального броуновского движения (FBM)
 * в шейдере. Эффект рендерится на полноэкранном кваде с помощью OpenGL 3.3 Core.
 *
 * Основные особенности:
 * - Генерация 3D-текстуры шума (Perlin Noise) на CPU
 * - Продвинутый фрагментный шейдер с FBM, искрами и размытием
 * - Интерактивное управление: пауза, скорость, цветовые режимы
 * - Отображение FPS в заголовке окна
 *
 * Управление:
 *   SPACE — пауза/воспроизведение
 *   + / - — ускорение/замедление анимации
 *   C     — переключение цветовой схемы (огонь → лава → синее пламя)
 *   R     — сброс настроек
 *
 * Зависимости:
 *   - OpenGL 3.3+
 *   - GLFW 3.3+
 *   - GLAD (сгенерирован для OpenGL 3.3 Core Profile)
 *
 * Сборка: Visual Studio 2022, MinGW, или любой современный C++17-совместимый компилятор.
 */

#include <iostream>
#include <vector>
#include <numeric>
#include <random>
#include <cmath>
#include <string>

// GLAD должен быть ПЕРВЫМ OpenGL-заголовком!
#include <glad/glad.h>

// GLFW — после GLAD
#define GLFW_NO_MAIN
#include <GLFW/glfw3.h>

#define GLSL(src) "#version 330 core\n" #src

/**
 * @class PerlinNoise3D
 * @brief Класс для генерации 3D-шума Перлина.
 *
 * Использует перестановочный массив (permutation table) и градиентные функции
 * для создания непрерывного, плавного шума в трёхмерном пространстве.
 * Используется для заполнения 3D-текстуры, передаваемой в шейдер.
 */
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

/**
 * @brief Создаёт 3D-текстуру на основе шума Перлина.
 *
 * Текстура заполняется значениями шума в диапазоне [0, 1],
 * нормализованными из [-1, 1]. Используется в шейдере как `sampler3D`.
 *
 * @param size — размер текстуры по каждой оси (size x size x size)
 * @param frequency — частота шума (меньше = плавнее)
 * @return GLuint — ID созданной OpenGL-текстуры
 */
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
        uv = pos * 0.5 + 0.7;
        gl_Position = vec4(pos, 0.0, 1.0);
    }
);

// --- Fragment shader: fire + smoke composition ---
const char* fragmentShaderSrc = GLSL(
    out vec4 FragColor;
in vec2 uv;
uniform sampler3D noiseTex;
uniform float time;
uniform int colorMode;

// FBM with more octaves for detail
float fbm(vec3 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 6; i++) {
        v += a * texture(noiseTex, p).r;
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

// Spark particles
float sparks(vec2 uv, float t) {
    // Use high-frequency noise for particles
    vec3 p = vec3(uv * 8.0, t * 0.3);
    float n = fbm(p + vec3(100.0, 0.0, 0.0)); // offset to avoid fire pattern
    // Only in lower half, with pulsing
    float height = 1.0 - smoothstep(0.2, 0.8, uv.y);
    float pulse = sin(t * 10.0 + uv.x * 50.0) * 0.5 + 0.5;
    return n * height * pulse * 0.7;
}

vec3 getFireColor(float fire) {
    if (colorMode == 1) {
        return mix(vec3(0.8, 0.1, 0.0), vec3(1.0, 0.4, 0.0), fire * 2.0);
    }
    else if (colorMode == 2) {
        return mix(vec3(0.0, 0.2, 0.8), vec3(0.2, 0.8, 1.0), fire * 2.0);
    }
    else {
        return mix(vec3(1.0, 0.4, 0.0), vec3(1.0, 1.0, 0.2), fire * 2.0);
    }
}

void main() {
    float t = time * 0.2;
    vec3 p = vec3(uv.x * 1.5, uv.y * 2.5 + t, t * 0.5);

    float fire = pow(fbm(p), 3.0);
    float smoke = smoothstep(0.4, 0.9, fbm(p + vec3(0.0, 1.0, -t * 0.2)));
    float heightMask = smoothstep(0.2, 1.0, uv.y);

    // --- Heat Distortion ---
    // Distort UV based on fire intensity and gradient
    vec2 distortion = vec2(
        fbm(p + vec3(0.5, 0.0, t * 0.3)) - 0.5,
        fbm(p + vec3(0.0, 0.5, t * 0.3)) - 0.5
    ) * fire * 0.03; // scale distortion

    vec2 distortedUV = uv + distortion;

    // Recompute fire with distorted UV for consistency
    vec3 pDistorted = vec3(distortedUV.x * 1.5, distortedUV.y * 2.5 + t, t * 0.5);
    float fireDistorted = pow(fbm(pDistorted), 3.0);
    vec3 colFire = getFireColor(fireDistorted);

    // --- Smoke ---
    vec3 colSmoke = mix(vec3(0.1), vec3(0.4), smoke);
    vec3 finalColor = mix(colFire, colSmoke, heightMask);

    // --- Add sparks ---
    float sparkIntensity = sparks(uv, t);
    finalColor += vec3(1.0, 0.8, 0.3) * sparkIntensity;

    // --- Subtle blur ---
    vec3 blurred = vec3(0.0);
    float step = 0.0015;
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            vec2 offset = vec2(float(i), float(j)) * step;
            blurred += texture(noiseTex, vec3(uv + offset, t * 0.1)).rgb;
        }
    }
    blurred /= 9.0;
    finalColor = mix(finalColor, blurred, 0.08); // 8% blur

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

/**
 * @brief Компилирует и линкует вершинный и фрагментный шейдеры.
 *
 * Возвращает ID готовой шейдерной программы.
 * Обработка ошибок компиляции осуществляется через stderr.
 *
 * @return GLuint — ID шейдерной программы
 */
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
    // FPS counter
    double lastTime = glfwGetTime();
    int frameCount = 0;
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(800, 600, "Fire & Smoke (Interactive)", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwTerminate();
        return -1;
    }

    // Fullscreen quad
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

    GLuint noiseTex = create3DNoiseTexture(256, 0.04f); // ↑ resolution, ↓ frequency for smoother fbm
    GLuint shader = createShaderProgram();

    // Uniform locations (cache them!)
    int timeLoc = glGetUniformLocation(shader, "time");
    int colorModeLoc = glGetUniformLocation(shader, "colorMode");

    // Interactive state
    bool paused = false;
    float baseTime = 0.0f;
    float speed = 1.0f;        // fire animation speed multiplier
    int colorMode = 0;         // 0 = classic fire, 1 = lava, 2 = blue flame
    // В начале main(), до цикла while:
    bool keySpacePressed = false;
    bool keyCPressed = false;
    bool keyRPressed = false;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        // --- Interactivity ---
        // Space: pause toggle
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            if (!keySpacePressed) {
                paused = !paused;
                if (paused) baseTime = (float)glfwGetTime();
                keySpacePressed = true;
            }
        }
        else {
            keySpacePressed = false;
        }

        // +/-: speed control
        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS) {
            speed = std::min(speed + 0.1f, 3.0f);
        }
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) {
            speed = std::max(speed - 0.1f, 0.1f);
        }

        // C: color mode toggle
        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
            if (!keyCPressed) {
                colorMode = (colorMode + 1) % 3;
                keyCPressed = true;
            }
        }
        else {
            keyCPressed = false;
        }

        // R: reset
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            if (!keyRPressed) {
                speed = 1.0f;
                colorMode = 0;
                paused = false;
                keyRPressed = true;
            }
        }
        else {
            keyRPressed = false;
        }

        // --- Render ---
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shader);

        float time = paused ? baseTime : (float)glfwGetTime();
        glUniform1f(timeLoc, time * speed);
        glUniform1i(colorModeLoc, colorMode);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, noiseTex);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // FPS counter update
        frameCount++;
        double currentTime = glfwGetTime();
        double deltaTime = currentTime - lastTime;
        if (deltaTime >= 0.5) { // Update every 0.5 seconds
            double fps = frameCount / deltaTime;
            std::string title = "Fire & Smoke (Interactive) | FPS: " + std::to_string(int(fps));
            glfwSetWindowTitle(window, title.c_str());
            frameCount = 0;
            lastTime = currentTime;
        }

        glfwSwapBuffers(window);
    }

    // Cleanup
    glDeleteTextures(1, &noiseTex);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(shader);
    glfwTerminate();
    return 0;
}