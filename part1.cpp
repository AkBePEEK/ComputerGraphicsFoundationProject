#define _USE_MATH_DEFINES
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>

// --- Shader Compilation (same as before) ---
unsigned int compileShader(const char* source, GLenum type) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);
    int success; glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(id, 512, nullptr, infoLog);
        std::cout << "Shader compile error:\n" << infoLog << std::endl;
    }
    return id;
}

unsigned int createShader(const char* vs, const char* fs) {
    unsigned int program = glCreateProgram();
    unsigned int v = compileShader(vs, GL_VERTEX_SHADER);
    unsigned int f = compileShader(fs, GL_FRAGMENT_SHADER);
    glAttachShader(program, v);
    glAttachShader(program, f);
    glLinkProgram(program);
    glDeleteShader(v); glDeleteShader(f);
    return program;
}

// --- Geometry Generators (same) ---
std::vector<float> createSquare(float gray = 0.5f) {
    return {
        -0.5f,-0.5f, gray,gray,gray,
         0.5f,-0.5f, gray,gray,gray,
         0.5f, 0.5f, gray,gray,gray,
        -0.5f,-0.5f, gray,gray,gray,
         0.5f, 0.5f, gray,gray,gray,
        -0.5f, 0.5f, gray,gray,gray
    };
}

std::vector<float> createCircle(float r = 1, float g = 1, float b = 1, int seg = 64) {
    std::vector<float> v = { 0,0, r,g,b };
    for (int i = 0; i <= seg; ++i) {
        float a = 2 * M_PI * i / seg;
        v.insert(v.end(), { cos(a), sin(a), r,g,b });
    }
    return v;
}

std::vector<float> createEllipse(float scaleY = 0.6f, float r = 1, float g = 1, float b = 1, int seg = 64) {
    std::vector<float> v = { 0,0, r,g,b };
    for (int i = 0; i <= seg; ++i) {
        float a = 2 * M_PI * i / seg;
        v.insert(v.end(), { cos(a), sin(a) * scaleY, r,g,b });
    }
    return v;
}

std::vector<float> createTriangle(float r = 1, float g = 1, float b = 1) {
    std::vector<float> v;
    for (int i = 0; i < 3; ++i) {
        float a = 2 * M_PI * i / 3 - M_PI / 2;
        v.insert(v.end(), { cos(a) * 0.4f, sin(a) * 0.4f, r,g,b });
    }
    return v;
}

struct BreathingCircle {
    float x, y, scale = 1.0f, dir = 0.005f, r, g, b;
    BreathingCircle(float x, float y) : x(x), y(y) {
        r = (float)rand() / RAND_MAX;
        g = (float)rand() / RAND_MAX;
        b = (float)rand() / RAND_MAX;
    }
};

// --- Global State ---
bool animRunning = true;
float squareAngle = 0, triangleAngle = 0, circleScale = 1.0f, circleDir = 0.005f;
float squareGray = 1.0f; // white
float win2R = 1, win2G = 1, win2B = 1;
float subBgR = 0.2f, subBgG = 0.3f, subBgB = 0.4f; // default subwindow bg
std::vector<BreathingCircle> circles;

// --- Matrix Utils (simplified) ---
void setTransformUniform(unsigned int prog, float angle = 0, float sx = 1, float sy = 1, float tx = 0, float ty = 0) {
    float c = cos(angle), s = sin(angle);
    float mat[16] = {
        sx * c, -sx * s, 0, 0,
        sy * s,  sy * c, 0, 0,
         0,     0,   1, 0,
        tx,    ty,   0, 1
    };
    glUniformMatrix4fv(glGetUniformLocation(prog, "transform"), 1, GL_FALSE, mat);
}

