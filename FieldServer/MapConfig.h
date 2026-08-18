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
// 총 400개의 섹터가 존재하고, 400명이 로그인했다면, 1섹터에 1명이 들어감. 이때 9배가 적절.
namespace map
{
	struct Sector
	{
		bool operator < (const Sector& other) const
		{
			if (x == other.x)
			{
				return y < other.y;
			}
			return x < other.x;
		}
		bool operator != (const Sector& other) const
		{
			return x != other.x || y != other.y;
		}
		bool operator == (const Sector& other) const
		{
			return x == other.x && y == other.y;
		}
		int8_t x;
		int8_t y;
	};
	struct Position
	{
		float operator-(const Position& other)
		{
			//거리 반환
			return std::sqrt((x - other.x) * (x - other.x) + (y - other.y) * (y - other.y));
		}
		float operator-( Position& other)
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