#pragma once

#include <cstdint>
#include <string>
#include <vector>

class CServerTerrain
{
public:
	bool LoadFromFile(const std::string& path);
	bool IsLoaded() const { return m_loaded; }

	float SampleHeight(float worldX, float worldZ) const;
	bool Contains(float worldX, float worldZ) const;

private:
	float SampleLocalHeight(float localX, float localZ) const;
	uint8_t SamplePixel(int x, int z) const;

	static constexpr int   kTerrainSamples      = 1025;
	static constexpr float kTerrainWorldSize     = 1200.0f;
	static constexpr float kTerrainScale         = kTerrainWorldSize / float(kTerrainSamples - 1);
	static constexpr float kTerrainOriginX       = -600.0f;
	static constexpr float kTerrainOriginZ       = -200.0f;
	static constexpr float kTerrainVerticalScale = 0.0738f;
	static constexpr float kTerrainPlacementY    = -0.01f;

	bool m_loaded = false;
	std::vector<uint8_t> m_pixels;
};
