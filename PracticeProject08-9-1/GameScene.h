//-----------------------------------------------------------------------------
// File: GameScene.h
//-----------------------------------------------------------------------------

#pragma once
#include "Scene.h"

// 현재 CScene 구현이 곧 GameScene이므로, 동작 변경 없이 타입만 분리한다.
class CGameScene final : public CScene
{
public:
    CGameScene() = default;
    ~CGameScene() override = default;
};