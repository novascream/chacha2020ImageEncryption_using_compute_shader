#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include<iostream>
#include<vector>
#include<cstdlib>
#include "./depdencies/include/custom/ComputeShader.h"
#define STB_IMAGE_IMPLEMENTATION
#include "./depdencies/include/image_loader/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "./depdencies/include/image_loader/stb_image_write.h"
#include "./depdencies/include/image_loader/img_loader.h"
#define _CRT_SECURE_NO_WARNINGS
#include "./depdencies/include/openssl/rand.h"
#include <iostream>
#include <iomanip>
#include <string>
typedef struct Image
{
	int height;
	int width;
	int channels;
	int buffer_size;
}Image;
void window_resize_callback(GLFWwindow* window_p, int width, int height)
{
	glViewport(0, 0, width, height);
}

int main() {

    glfwInit();
    GLFWwindow* window = glfwCreateWindow(600, 600, "Compute", NULL, NULL);
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "ERROR::FUNCTION POINTERS WEREN'T LOADED" <<std::endl;
        return -1;
    }

    //GLint64 max_ssbo;
    //glGetInteger64v(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS, &max_ssbo);
    //std::cout << "ssbo_size: " << max_ssbo;
    int w, h, c;

    image_loader loaded_images = image_loader("./images/aircraft_carrier");

    unsigned char* img = stbi_load("./images/container.jpg", &w, &h, &c, 0);
    int size = w * h * c;
    int padded = ((size + 3) / 4) * 4;
    std::vector<uint32_t> buffer(padded / 4, 0);
    memcpy(buffer.data(), img, size);

    
    unsigned char key[32], nonce[12];
    RAND_bytes(key, 32);
    RAND_bytes(nonce, 12);

    uint32_t k[8], n[3];
    for (int i = 0;i < 8;i++)
    {
        memcpy(&k[i], key + i * 4, 4);
    }
    for (int i = 0;i < 3;i++)
    {
        memcpy(&n[i], nonce + i * 4, 4);
    }
   
    GLuint ssbo;
    glGenBuffers(1, &ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, loaded_images.padded, loaded_images.buffer.data(), GL_DYNAMIC_DRAW);

   
    ComputeShader cs("./shaders/chacha_compute.ps");
    cs.use();

    
    cs.setInt("C1", 0x61707865);
    cs.setInt("C2", 0x3320646e);
    cs.setInt("C3", 0x79622d32);
    cs.setInt("C4", 0x6b206574);
    cs.setInt("K1", k[0]); cs.setInt("K2", k[1]);
    cs.setInt("K3", k[2]); cs.setInt("K4", k[3]);
    cs.setInt("K5", k[4]); cs.setInt("K6", k[5]);
    cs.setInt("K7", k[6]); cs.setInt("K8", k[7]);
    cs.setInt("N1", n[0]);
    cs.setInt("N2", n[1]);
    cs.setInt("N3", n[2]);

    int blocks = (padded + 63) / 64;


    //CYPHER
    
    //std::cout<<"blocks: " << loaded_images.padded / 256;
    glDispatchCompute(std::ceil(loaded_images.padded/256), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    
    uint32_t* ptr = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    uint8_t* bytePtr = (uint8_t*)ptr;
   
    for (int i = 0;i < loaded_images.img_Meta.size();i++)
    {
        int size = loaded_images.img_Meta[i].buffer_size;
        std::vector<unsigned char> encrypted(size);
        memcpy(encrypted.data(), bytePtr + loaded_images.img_Meta[i].offset, size);
        std::string num = std::to_string(i);
        std::string filename = std::to_string(i) + "_encrypted.png";
        stbi_write_png( filename.c_str(), loaded_images.img_Meta[i].width, loaded_images.img_Meta[i].height, loaded_images.img_Meta[i].channels, encrypted.data(), loaded_images.img_Meta[i].width * loaded_images.img_Meta[i].channels);
        
    }
    std::cout << "debug2";
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    //std::vector<unsigned char> encrypted(size);
    //memcpy(encrypted.data(), ptr, size);
    //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    //stbi_write_png("encrypted.png", w, h, c, encrypted.data(), w * c);

    // DECYPHER
    std::cout << "debug1";
    glDispatchCompute(std::ceil(loaded_images.padded / 256), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    ptr = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    bytePtr = (uint8_t*)ptr;
    for (int i = 0;i < loaded_images.img_Meta.size();i++)
    {
        int size = loaded_images.img_Meta[i].buffer_size;
        std::vector<unsigned char> encrypted(size);
        memcpy(encrypted.data(), bytePtr + loaded_images.img_Meta[i].offset, size);
        std::string num = std::to_string(i);
        std::string filename = std::to_string(i) + "_decrypted.png";
        stbi_write_png(filename.c_str(), loaded_images.img_Meta[i].width, loaded_images.img_Meta[i].height, loaded_images.img_Meta[i].channels, encrypted.data(), loaded_images.img_Meta[i].width * loaded_images.img_Meta[i].channels);
    }
    //std::vector<unsigned char> decrypted(size);
    //memcpy(decrypted.data(), ptr, size);
    //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    //stbi_write_png("decrypted.png", w, h, c, decrypted.data(), w * c);
    //stbi_image_free(img);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
}