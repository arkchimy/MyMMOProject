#pragma once
#include <vector>
#include <string>
#include <unordered_map>
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11SamplerState;

namespace render
{
	class Image
	{
		friend class Animation;
	public:
		Image();
		~Image();
		void ReadFromFile(const char* filename);
		inline const char* GetFilename() const { return mFilename; }
		ID3D11ShaderResourceView* GetSRV() const { return mSRV; }
		ID3D11SamplerState* GetSampleState() const { return mSampleState; }
		inline int GetWidth() const { return mWidth; }
		inline int GetHeight() const { return mHeight; }
	private:
		int mWidth;
		int mHeight;
		const char* mFilename;

		ID3D11Texture2D* mTexture;
		ID3D11ShaderResourceView* mSRV;
		ID3D11SamplerState* mSampleState;
	};
}