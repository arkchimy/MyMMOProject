#pragma once
#include <string>
#include <unordered_map>

namespace render
{
	class Image;
}
namespace render
{
	class ImageManager
	{
	private:
		ImageManager() = default;
	public:
		~ImageManager();
		static ImageManager* const GetInstance()
		{
			static ImageManager sInstance;
			return &sInstance;
		}
		const Image* Load(const std::string& filname);
		void Release(const std::string& filname);
	private:

		std::unordered_map<std::string, Image*> mCache;
		std::unordered_map<std::string, int> mRefcnt;

	};

}
