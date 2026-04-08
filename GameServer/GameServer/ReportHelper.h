#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <cctype>
#include <cstdlib>
#include <cstdio>

#include "Protocol.pb.h"
#include "ColliderComponent.h"

struct ReportObjectOOBB
{
	int placementIndex = -1;
	std::string assetName;
	std::string objectName;
	BoundingOrientedBox localOOBB{};
	std::vector<BoundingOrientedBox> localSubOOBBs;
};

struct StaticWorldReportCache
{
	std::unordered_map<int, ReportObjectOOBB> byBuildingType;
};

namespace ReportHelper
{
   namespace Detail
	{
		inline std::string TrimString(const std::string& text)
		{
			size_t begin = 0;
			while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])))
				++begin;

			size_t end = text.size();
			while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
				--end;

			return text.substr(begin, end - begin);
		}

		inline bool TrySplitKeyValue(const std::string& line, std::string& outKey, std::string& outValue)
		{
			const size_t sep = line.find(':');
			if (sep == std::string::npos)
				return false;

			outKey = TrimString(line.substr(0, sep));
			outValue = TrimString(line.substr(sep + 1));
			return !outKey.empty();
		}

		inline bool ParseVector3Tuple(const std::string& text, XMFLOAT3& outValue)
		{
			float x = 0.0f;
			float y = 0.0f;
			float z = 0.0f;
			if (::sscanf_s(text.c_str(), "(%f, %f, %f)", &x, &y, &z) != 3)
				return false;

			outValue = XMFLOAT3(x, y, z);
			return true;
		}

		inline bool ParseVector4Tuple(const std::string& text, XMFLOAT4& outValue)
		{
			float x = 0.0f;
			float y = 0.0f;
			float z = 0.0f;
			float w = 1.0f;
			if (::sscanf_s(text.c_str(), "(%f, %f, %f, %f)", &x, &y, &z, &w) != 4)
				return false;

			outValue = XMFLOAT4(x, y, z, w);
			return true;
		}
	}

	inline Protocol::BuildingType AssetToBuildingType(const std::string& asset)
	{
		if (asset == "Grass") return Protocol::BUILDING_TYPE_GRASS;
		if (asset == "Ground") return Protocol::BUILDING_TYPE_GROUND;
		if (asset == "Building1") return Protocol::BUILDING_TYPE_BUILDING1;
		if (asset == "Building2") return Protocol::BUILDING_TYPE_BUILDING2;
		if (asset == "Building3") return Protocol::BUILDING_TYPE_BUILDING3;
		if (asset == "Building4") return Protocol::BUILDING_TYPE_BUILDING4;
		if (asset == "Building5") return Protocol::BUILDING_TYPE_BUILDING5;
		if (asset == "Building6") return Protocol::BUILDING_TYPE_BUILDING6;
		if (asset == "Building7") return Protocol::BUILDING_TYPE_BUILDING7;
		if (asset == "Building8") return Protocol::BUILDING_TYPE_BUILDING8;
		if (asset == "Building9") return Protocol::BUILDING_TYPE_BUILDING9;
		if (asset == "VillageWall") return Protocol::BUILDING_TYPE_VILLAGE_WALL;
		if (asset == "DirtRoad") return Protocol::BUILDING_TYPE_DIRT_ROAD;
		if (asset == "Tower") return Protocol::BUILDING_TYPE_TOWER;
		return Protocol::BUILDING_TYPE_NONE;
	}

	inline const char* BuildingTypeToAssetName(Protocol::BuildingType type)
	{
		switch (type)
		{
		case Protocol::BUILDING_TYPE_GRASS: return "Grass";
		case Protocol::BUILDING_TYPE_GROUND: return "Ground";
		case Protocol::BUILDING_TYPE_BUILDING1: return "Building1";
		case Protocol::BUILDING_TYPE_BUILDING2: return "Building2";
		case Protocol::BUILDING_TYPE_BUILDING3: return "Building3";
		case Protocol::BUILDING_TYPE_BUILDING4: return "Building4";
		case Protocol::BUILDING_TYPE_BUILDING5: return "Building5";
		case Protocol::BUILDING_TYPE_BUILDING6: return "Building6";
		case Protocol::BUILDING_TYPE_BUILDING7: return "Building7";
		case Protocol::BUILDING_TYPE_BUILDING8: return "Building8";
		case Protocol::BUILDING_TYPE_BUILDING9: return "Building9";
		case Protocol::BUILDING_TYPE_VILLAGE_WALL: return "VillageWall";
		case Protocol::BUILDING_TYPE_DIRT_ROAD: return "DirtRoad";
		case Protocol::BUILDING_TYPE_TOWER: return "Tower";
		default: return "";
		}
	}

	// 파일에서 리포트를 읽어 타입별 캐시를 구성
 inline bool LoadStaticWorldOverallLocalOOBBReport(
		const std::vector<std::string>& candidates,
      StaticWorldReportCache& outCache)
	{
		outCache.byBuildingType.clear();

		std::ifstream fin;
		for (const auto& path : candidates)
		{
			fin.open(path);
			if (fin.is_open())
				break;
			fin.clear();
		}

		if (!fin.is_open())
			return false;

		std::string line;
		bool inObject = false;
		bool inSubOOBB = false;

		ReportObjectOOBB current{};
		bool hasCenter = false;
		bool hasRotation = false;
		bool hasExtents = false;

		BoundingOrientedBox currentSubOOBB{};
		bool hasSubCenter = false;
		bool hasSubRotation = false;
		bool hasSubExtents = false;

		while (std::getline(fin, line))
		{
			line = Detail::TrimString(line);
			if (line.empty())
				continue;

			if (line == "ObjectBegin")
			{
				inObject = true;
				inSubOOBB = false;
				current = ReportObjectOOBB{};
				hasCenter = false;
				hasRotation = false;
				hasExtents = false;
				hasSubCenter = false;
				hasSubRotation = false;
				hasSubExtents = false;
				continue;
			}

			if (!inObject)
				continue;

			if (line == "ObjectEnd")
			{
				if (inSubOOBB && hasSubCenter && hasSubRotation && hasSubExtents)
					current.localSubOOBBs.push_back(currentSubOOBB);

				if (current.placementIndex >= 0 && hasCenter && hasRotation && hasExtents)
				{
					const Protocol::BuildingType buildingType = AssetToBuildingType(current.assetName);
					if (buildingType != Protocol::BUILDING_TYPE_NONE)
					{
						const int buildingTypeKey = static_cast<int>(buildingType);
						if (outCache.byBuildingType.find(buildingTypeKey) == outCache.byBuildingType.end())
							outCache.byBuildingType[buildingTypeKey] = current;
					}
				}

				inObject = false;
				inSubOOBB = false;
				continue;
			}

			if (line == "SubOOBBBegin")
			{
				inSubOOBB = true;
				currentSubOOBB = BoundingOrientedBox{};
				hasSubCenter = false;
				hasSubRotation = false;
				hasSubExtents = false;
				continue;
			}

			if (line == "SubOOBBEnd")
			{
				if (inSubOOBB && hasSubCenter && hasSubRotation && hasSubExtents)
					current.localSubOOBBs.push_back(currentSubOOBB);

				inSubOOBB = false;
				continue;
			}

			std::string key;
			std::string value;
			if (!Detail::TrySplitKeyValue(line, key, value))
				continue;

			if (key == "PlacementIndex")
				current.placementIndex = std::atoi(value.c_str());
			else if (key == "AssetName")
				current.assetName = value;
			else if (key == "ObjectName")
				current.objectName = value;
			else if (key == "OverallLocalOOBB.Center")
				hasCenter = Detail::ParseVector3Tuple(value, current.localOOBB.Center);
			else if (key == "OverallLocalOOBB.RotationQuat")
				hasRotation = Detail::ParseVector4Tuple(value, current.localOOBB.Orientation);
			else if (key == "OverallLocalOOBB.Extents")
				hasExtents = Detail::ParseVector3Tuple(value, current.localOOBB.Extents);
			else if (key == "SubLocalOOBB.Center")
				hasSubCenter = Detail::ParseVector3Tuple(value, currentSubOOBB.Center);
			else if (key == "SubLocalOOBB.RotationQuat")
				hasSubRotation = Detail::ParseVector4Tuple(value, currentSubOOBB.Orientation);
			else if (key == "SubLocalOOBB.Extents")
				hasSubExtents = Detail::ParseVector3Tuple(value, currentSubOOBB.Extents);
		}

		return !outCache.byBuildingType.empty();
	}

	// 타입으로 조회 (없으면 nullptr)
 inline const ReportObjectOOBB* FindByBuildingType(
		const StaticWorldReportCache& cache,
       Protocol::BuildingType type)
	{
		const auto it = cache.byBuildingType.find(static_cast<int>(type));
		if (it == cache.byBuildingType.end())
			return nullptr;

		return &it->second;
	}
}