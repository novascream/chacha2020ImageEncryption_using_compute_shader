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
#include "./depdencies/include/imgui/imfilebrowser.h"
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
bool out_dir_err = false;

void window_resize_callback(GLFWwindow* window_p, int width, int height)
{
	glViewport(0, 0, width, height);
}

void save_key_nounce(const char* path, uint8_t* key, uint8_t* nounce)
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

//builds batches of [start,end) index pairs that fit within vram_budget bytes
//uses stbi_info to read dimensions without decoding - no RAM blown
std::vector<std::pair<int, int>> make_batches(const std::vector<std::filesystem::path>& files, size_t vram_budget)
{
	std::vector<std::pair<int, int>> batches;
	int start = 0;
	size_t running = 0;

	for (int i = 0; i < (int)files.size(); i++)
	{
		int w, h, c;
		//stbi_info only reads header - doesnt decode the image
		if (!stbi_info(files[i].string().c_str(), &w, &h, &c))
			continue;

		size_t decoded = (size_t)w * h * c;

		//if adding this image would exceed budget and we already have some, flush the batch
		if (running + decoded > vram_budget && i > start)
		{
			batches.push_back({ start, i });
			start = i;
			running = decoded;
		}
		else
		{
			running += decoded;
		}
	}
	//last batch
	if (start < (int)files.size())
		batches.push_back({ start, (int)files.size() });

	return batches;
}