// --- Input Callbacks ---
void keyCallback(GLFWwindow* w, int key, int sc, int action, int mods) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_SPACE) animRunning = !animRunning;
    else if (key == GLFW_KEY_W) squareGray = 1.0f;
    else if (key == GLFW_KEY_R) squareGray = 0.5f;
    else if (key == GLFW_KEY_G) squareGray = 0.7f;
    else if (key == GLFW_KEY_1) { subBgR = 1; subBgG = 0; subBgB = 0; } // red
    else if (key == GLFW_KEY_2) { subBgR = 0; subBgG = 1; subBgB = 0; } // green
    else if (key == GLFW_KEY_3) { subBgR = 0; subBgG = 0; subBgB = 1; } // blue
    else if (key == GLFW_KEY_R) { win2R = 1; win2G = 0; win2B = 0; }
    else if (key == GLFW_KEY_G) { win2R = 0; win2G = 1; win2B = 0; }
    else if (key == GLFW_KEY_B) { win2R = 0; win2G = 0; win2B = 1; }
    else if (key == GLFW_KEY_Y) { win2R = 1; win2G = 1; win2B = 0; }
    else if (key == GLFW_KEY_O) { win2R = 1; win2G = 0.5f; win2B = 0; }
    else if (key == GLFW_KEY_P) { win2R = 0.5f; win2G = 0; win2B = 0.5f; }
    else if (key == GLFW_KEY_W) { win2R = 1; win2G = 1; win2B = 1; }
}

void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double x, y;
        glfwGetCursorPos(w, &x, &y);
        int width, height;
        glfwGetWindowSize(w, &width, &height);
        float ndcX = (2 * x / width) - 1;
        float ndcY = 1 - (2 * y / height);
        // Only spawn in main viewport (left half)
        if (ndcX < 0) circles.emplace_back(ndcX, ndcY);
    }
}

// --- Render Helpers ---
void drawSquare(unsigned int shader, float angle, float gray) {
    setTransformUniform(shader, angle);
    auto data = createSquare(gray);
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDeleteVertexArrays(1, &VAO); glDeleteBuffers(1, &VBO);
}

void drawCircle(unsigned int shader, float scale, float r, float g, float b, float tx = 0, float ty = 0) {
    setTransformUniform(shader, 0, scale, scale, tx, ty);
    auto data = createCircle(r, g, b);
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLE_FAN, 0, data.size() / 5);
    glDeleteVertexArrays(1, &VAO); glDeleteBuffers(1, &VBO);
}

void drawEllipse(unsigned int shader) {
    setTransformUniform(shader);
    auto data = createEllipse();
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLE_FAN, 0, data.size() / 5);
    glDeleteVertexArrays(1, &VAO); glDeleteBuffers(1, &VBO);
}

void drawTriangle(unsigned int shader, float angle, float r, float g, float b, float tx = 0, float ty = 0) {
    setTransformUniform(shader, angle, 1, 1, tx, ty);
    auto data = createTriangle(r, g, b);
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDeleteVertexArrays(1, &VAO); glDeleteBuffers(1, &VBO);
}

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 vPosition;
layout (location = 1) in vec3 vColor;
out vec3 ourColor;
uniform mat4 transform;
void main() {
    gl_Position = transform * vec4(vPosition, 0.0, 1.0);
    ourColor = vColor;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec3 ourColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(ourColor, 1.0);
}
)";

int main() {
    srand((unsigned)time(nullptr));
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Assignment 2 - Part 1", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD init failed!" << std::endl;
        return -1;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    unsigned int shader = createShader(vertexShaderSource, fragmentShaderSource);
    glUseProgram(shader);

    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);

    float lastTime = (float)glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime();
        float dt = now - lastTime;
        lastTime = now;

        if (animRunning) {
            squareAngle += 1.0f * dt;
            triangleAngle -= 1.0f * dt;
            circleScale += circleDir;
            if (circleScale > 1.2f || circleScale < 0.8f) circleDir *= -1;
            for (auto& c : circles) {
                c.scale += c.dir;
                if (c.scale > 1.2f || c.scale < 0.8f) c.dir *= -1;
            }
        }

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // --- Main View (Left Half) ---
        glViewport(0, 0, width / 2, height);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        drawSquare(shader, squareAngle, squareGray);
        for (const auto& c : circles) {
            drawCircle(shader, c.scale, c.r, c.g, c.b, c.x, c.y);
        }

        // --- SubWindow (Top Right) ---
        glViewport(width / 2, height / 2, width / 2, height / 2);
        glClearColor(subBgR, subBgG, subBgB, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        drawEllipse(shader);

        // --- Window 2 (Bottom Right) ---
        glViewport(width / 2, 0, width / 2, height / 2);
        glClearColor(0.1f, 0.1f, 0.1f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        drawCircle(shader, circleScale, win2R, win2G, win2B, -0.5f, 0.0f);
        drawTriangle(shader, triangleAngle, win2R, win2G, win2B, 0.5f, 0.0f);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteProgram(shader);
    glfwTerminate();
    return 0;
}