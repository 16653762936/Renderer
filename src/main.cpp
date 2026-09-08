// glad 必须在 glfw 之前：它负责声明现代 OpenGL 函数。
// GLFW_INCLUDE_NONE = 别让 GLFW 再带一份旧的 gl.h，两份会打架。
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdio>

// 轨道相机：围着一个目标点转。yaw/pitch 是球坐标，distance 是离目标多远。
struct Camera
{
    float yaw;
    float pitch;
    float distance;
    glm::vec3 target;
    bool dragging;
    double lastMouseX;
    double lastMouseY;
    int framebufferW;
    int framebufferH;
};

static Camera* cameraFromWindow(GLFWwindow* window)
{
    return (Camera*)glfwGetWindowUserPointer(window); // 取出我们挂在窗口上的 Camera 指针
}

static glm::vec3 cameraEye(const Camera* cam)
{
    // 球坐标：yaw 左右转，pitch 上下转。得到的是「目标点周围」的相机位置。
    const float cp = glm::cos(cam->pitch);
    return cam->target + cam->distance * glm::vec3(
        cp * glm::sin(cam->yaw),
        glm::sin(cam->pitch),
        cp * glm::cos(cam->yaw));
}

static void onMouseButton(GLFWwindow* window, int button, int action, int /*mods*/)
{
    Camera* cam = cameraFromWindow(window);
    if (button != GLFW_MOUSE_BUTTON_LEFT)
        return;
    if (action == GLFW_PRESS)
    {
        cam->dragging = true;
        glfwGetCursorPos(window, &cam->lastMouseX, &cam->lastMouseY); // 记下按下时的鼠标位置
    }
    else
    {
        cam->dragging = false;
    }
}

static void onCursorPos(GLFWwindow* window, double x, double y)
{
    Camera* cam = cameraFromWindow(window);
    if (!cam->dragging)
        return;

    const float dx = (float)(x - cam->lastMouseX);
    const float dy = (float)(y - cam->lastMouseY);
    cam->lastMouseX = x;
    cam->lastMouseY = y;

    const float sensitivity = 0.005f; // 鼠标灵敏度
    cam->yaw += dx * sensitivity;
    cam->pitch -= dy * sensitivity; // 鼠标往上拖，相机抬头，pitch 减小更直观

    // 夹住俯仰角，避免翻过头顶万向节死锁。
    const float limit = 1.55f; // 大约 ±89 度
    if (cam->pitch > limit)
        cam->pitch = limit;
    if (cam->pitch < -limit)
        cam->pitch = -limit;
}

static void onScroll(GLFWwindow* window, double /*dx*/, double dy)
{
    Camera* cam = cameraFromWindow(window);
    cam->distance *= (dy > 0.0) ? 0.9f : 1.1f; // 滚轮拉近 / 推远
    if (cam->distance < 1.0f)
        cam->distance = 1.0f;
    if (cam->distance > 40.0f)
        cam->distance = 40.0f;
}

static void onFramebufferSize(GLFWwindow* window, int width, int height)
{
    Camera* cam = cameraFromWindow(window);
    if (width <= 0 || height <= 0)
        return;
    cam->framebufferW = width;
    cam->framebufferH = height;
    glViewport(0, 0, width, height); // 视口：OpenGL 往窗口哪块区域画。窗口一改大小就要重设。
}

static unsigned int compileShader(unsigned int type, const char* source)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::printf("shader compile failed:\n%s\n", log);
    }
    return shader;
}

static unsigned int createShaderProgram()
{
    // MVP 还是那三块。光照要在世界空间算，所以额外传出世界坐标和世界法线。
    const char* vertexSrc = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aColor;
        layout (location = 2) in vec3 aNormal;
        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProj;
        out vec3 vColor;
        out vec3 vWorldPos;
        out vec3 vWorldN;
        void main()
        {
            vec4 world = uModel * vec4(aPos, 1.0);
            vWorldPos = world.xyz;
            // 法线是方向，不能直接乘 Model（非均匀缩放会歪）。逆转置才对。
            vWorldN = mat3(transpose(inverse(uModel))) * aNormal;
            gl_Position = uProj * uView * world;
            vColor = aColor;
        }
    )";

    // Blinn-Phong：环境光 + 漫反射 + 高光。平行光方向对整场景一样。
    const char* fragmentSrc = R"(
        #version 330 core
        in vec3 vColor;
        in vec3 vWorldPos;
        in vec3 vWorldN;
        uniform vec3 uLightDir;   // 光线前进方向（从太阳指向地面）
        uniform vec3 uLightColor;
        uniform vec3 uCameraPos;
        out vec4 FragColor;
        void main()
        {
            vec3 N = normalize(vWorldN);
            vec3 L = normalize(-uLightDir);          // 指向光源
            vec3 V = normalize(uCameraPos - vWorldPos); // 指向相机
            vec3 H = normalize(L + V);               // 半角：Blinn 用它代替反射向量

            float ndotl = max(dot(N, L), 0.0);
            float spec = pow(max(dot(N, H), 0.0), 32.0);

            vec3 ambient  = 0.12 * vColor;
            vec3 diffuse  = ndotl * vColor * uLightColor;
            vec3 specular = spec * uLightColor * 0.35;
            FragColor = vec4(ambient + diffuse + specular, 1.0);
        }
    )";

    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexSrc);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        std::printf("shader link failed:\n%s\n", log);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

