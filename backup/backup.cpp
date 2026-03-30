#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <string>

#include "./depdencies/include/custom/shader.h"
#include "./depdencies/include/custom/ComputeShader.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "./depdencies/include/image_loader/stb_image.h"

#define _CRT_SECURE_NO_WARNINGS
#include "./depdencies/include/openssl/rand.h"

typedef struct Image {
    int height;
    int width;
    int channels;
    int buffer_size;
} Image;

void window_resize_callback(GLFWwindow* window_p, int width, int height)
{
    glViewport(0, 0, width, height);
}

int main()
{
    using namespace std;

    // Init OpenGL
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window_p = glfwCreateWindow(600, 600, "Compute", NULL, NULL);
    if (window_p == NULL) {
        cout << "ERROR::WINDOW FAILED" << endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window_p);
    glfwSetFramebufferSizeCallback(window_p, window_resize_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cout << "ERROR::FUNCTION POINTERS WEREN'T LOADED" << endl;
        return -1;
    }

    // OpenSSL random
    unsigned char key[32];
    unsigned char nounce[12];

    uint32_t words_key[8];
    uint32_t words_nounce[3];

    if (RAND_bytes(key, sizeof(key)) != 1) {
        cerr << "ERROR::COULD NOT GENERATE RANDOM BYTE" << endl;
        return 1;
    }

    if (RAND_bytes(nounce, sizeof(nounce)) != 1) {
        cerr << "ERROR::COULD NOT GENERATE RANDOM BYTE" << endl;
        return 1;
    }

    for (int i = 0; i < 8; ++i) {
        memcpy(&words_key[i], &key[i * 4], 4);
    }

    for (int i = 0; i < 3; ++i) {
        memcpy(&words_nounce[i], &nounce[i * 4], 4);
    }

    // Load image
    Image img_1;
    unsigned char* data = stbi_load("./images/brickwall.jpg",&img_1.height,&img_1.width, &img_1.channels,0);

    if (data == NULL) {
        const char* buffer = stbi_failure_reason();
        cout << buffer;
        return -1;
    }

    img_1.buffer_size = img_1.height * img_1.width * img_1.channels;

    for (int i = 0; i < 10; i++) {
        cout << static_cast<unsigned int>(data[i]) << "\n";
    }

    vector<unsigned int> Debug_1;

    
    ComputeShader ChaCha("./shaders/chacha_compute.ps");
    ChaCha.use();

    ChaCha.setInt("C1", 0x61707865);
    ChaCha.setInt("C2", 0x3320646e);
    ChaCha.setInt("C3", 0x79622d32);
    ChaCha.setInt("C4", 0x6b206574);

    ChaCha.setInt("K1", words_key[0]);
    ChaCha.setInt("K2", words_key[1]);
    ChaCha.setInt("K3", words_key[2]);
    ChaCha.setInt("K4", words_key[3]);
    ChaCha.setInt("K5", words_key[4]);
    ChaCha.setInt("K6", words_key[5]);
    ChaCha.setInt("K6", words_key[6]); /
    ChaCha.setInt("K8", words_key[7]);

    ChaCha.setInt("N3", 0);
    ChaCha.setInt("N1", words_nounce[0]);
    ChaCha.setInt("N2", words_nounce[1]);
    ChaCha.setInt("N3", words_nounce[2]);

    
    unsigned int ssbo;
    glGenBuffers(1, &ssbo);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, img_1.buffer_size, data, GL_DYNAMIC_DRAW);

    glDispatchCompute(10, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);

    unsigned int* ptr;
    ptr = (unsigned int*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_WRITE);

    for (int i = 0; i < 10; i++) {
        Debug_1.push_back(ptr[i]);
    }

    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

    for (int i = 0; i < 10; i++) {
        cout << "P: " << Debug_1[i] << "\n";
    }

    // Main loop
    while (!glfwWindowShouldClose(window_p)) {
        glfwSwapBuffers(window_p);
        glfwPollEvents();
    }

    return 0;
}