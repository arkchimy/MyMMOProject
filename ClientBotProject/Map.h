#pragma once
#include <cmath>

// 타일 1개 월드 크기 (mScale=2 적용)
constexpr int TILE_WORLD_W = 64; // MAP_WIDTH(64) 
constexpr int TILE_WORLD_H = 47;  // MAP_HEIGHT(47)  

// 섹터 설정
constexpr int SECTOR_TILE_CNT = 5;
// 섹터 하나의 실제 월드 좌표 크기
constexpr int SECTOR_WORLD_W = SECTOR_TILE_CNT * TILE_WORLD_W; // 320 
// 섹터 하나의 실제 월드 좌표 크기
constexpr int SECTOR_WORLD_H = SECTOR_TILE_CNT * TILE_WORLD_H; // 235

constexpr int SECTOR_COL_CNT = 20; // 6400
constexpr int SECTOR_ROW_CNT = 20; // 4700

namespace map
{
	struct Position
	{
		const float operator-(const Position& other)
		{
			//거리 반환
			return std::sqrt((x - other.x) * (x - other.x) + (y - other.y) * (y - other.y));
		}
		bool operator != (const Position& other)
		{
			return x != other.x || y != other.y;
		}
		float x;
		float y;
	};
}