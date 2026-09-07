#pragma once
//TileMap.h
#include <cstdint>
#include "../Render/Image.h"

struct ID3D11Buffer;

namespace map
{
	enum MapConfig
	{
		MAP_CONFIG_ROW = 100,
		MAP_CONFIG_COL = 50,
		MAP_CONFIG_TILESET = 6,
		MAP_WIDTH = 64,
		MAP_HEIGHT = 47,
		MAP_SCALE = 2,
	};
	class TileMap final
	{
	public:
		TileMap();
		~TileMap();
		TileMap(const TileMap&) = delete;
		const TileMap& operator =(const TileMap&) = delete;
		TileMap(TileMap&&) = delete;
		TileMap& operator=(TileMap&&) = delete;

		void Render(const float cameraX, const float cameraY);

	private:

		void createVertexBuffer();
		void createConstantBuffer();
		void updateConstantBuffer(int32_t row, int32_t col, const float cameraX, const float cameraY);

	private:
		render::Image mTileImg[MAP_CONFIG_TILESET];
		int32_t mTile[MAP_CONFIG_ROW][MAP_CONFIG_COL];

		ID3D11Buffer* mVertexBuffer = nullptr;
		ID3D11Buffer* mConstantBuffer = nullptr;
		float mScale;
	};

}