//encrypt a window [start,end) from the pre-built file list
//batch_index is used for naming .cb and key files
void encrypt(
	const std::vector<std::filesystem::path>& files,
	int start, int end,
	const std::string& en_out_dir,
	const std::string& key_out_dir,
	int batch_index,
	int& global_index)
{
	image_loader loaded_images(files, start, end);
	if (loaded_images.img_Meta.empty())
		return;

	unsigned char key[32], nonce[12];
	RAND_bytes(key, 32);
	RAND_bytes(nonce, 12);
	save_key_nounce((key_out_dir + "/" + std::to_string(batch_index) + "_key.bin").c_str(), key, nonce);

	uint32_t k[8], n[3];
	for (int i = 0; i < 8; i++)
	{
		memcpy(&k[i], key + i * 4, 4);
	}
	for (int i = 0; i < 3; i++)
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

	//fixed: cast to float before dividing so ceil actually does something
	glDispatchCompute((GLuint)std::ceil((float)loaded_images.padded / 256.0f), 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	uint32_t* ptr = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	uint8_t* bytePtr = (uint8_t*)ptr;

	//write .cb file: [count][metadata array][encrypted buffer]
	std::ofstream Encrypted_file(en_out_dir + "/" + std::to_string(batch_index) + ".cb", std::ios::binary);
	uint32_t count = (uint32_t)loaded_images.img_Meta.size();
	Encrypted_file.write((char*)&count, sizeof(count));
	Encrypted_file.write((char*)loaded_images.img_Meta.data(), loaded_images.img_Meta.size() * sizeof(image_loader::Image));
	Encrypted_file.write((char*)bytePtr, loaded_images.padded);
	Encrypted_file.close();

	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glDeleteBuffers(1, &ssbo);

	global_index += (end - start);
}

void decrypt(
	const char* path,
	const char* key_path,
	const std::string& dec_out_dir,
	int& global_index)
{
	uint8_t key[32];
	uint8_t nounce[12];
	load_key_nounce(key_path, key, nounce);
	if (file_err2)
		return;

	uint32_t k[8], n[3];
	for (int i = 0; i < 8; i++)
	{
		memcpy(&k[i], key + i * 4, 4);
	}
	for (int i = 0; i < 3; i++)
	{
		memcpy(&n[i], nounce + i * 4, 4);
	}

	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		std::cout << "brr";
		return;
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

	std::vector<uint8_t> buffer(total);
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

	//fixed: same integer division bug fix as encrypt
	glDispatchCompute((GLuint)std::ceil((float)total / 256.0f), 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	uint32_t* ptr = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	uint8_t* bytePtr = (uint8_t*)ptr;

	for (int i = 0; i < (int)meta.size(); i++)
	{
		int size = meta[i].buffer_size;
		std::vector<unsigned char> decrypted(size);
		memcpy(decrypted.data(), bytePtr + meta[i].offset, size);
		std::string filename = dec_out_dir + "/" + std::to_string(global_index++) + "_decrypted.png";
		stbi_write_png(filename.c_str(), meta[i].width, meta[i].height, meta[i].channels, decrypted.data(), meta[i].width * meta[i].channels);
	}

	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glDeleteBuffers(1, &ssbo);
}

//FOR IMAGE loading to ui

int main()
{
	namespace fs = std::filesystem;

	glfwInit();
	GLFWwindow* window = glfwCreateWindow(700, 600, "Compute", NULL, NULL);
	glfwMakeContextCurrent(window);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		function_pointers = true;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsLight();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 430");

	//state for the batching system
	static std::vector<std::filesystem::path> all_files;
	static std::vector<std::pair<int, int>> batches;
	static int current_batch = 0;
	static int total_batches = 0;
	static bool encrypting = false;
	static int global_enc_index = 0;

	//one browser instance per picker - all set to directory mode
	//active_target points to whichever char buffer the user clicked Browse on
	ImGui::FileBrowser folder_picker(ImGuiFileBrowserFlags_SelectDirectory | ImGuiFileBrowserFlags_CreateNewDir);
	static char* active_target = nullptr;

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Logs");
		if (file_err1)
			ImGui::Text("Saving key failed");
		if (file_err2)
			ImGui::Text("Opening key failed");
		if (function_pointers)
			ImGui::Text("Loading Function Pointers from driver failed");
		if (path_wrong_imag)
			ImGui::Text("Path Entered for images is invalid");
		if (out_dir_err)
			ImGui::Text("Output directory does not exist or could not be created");
		ImGui::End();

		static char encrypt_path[256] = "";
		static char en_out_path[256] = "";
		static char key_out_path[256] = "";
		static char decrypt_path[256] = "";
		static char key_in_path[256] = "";
		static char dec_out_path[256] = "";
		//user enters their VRAM in MB - we convert to bytes internally
		static int vram_mb = 2048;

		ImGui::Begin("Encrypt");

		ImGui::InputText("Images Folder", encrypt_path, 256);
		ImGui::SameLine();
		if (ImGui::Button("Browse##enc_src"))
		{
			//record which buffer to write into, then open
			active_target = encrypt_path;
			folder_picker.Open();
		}

		ImGui::InputText("Encrypted Files Output", en_out_path, 256);
		ImGui::SameLine();
		if (ImGui::Button("Browse##enc_out"))
		{
			active_target = en_out_path;
			folder_picker.Open();
		}

		ImGui::InputText("Key Files Output", key_out_path, 256);
		ImGui::SameLine();
		if (ImGui::Button("Browse##key_out"))
		{
			active_target = key_out_path;
			folder_picker.Open();
		}

		ImGui::InputInt("Your VRAM (MB)", &vram_mb);
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Enter your GPU VRAM in MB.\nCheck GPU specs or Task Manager > Performance > GPU.(fast fetch for linux)");

		if (ImGui::Button("Encrypt") && !encrypting)
		{
			file_err1 = false;
			path_wrong_imag = false;
			out_dir_err = false;

			//validate + create output dirs
			std::error_code ec;
			fs::create_directories(en_out_path, ec);
			fs::create_directories(key_out_path, ec);
			if (ec)
			{
				out_dir_err = true;
			}
			else
			{
				//collect all files from the source folder
				all_files.clear();
				std::error_code ec2;
				for (const auto& entry : fs::directory_iterator(encrypt_path, ec2))
					all_files.push_back(entry.path());

				if (ec2)
				{
					path_wrong_imag = true;
				}
				else
				{
					//build batch windows from VRAM budget
					size_t vram_budget = (size_t)vram_mb * 1024 * 1024;
					batches = make_batches(all_files, vram_budget);
					total_batches = (int)batches.size();
					current_batch = 0;
					global_enc_index = 0;
					encrypting = true;
				}
			}
		}

		//process one batch per frame so the UI doesnt freeze
		if (encrypting && current_batch < total_batches)
		{
			auto [start, end] = batches[current_batch];
			encrypt(all_files, start, end, en_out_path, key_out_path, current_batch, global_enc_index);
			current_batch++;
			if (current_batch >= total_batches)
				encrypting = false;
		}

		//progress bar - only shown while encrypting
		if (encrypting || total_batches > 0)
		{
			float progress = total_batches > 0 ? (float)current_batch / (float)total_batches : 0.0f;
			ImGui::ProgressBar(progress, ImVec2(-1, 0));
			ImGui::Text("Batch %d / %d  |  Images scanned: %d", current_batch, total_batches, global_enc_index);
		}

		ImGui::End();

		ImGui::Begin("Decrypt");

		ImGui::InputText("Encrypted Folder (.cb files)", decrypt_path, 256);
		ImGui::SameLine();
		if (ImGui::Button("Browse##dec_src"))
		{
			active_target = decrypt_path;
			folder_picker.Open();
		}

		ImGui::InputText("Keys Folder", key_in_path, 256);
		ImGui::SameLine();
		if (ImGui::Button("Browse##key_in"))
		{
			active_target = key_in_path;
			folder_picker.Open();
		}

		ImGui::InputText("Decrypted Output Folder", dec_out_path, 256);
		ImGui::SameLine();
		if (ImGui::Button("Browse##dec_out"))
		{
			active_target = dec_out_path;
			folder_picker.Open();
		}

		if (ImGui::Button("Decrypt"))
		{
			file_err2 = false;
			std::error_code ec;
			fs::create_directories(dec_out_path, ec);

			std::string temp_d = decrypt_path;
			std::string temp_k = key_in_path;
			int gl = 0;
			int i = 0;

			//iterate .cb files by index - same naming convention as encrypt
			for (;; i++)
			{
				std::string file_path = temp_d + "/" + std::to_string(i) + ".cb";
				std::string key_file = temp_k + "/" + std::to_string(i) + "_key.bin";
				if (!fs::exists(file_path) || !fs::exists(key_file))
					break;
				decrypt(file_path.c_str(), key_file.c_str(), dec_out_path, gl);
			}
		}

		ImGui::End();

		ImGui::Begin("Notes:");
		ImGui::Text("1>Make sure your system has opengl support as this software uses opengl 4.3+ compute shader for computational speed.");
		ImGui::Text("2>This software uses chacha20 encryption algorithm to encrypt images");
		ImGui::Text("3>Make sure the directory is filled with only images as the current setup uses stbi_load at its core to load images into a buffer.");
		ImGui::Text("4>VRAM budget controls how many images are packed per GPU batch - set it to your GPU's VRAM in MB.");
		ImGui::End();


		//tick the browser every frame - it renders itself when open
		//HasSelected fires exactly once after user hits OK
		folder_picker.Display();
		if (folder_picker.HasSelected())
		{
			if (active_target)
				strncpy(active_target, folder_picker.GetSelected().string().c_str(), 255);
			active_target = nullptr;
			folder_picker.ClearSelected();
		}

		ImGui::Render();
		glClearColor(1.0f, 1.0f, 1.0f, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);
	}
}
