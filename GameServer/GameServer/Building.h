#pragma once
#include "ServerObject.h"
#include "Protocol.pb.h"

class CBuilding : public CServerObject
{
public:
	void SetBuildingType(Protocol::BuildingType type) { _buildingType = type; }
	Protocol::BuildingType GetBuildingType() const { return _buildingType; }

private:
	Protocol::BuildingType _buildingType = Protocol::BUILDING_TYPE_NONE;
};

