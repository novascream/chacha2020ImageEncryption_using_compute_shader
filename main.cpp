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
#include<filesystem>
bool file_err1 = false;
bool file_err2 = false;
bool path_wrong_imag = false;
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
void encrypt(const char* path,int dir,int &global_index)
{
    image_loader loaded_images = image_loader(path);
    if (loaded_images.path_wrong)
    {
        path_wrong_imag = true;
    }
    unsigned char key[32], nonce[12];
    RAND_bytes(key, 32);
    RAND_bytes(nonce, 12);
    save_key_nounce(("./key_files/"+ std::to_string(dir) +"_key.bin").c_str(), key, nonce);
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
    std::ofstream Encrypted_file("./en_files/"+ std::to_string(dir) + ".cb", std::ios::binary);
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
        std::string filename = "./encrypted/" + std::to_string(global_index++) + "_encrypted.png";
        stbi_write_png(filename.c_str(), loaded_images.img_Meta[i].width, loaded_images.img_Meta[i].height, loaded_images.img_Meta[i].channels, encrypted.data(), loaded_images.img_Meta[i].width * loaded_images.img_Meta[i].channels);

    }

    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glDeleteBuffers(1, &ssbo);
}
void decrypt(const char* path,const char* key_path,int &global_index)
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
    if (!file)
    {
        std::cout << "brr";
    }
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
        std::string filename = "./decrypted/" + std::to_string(global_index++) + "_dencrypted.png";
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
int split_imgs_to_dirs(const char* path,int chunk_size)
{
    namespace fs = std::filesystem;
    //create dirs number of directories and move chunk_size of images to each one till eod
    std::vector<fs::path> files;
    for (const auto& input : fs::directory_iterator(path))
    {
        files.push_back(input.path());
    }
    size_t count = files.size();
    int dirs = std::ceil(count / chunk_size);
    for (int i = 0;i < dirs;i++)
    {
        fs::create_directory("f_"+std::to_string(i));
    }
    for (int i = 0;i < files.size();i++)
    {
        int index = i / chunk_size;
        fs::path dest = "f_" + std::to_string(index) + "/" + files[i].filename().string();
        fs::rename(files[i],dest);
    }
    std::cout << dirs;
    return dirs;
}
//FOR IMAGE loading to ui
unsigned int tex2 = 0;
unsigned int tex1 = 0;
int main() {
    namespace fs = std::filesystem;
    int dirs = 0;

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
        if (path_wrong_imag)
        {
            ImGui::Text("Path Entered for images is invalid");
        }
        ImGui::End();
        static char encrypt_path[256] = "";
        static char decrypt_path[256] = "";
        static char key_path[256] = "";
        static int chunk_size = 0;
        ImGui::InputText("Encrypt Path", encrypt_path, 256);
        ImGui::InputInt("Enter chunk_size", &chunk_size);
        if (ImGui::Button("Encrypt"))
        {
            
            //std::vector<std::string> folders = split_images(encrypt_path);
            dirs = split_imgs_to_dirs(encrypt_path, chunk_size);
            fs::create_directory("en_files");
            fs::create_directory("key_files");
            int gl = 0;
            for (int i = 0;i < dirs;i++) {
                std::string dir = "f_" + std::to_string(i);
                encrypt(dir.c_str(), i,gl);
            }
        }

        ImGui::Separator();
        ImGui::Begin("Decrypt");
        ImGui::InputText("Decrypt folder Path", decrypt_path, 256);
        ImGui::InputText("Keys Path", key_path, 256);
        if (ImGui::Button("Decrypt"))
        {
            std::cout << "out";

            std::string temp_d = decrypt_path;
            std::string temp_k = key_path;
            int gl = 0;

            for (int i = 0; i < std::distance(
                fs::directory_iterator(decrypt_path),
                fs::directory_iterator{});i++)
            {
                std::string file_path = temp_d + "/" + std::to_string(i) + ".cb";
                std::string key_file = temp_k + "/" + std::to_string(i) + "_key.bin";
                decrypt(file_path.c_str(), key_file.c_str(),gl);
            }
        }
        ImGui::End();
        
        ImGui::Begin("Notes:");
        ImGui::Text("1>Make sure your system has opengl support as this software uses opengl 4.3+ compute shader for computational speed.");
        ImGui::Text("2>This software uses chacha20 enryption algorithm to enrypt images");
        ImGui::Text("3>Make sure the dirctory is filled with only  images as the current setup uses stbi_load at its core to load images into a buffer.");
        ImGui::End();
        
        if (tex1 == 0)
        {
            int w, h, c;
            unsigned char* data_p = stbi_load("before.jpg", &w, &h, &c, 0);
            glGenTextures(1, &tex1);
            glBindTexture(GL_TEXTURE_2D, tex1);
            GLenum format = (c == 4) ? GL_RGBA : GL_RGB;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data_p);
            stbi_image_free(data_p);
            data_p = stbi_load("after.png", &w, &h, &c, 0);
            glGenTextures(1, &tex2);
            glBindTexture(GL_TEXTURE_2D, tex2);
            format = (c == 4) ? GL_RGBA : GL_RGB;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data_p);
            stbi_image_free(data_p);
        }
        ImGui::Begin("Example");
        ImGui::Text("Before encryption in buffer:");
        ImGui::Image((void *)(intptr_t)tex1,ImVec2(200,200));
        ImGui::Text("After  encryption in buffer:");
        ImGui::Image((void*)(intptr_t)tex2, ImVec2(200, 200));
        ImGui::End();
        ImGui::Render();
        glClearColor(1.0f, 1.0f, 1.0f, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
}