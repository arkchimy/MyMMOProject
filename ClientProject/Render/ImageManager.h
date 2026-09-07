#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

namespace render
{
	class Image;
}
namespace render
{
	class ImageManager final
	{
	private:
		ImageManager() = default;
	public:
		~ImageManager();

		ImageManager(const ImageManager&) = delete;
		ImageManager& operator=(const ImageManager&) = delete;
		ImageManager(ImageManager&&) = delete;
		ImageManager& operator=(ImageManager&&) = delete;

		static ImageManager* const GetInstance()
		{
			static ImageManager sInstance;
			return &sInstance;
		}
		const Image* Load(const std::string& filname);
		void Release(const std::string& filname);
	private:

		std::unordered_map<std::string, Image*> mCache;
		std::unordered_map<std::string, int32_t> mRefcnt;

	};

}
