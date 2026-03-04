#pragma once
#include "Component.h"
#include <vector>
#include <unordered_map>
#include <string>

class CColliderComponent;

class CCollisionSystem final : public CComponentT<CCollisionSystem>
{
public:
    explicit CCollisionSystem(CGameObject* owner);

    void OnUpdate(float dt) override;

    // Scene/오브젝트 생성/삭제 시 호출해주면 됨
    void RegisterCollider(CColliderComponent* c);
    void UnregisterCollider(CColliderComponent* c);

private:
    bool PassFilter(const CColliderComponent* a, const CColliderComponent* b) const;
    void HandlePair(CColliderComponent* a, CColliderComponent* b);

private:
    std::vector<CColliderComponent*> mColliders;
};