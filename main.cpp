#include <glad/glad.h>
#include <GLFW/glfw3.h>
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
#include "./depdencies/include/imgui/imgui.h"
#include "./depdencies/include/imgui/imgui_stdlib.h"
#include "./depdencies/include/imgui/backends/imgui_impl_glfw.h"
#include "./depdencies/include/imgui/backends/imgui_impl_opengl3.h"
#include <iostream>
#include <iomanip>
#include <string>
bool file_err1 = false;
bool file_err2 = false;
bool function_pointers = false;
void window_resize_callback(GLFWwindow* window_p, int width, int height)
{
	glViewport(0, 0, width, height);
}
void save_key_nounce(const char* path,uint8_t* key, uint8_t* nounce)
{
    std::ofstream file(path, std::ios::binary);
    if (!file)
    {
        file_err1 = true;
        return;
    }
    file.write((char*)key, 32);
    file.write((char*)nounce, 12);
    file.close();
}
void load_key_nounce(const char* path, uint8_t* key, uint8_t* nounce)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        file_err2 = true;
        return;
    }
    file.read((char*)key, 32);
    file.read((char*)nounce, 12);
    file.close();

}
void encrypt(const char* path)
{
    image_loader loaded_images = image_loader(path);
    unsigned char key[32], nonce[12];
    RAND_bytes(key, 32);
    RAND_bytes(nonce, 12);
    save_key_nounce("key.bin", key, nonce);
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
    glDispatchCompute(std::ceil(loaded_images.padded / 256), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    uint32_t* ptr = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    uint8_t* bytePtr = (uint8_t*)ptr;
    std::ofstream Encrypted_file("cb.sg", std::ios::binary);
    uint32_t count = loaded_images.img_Meta.size();
    Encrypted_file.write((char*)&count,sizeof(count));
    Encrypted_file.write((char*)loaded_images.img_Meta.data(), loaded_images.img_Meta.size() * sizeof(image_loader::Image));
    Encrypted_file.write((char*)bytePtr,loaded_images.padded);
    Encrypted_file.close();
    for (int i = 0;i < loaded_images.img_Meta.size();i++)
    {
        int size = loaded_images.img_Meta[i].buffer_size;
        std::vector<unsigned char> encrypted(size);
        memcpy(encrypted.data(), bytePtr + loaded_images.img_Meta[i].offset, size);
        std::string num = std::to_string(i);
        std::string filename = "./encrypted/" + std::to_string(i) + "_encrypted.png";
        stbi_write_png(filename.c_str(), loaded_images.img_Meta[i].width, loaded_images.img_Meta[i].height, loaded_images.img_Meta[i].channels, encrypted.data(), loaded_images.img_Meta[i].width * loaded_images.img_Meta[i].channels);

    }

    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glDeleteBuffers(1, &ssbo);
}
void decrypt(const char* path,const char* key_path)
{
    uint8_t key[32];
    uint8_t nounce[12];
    load_key_nounce(key_path, key, nounce);
    if (file_err2)
    {
        return;
    }
    uint32_t k[8], n[3];
    for (int i = 0;i < 8;i++)
    {
        memcpy(&k[i], key + i * 4, 4);
    }
    for (int i = 0;i < 3;i++)
    {
        memcpy(&n[i], nounce + i * 4, 4);
    }
    std::ifstream file(path, std::ios::binary);
    uint32_t count;
    file.read((char*)&count, sizeof(count));
    std::vector<image_loader::Image> meta(count);
    file.read((char*)meta.data(), count * sizeof(image_loader::Image));
    uint32_t total = 0;
    for (auto& m : meta)
    {
        total += m.buffer_size;
    }
    total = ((total + 127) / 128) * 128;
    std::vector<uint8_t> buffer(total);;
    file.read((char*)buffer.data(), total);
    file.close();
    GLuint ssbo;
    glGenBuffers(1, &ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, total, buffer.data(), GL_DYNAMIC_DRAW);
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
    glDispatchCompute(std::ceil(total / 256), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    uint32_t* ptr = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    uint8_t* bytePtr = (uint8_t*)ptr;
    for (int i = 0;i < meta.size();i++)
    {
        int size = meta[i].buffer_size;
        std::vector<unsigned char> encrypted(size);
        memcpy(encrypted.data(), bytePtr + meta[i].offset, size);
        std::string num = std::to_string(i);
        std::string filename = "./decrypted/" + std::to_string(i) + "_dencrypted.png";
        stbi_write_png(filename.c_str(), meta[i].width, meta[i].height, meta[i].channels, encrypted.data(), meta[i].width * meta[i].channels);

    }
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glDeleteBuffers(1, &ssbo);
}
GLuint  load_img(const char* path)
{
    int w, h, c;
    unsigned char* data_p = stbi_load(path, &w, &h, &c, 0);

   
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLenum format = (c == 4) ? GL_RGBA : GL_RGB;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data_p);
    stbi_image_free(data_p);
    return tex;
}
//FOR IMAGE loading to ui
unsigned int tex_before = 0;
unsigned int tex_after = 0;
int main() {

    glfwInit();
    GLFWwindow* window = glfwCreateWindow(600, 600, "Compute", NULL, NULL);
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        function_pointers = true;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsLight();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");

    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Logs");
        if (file_err1)
        {
            ImGui::Text("Saving key failed");
        }
        if (file_err2)
        {
            ImGui::Text("Openging key failed");
        }
        if (function_pointers)
        {
            ImGui::Text("Loading Function Pointers from driver failed");
        }
        ImGui::End();
        static char encrypt_path[256] = "";
        static char decrypt_path[256] = "";
        static char key_path[256] = "";

        ImGui::InputText("Encrypt Path", encrypt_path, 256);

        if (ImGui::Button("Encrypt"))
        {
            encrypt(encrypt_path);
        }

        ImGui::Separator();
        ImGui::Begin("Decrypt");
        ImGui::InputText("Decrypt Path", decrypt_path, 256);
        ImGui::InputText("Key Path", key_path, 256);
        if (ImGui::Button("Decrypt"))
        {
            decrypt(decrypt_path, key_path);
        }
        ImGui::End();
        
        ImGui::Begin("Notes:");
        ImGui::Text("1>Make sure your system has opengl support as this software uses opengl 4.3+ compute shader for computational speed.");
        ImGui::Text("2>This software uses chacha20 enryption algorithm to enrypt images");
        ImGui::Text("3>Make sure the dirctory is filled with only  images as the current setup uses stbi_load at its core to load images into a buffer.");
        ImGui::End();
        
        if (tex_before == 0)
        {
            GLuint tex_before = load_img("before.jpg");
            GLuint tex_after = load_img("after.png");
        }
        ImGui::Begin("Example");
        ImGui::Text("Before decryption in buffer:");
        ImGui::Image((void *)(intptr_t)tex_before,ImVec2(200,200));
        ImGui::End();
        ImGui::Render();
        glClearColor(1.0f, 1.0f, 1.0f, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
}