//-----------------------------------------------------------------------------
// File: GameSceneAttachmentBinder.h
//-----------------------------------------------------------------------------
#pragma once

#include "stdafx.h"
#include "GameScene.h"
#include "GameSceneContentCatalog.h"

#include <array>
#include <vector>

class CGameObject;
class CFollowTransformComponent;

namespace GameSceneAttachmentBinder
{
	struct LinkInput
	{
		CFollowTransformComponent* playerSpotFollower = nullptr;
		CGameObject* preferredPlayer = nullptr;

		const std::array<CGameObject*, 4>* playersBySlot = nullptr;
		UINT playerCount = 0;

		const std::vector<CGameObject*>* playerSwordRefs = nullptr;
		const std::vector<CGameObject*>* playerBowRefs = nullptr;
		const std::vector<CGameObject*>* playerAxeRefs = nullptr;
		const std::vector<CGameObject*>* playerGunRefs = nullptr;

		const std::vector<CGameObject*>* enemySwordRefs = nullptr;
		const std::vector<CGameObject*>* enemyBowRefs = nullptr;

		const std::vector<CGameObject*>* swordManRefs = nullptr;
		const std::vector<CGameObject*>* bowManRefs = nullptr;
		const std::vector<CGameObject*>* mutantRefs = nullptr;
		const std::vector<CGameObject*>* helmetRefs = nullptr;

		bool applyOfflineTestLoadout = false;
	};

	void LinkSceneObjects(
		const LinkInput& input,
		std::vector<AttachmentBindSpec>& outAttachmentBinds
	);
}