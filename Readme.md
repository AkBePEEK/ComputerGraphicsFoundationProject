# Fire & Smoke Interactive Shader

Course project on the discipline **Computer Graphics Fundamentals**  
Student: Akhbergen Berdiyar and Lada Mulkulanova  
Technologies: **C++, OpenGL 3.3 Core, GLFW, GLAD**

---

## 🎯 Goal

Realization of the procedural effect of fire and smoke using:
- 3D Perlin noise
- Fractal Brownian Motion (FBM)
- Interactive controls and visual enhancements (sparks, blurring)

---

## 🖼️ Visual features

- Realistic fire with pulsation and vertical flow
- Smoke mixing with fire in the upper part
- Sparks at the top of the flame
- Software blur to soften transitions
- Three color modes: classic fire, lava, blue flame

---

## ⌨️ Management

| Key | Action |
|--------|--------|
| **SPACE** | Pause / resume animation |
| **+ / -** | Increase/ decrease the speed of fire |
| **C**     | Switching the color scheme |
| **R**     | Reset all settings |

FPS is displayed in the title bar of the window.

---

## 🛠 Assembly

### Requirements
- Windows 10/11
- Visual Studio 2022 **or** MinGW-w64
- OpenGL 3.3+ support (integrated graphics Intel HD 4000 and above)

### Project structure
Project/
├── src/
│ └── Project.cpp
├── Libraries/
│ ├── include/
│ │ ├── glad/
│ │ └── GLFW/
│ └── lib/
├── README.md
└── Project.sln (Visual Studio)

### Build in Visual Studio
1. Open `Project.sln`
2. Select the configuration for debugging x64**
3. Click **Build → Rebuild Solution**

---

## 📦 Dependencies

- **GLFW 3.3+** — view and input management
- **GLAD** — OpenGL loader (configured for core profile 3.3)
- All libraries included in the directory (`Libraries/`)

---

## 📈 Performance

- Noise resolution: **2563**
- Average frame rate per second: **>2000**
- Volume: ~64 MB (3D floating point graphics)

---

## 📚 Algorithms used

- **The Noise of Perlin (1985)** — Ken Perlin
- **Fractional Brownian motion (FBM)** — summation of results
- **Blurring in the screen space** - just an image inside

---

© 2025 akbep — Fundamentals of Computer Graphics