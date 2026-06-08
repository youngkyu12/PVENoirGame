#include "pch.h"
#include "ServerTerrain.h"

#include <cmath>
#include <fstream>

bool CServerTerrain::LoadFromFile(const std::string& path)
{
	m_loaded = false;
	m_pixels.clear();

	std::ifstream fin(path, std::ios::binary);
	if (!fin.is_open())
		return false;

	constexpr size_t kPixelCount =
		static_cast<size_t>(kTerrainSamples) * static_cast<size_t>(kTerrainSamples);

	std::vector<uint8_t> raw(kPixelCount, 0);
	fin.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
	if (fin.gcount() != static_cast<std::streamsize>(raw.size()))
		return false;

	m_pixels.assign(kPixelCount, 0);
	for (int y = 0; y < kTerrainSamples; ++y)
	{
		for (int x = 0; x < kTerrainSamples; ++x)
		{
			const int src = x + (y * kTerrainSamples);
			const int dst = x + ((kTerrainSamples - 1 - y) * kTerrainSamples);
			m_pixels[static_cast<size_t>(dst)] = raw[static_cast<size_t>(src)];
		}
	}

	m_loaded = true;
	return true;
}

bool CServerTerrain::Contains(float worldX, float worldZ) const
{
	const float maxX = kTerrainOriginX + kTerrainWorldSize;
	const float maxZ = kTerrainOriginZ + kTerrainWorldSize;
	return worldX >= kTerrainOriginX && worldX < maxX &&
		worldZ >= kTerrainOriginZ && worldZ < maxZ;
}

float CServerTerrain::SampleHeight(float worldX, float worldZ) const
{
	if (!m_loaded || !Contains(worldX, worldZ))
		return 0.0f;

	const float localX = worldX - kTerrainOriginX;
	const float localZ = worldZ - kTerrainOriginZ;

	return kTerrainPlacementY + SampleLocalHeight(localX, localZ);
}

float CServerTerrain::SampleLocalHeight(float localX, float localZ) const
{
	const float fx = localX / kTerrainScale;
	const float fz = localZ / kTerrainScale;

	if (fx < 0.0f || fz < 0.0f || fx >= kTerrainSamples || fz >= kTerrainSamples)
		return 0.0f;

	const int x = static_cast<int>(fx);
	const int z = static_cast<int>(fz);
	if (x < 0 || z < 0 || x >= (kTerrainSamples - 1) || z >= (kTerrainSamples - 1))
		return 0.0f;

	const float fxPercent = fx - static_cast<float>(x);
	const float fzPercent = fz - static_cast<float>(z);

	const float h00 = static_cast<float>(SamplePixel(x,     z    ));
	const float h10 = static_cast<float>(SamplePixel(x + 1, z    ));
	const float h01 = static_cast<float>(SamplePixel(x,     z + 1));
	const float h11 = static_cast<float>(SamplePixel(x + 1, z + 1));

	const float bottom = h00 * (1.0f - fxPercent) + h10 * fxPercent;
	const float top    = h01 * (1.0f - fxPercent) + h11 * fxPercent;
	return (bottom * (1.0f - fzPercent) + top * fzPercent) * kTerrainVerticalScale;
}

uint8_t CServerTerrain::SamplePixel(int x, int z) const
{
	return m_pixels[static_cast<size_t>(x + (z * kTerrainSamples))];
}
