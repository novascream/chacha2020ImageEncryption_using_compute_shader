#include<filesystem>
class image_loader
{
public:
	typedef struct Image
	{

		int height;
		int width;
		int channels;
		int buffer_size;
		size_t offset;
	}Image;
	std::vector<Image> img_Meta;
	std::vector<uint8_t> buffer;
	int padded = 0;
	bool path_wrong = false;
	image_loader(const char* path)
	{
		namespace fs = std::filesystem;
		size_t final_size = 0;
		size_t offset = 0;
		std::error_code ec;
		auto it = fs::directory_iterator(path,ec);
		if (ec)
		{
			path_wrong = true;
			return;
		}
		for (const auto& input : fs::directory_iterator(path))
		{
			std::string temp_img_path = input.path().string();
			int h, w, c;
			unsigned char* img = stbi_load(temp_img_path.c_str(), &w, &h, &c, 0);
			if (!img)
			{
				continue;
			}
			int size = w * h * c;
			img_Meta.push_back({ h,w,c,size,offset});
			offset += size;
			final_size += size;
			buffer.insert(buffer.end(), img, img+size);
			stbi_image_free(img);
		}
		padded = ((final_size + 127) / 128) * 128;
		buffer.resize(padded, 0);
	}
};