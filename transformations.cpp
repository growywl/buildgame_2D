#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader_s.h>

#include <iostream>
#include <algorithm> // std::clamp

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// ---- DVD bounce + grow state (global for simplicity) ----
static glm::vec2 gPos(0.0f, 0.0f);
static glm::vec2 gVel(0.65f, 0.42f);  // NDC units per second
static float gScale = 0.35f;          // initial scale
static float gGrow = 1.12f;           // grow multiplier per collision (1.12 = +12%)
static float gMaxScale = 0.95f;       // clamp max size (reset if exceed)
static float gLastTime = 0.0f;

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL - DVD Bounce + Grow", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // build and compile our shader program
    // ------------------------------------
    Shader ourShader("5.1.transform.vs", "5.1.transform.fs");

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    float vertices[] = {
        // positions          // texture coords
         0.5f,  0.5f, 0.0f,   1.0f, 1.0f, // top right
         0.5f, -0.5f, 0.0f,   1.0f, 0.0f, // bottom right
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, // bottom left
        -0.5f,  0.5f, 0.0f,   0.0f, 1.0f  // top left
    };
    unsigned int indices[] = {
        0, 1, 3,
        1, 2, 3
    };

    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // texture coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // load and create textures
    // -------------------------
    unsigned int texture1;

    // texture 1
    // texture 1 = beach-ball.png
    glGenTextures(1, &texture1);
    glBindTexture(GL_TEXTURE_2D, texture1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);

    std::string p = FileSystem::getPath("resources/textures/beach-ball.png");
    std::cout << "Loading: " << p << std::endl;

    unsigned char* data = stbi_load(p.c_str(), &width, &height, &nrChannels, 0);
    if (data)
    {
        GLenum format = GL_RGB;
        if (nrChannels == 1) format = GL_RED;
        else if (nrChannels == 3) format = GL_RGB;
        else if (nrChannels == 4) format = GL_RGBA;

        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        std::cout << "Failed to load beach-ball.png" << std::endl;
    }
    stbi_image_free(data);


    // enable alpha blending (so png transparency works)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // tell OpenGL which texture unit each sampler belongs to
    ourShader.use();
    ourShader.setInt("texture1", 0);

    // init time
    gLastTime = (float)glfwGetTime();

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // input
        processInput(window);

        // time step
        float now = (float)glfwGetTime();
        float dt = now - gLastTime;
        gLastTime = now;

        // update position (DVD bounce)
        gPos += gVel * dt;

        // compute half extent in NDC:
        // local quad spans [-0.5,0.5] so half-size = 0.5 * scale
        float halfExtent = 0.5f * gScale;

        bool collided = false;

        // bounce X
        if (gPos.x + halfExtent > 1.0f) {
            gPos.x = 1.0f - halfExtent;
            gVel.x *= -1.0f;
            collided = true;
        }
        if (gPos.x - halfExtent < -1.0f) {
            gPos.x = -1.0f + halfExtent;
            gVel.x *= -1.0f;
            collided = true;
        }

        // bounce Y
        if (gPos.y + halfExtent > 1.0f) {
            gPos.y = 1.0f - halfExtent;
            gVel.y *= -1.0f;
            collided = true;
        }
        if (gPos.y - halfExtent < -1.0f) {
            gPos.y = -1.0f + halfExtent;
            gVel.y *= -1.0f;
            collided = true;
        }

        // grow on collision
        if (collided) {
            gScale *= gGrow;

            gScale = std::min(gScale, 2.0f); // กันตัวเลขบานปลาย (เลือกค่าเพดานสูง ๆ)

            float newHalf = 0.5f * gScale;
            gPos.x = std::clamp(gPos.x, -1.0f + newHalf,  1.0f - newHalf);
            gPos.y = std::clamp(gPos.y, -1.0f + newHalf,  1.0f - newHalf);
        }


        // render
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // bind textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture1);

        // build transform matrix
        float rot = now * 2.0f; // optional rotation animation
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, glm::vec3(gPos.x, gPos.y, 0.0f));
        transform = glm::rotate(transform, rot, glm::vec3(0.0f, 0.0f, 1.0f));
        transform = glm::scale(transform, glm::vec3(gScale, gScale, 1.0f));

        // set uniform
        ourShader.use();
        unsigned int transformLoc = glGetUniformLocation(ourShader.ID, "transform");
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(transform));

        // draw
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // optional controls
    // R: reset
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        gPos = glm::vec2(0.0f, 0.0f);
        gVel = glm::vec2(0.65f, 0.42f);
        gScale = 0.35f;
        gGrow = 1.12f;
    }

    // UP/DOWN: adjust grow factor
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) gGrow += 0.001f;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) gGrow -= 0.001f;
    gGrow = std::clamp(gGrow, 1.01f, 1.30f);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}