int main()
{
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Renderer - Stage 5", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGL(glfwGetProcAddress))
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    Camera cam = {};
    cam.yaw = 0.6f;
    cam.pitch = 0.4f;
    cam.distance = 4.0f;
    cam.framebufferW = 1280;
    cam.framebufferH = 720;
    glfwSetWindowUserPointer(window, &cam); // 把相机挂到窗口上，鼠标回调里才能找到它
    glfwSetMouseButtonCallback(window, onMouseButton);
    glfwSetCursorPosCallback(window, onCursorPos);
    glfwSetScrollCallback(window, onScroll);
    glfwSetFramebufferSizeCallback(window, onFramebufferSize);
    glViewport(0, 0, cam.framebufferW, cam.framebufferH);

    glEnable(GL_DEPTH_TEST);

    unsigned int shaderProgram = createShaderProgram();
    const int uModelLoc = glGetUniformLocation(shaderProgram, "uModel");
    const int uViewLoc = glGetUniformLocation(shaderProgram, "uView");
    const int uProjLoc = glGetUniformLocation(shaderProgram, "uProj");
    const int uLightDirLoc = glGetUniformLocation(shaderProgram, "uLightDir");
    const int uLightColorLoc = glGetUniformLocation(shaderProgram, "uLightColor");
    const int uCameraPosLoc = glGetUniformLocation(shaderProgram, "uCameraPos");

    // 每面 4 个顶点：位置 + 颜色 + 法线。不能再共用 8 个角——
    // 一个角属于 3 个面，法线方向不同，必须拆开，否则光照会糊成圆角。
    // 步长 9 个 float：xyz rgb nxnynz
    const float vertices[] = {
        // -Z
        -0.5f, -0.5f, -0.5f,  0.85f, 0.25f, 0.25f,   0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.85f, 0.25f, 0.25f,   0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.85f, 0.25f, 0.25f,   0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.85f, 0.25f, 0.25f,   0.0f,  0.0f, -1.0f,
        // +Z
        -0.5f, -0.5f,  0.5f,  0.25f, 0.75f, 0.35f,   0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.25f, 0.75f, 0.35f,   0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.25f, 0.75f, 0.35f,   0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.25f, 0.75f, 0.35f,   0.0f,  0.0f,  1.0f,
        // -X
        -0.5f, -0.5f, -0.5f,  0.25f, 0.45f, 0.90f,  -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.25f, 0.45f, 0.90f,  -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.25f, 0.45f, 0.90f,  -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.25f, 0.45f, 0.90f,  -1.0f,  0.0f,  0.0f,
        // +X
         0.5f, -0.5f, -0.5f,  0.95f, 0.80f, 0.25f,   1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.95f, 0.80f, 0.25f,   1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.95f, 0.80f, 0.25f,   1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.95f, 0.80f, 0.25f,   1.0f,  0.0f,  0.0f,
        // -Y
        -0.5f, -0.5f, -0.5f,  0.55f, 0.35f, 0.20f,   0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.55f, 0.35f, 0.20f,   0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.55f, 0.35f, 0.20f,   0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.55f, 0.35f, 0.20f,   0.0f, -1.0f,  0.0f,
        // +Y
        -0.5f,  0.5f, -0.5f,  0.90f, 0.90f, 0.90f,   0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.90f, 0.90f, 0.90f,   0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.90f, 0.90f, 0.90f,   0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.90f, 0.90f, 0.90f,   0.0f,  1.0f,  0.0f
    };

    const unsigned int indices[] = {
        0,  1,  2,   2,  3,  0,
        4,  5,  6,   6,  7,  4,
        8,  9, 10,  10, 11,  8,
       12, 13, 14,  14, 15, 12,
       16, 17, 18,  18, 19, 16,
       20, 21, 22,  22, 23, 20
    };

    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    const int stride = 9 * (int)sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glBindVertexArray(0);

    const float clearR = 0.08f;
    const float clearG = 0.09f;
    const float clearB = 0.12f;

    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        const double now = glfwGetTime();
        const float dt = (float)(now - lastTime); // 这一帧过了多少秒，用来让 WASD 速度和帧率无关
        lastTime = now;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, 1);

        // WASD 平移观察目标（在水平面上走），相机跟着目标走，像编辑器里平移视口。
        const glm::vec3 forward(glm::sin(cam.yaw), 0.0f, glm::cos(cam.yaw));
        const glm::vec3 right(glm::cos(cam.yaw), 0.0f, -glm::sin(cam.yaw));
        const float moveSpeed = 2.5f * dt;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            cam.target -= glm::vec3(forward.x, 0.0f, forward.z) * moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            cam.target += glm::vec3(forward.x, 0.0f, forward.z) * moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            cam.target -= glm::vec3(right.x, 0.0f, right.z) * moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            cam.target += glm::vec3(right.x, 0.0f, right.z) * moveSpeed;

        glClearColor(clearR, clearG, clearB, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const glm::mat4 model(1.0f); // 单位矩阵：立方体停在原点，方便看出是相机在动
        const glm::vec3 eye = cameraEye(&cam);
        const glm::mat4 view = glm::lookAt(eye, cam.target, glm::vec3(0.0f, 1.0f, 0.0f));
        const float aspect = (float)cam.framebufferW / (float)cam.framebufferH;
        const glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(uModelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(uViewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(uProjLoc, 1, GL_FALSE, glm::value_ptr(proj));
        glUniform3f(uLightDirLoc, -0.35f, -1.0f, -0.25f); // 斜上方打下来
        glUniform3f(uLightColorLoc, 1.0f, 0.96f, 0.88f);
        glUniform3fv(uCameraPosLoc, 1, glm::value_ptr(eye));
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
