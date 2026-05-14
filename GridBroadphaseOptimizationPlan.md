# Grid Broadphase Optimization Plan

## 목적

현재 서버에는 `FineGrid`/`MegaGrid` 계열의 공간 구조가 이미 존재하지만, 주요 게임 로직은 아직 이 Grid를 실제 후보 선별에 충분히 사용하지 않는다.

이 문서는 지금까지의 코드 확인과 설계 논의를 반영한 최종 수정안이다. 컨텍스트 없이 이 문서만 읽어도 작업을 시작할 수 있도록, 적용 대상과 우선순위를 구체화한다.

빌드 또는 런타임 검증 없이 정적 분석 기준으로 작성했다. 이 프로젝트 지침상 명시 허가 없이 빌드하지 않는다.

## 최종 방향

Grid의 역할을 둘로 나눈다.

```text
FineGrid:
  충돌 broadphase 최적화
  특히 world static collision 후보 축소

MegaGrid:
  패킷 작성 시 enemy interest 후보 추출
  player 기준 반경 200과 겹치는 MegaGrid의 enemy id만 후보로 사용
```

객체 소유권은 계속 `Room`이 가진다.

```cpp
map<uint64, PlayerRef> players;
map<uint64, EnemyRef> enemies;
map<uint64, BuildingRef> buildings;
Vector<ProjectileRef> m_arrowPool;
Vector<ProjectileRef> m_bulletPool;
```

Grid/MegaGrid는 실제 object를 소유하지 않는다. cell에는 object pointer 소유권을 두지 않고, 기본적으로 `Room` map/pool의 key인 id를 저장한다.

단, static collision에서 성능을 최우선으로 할 경우 `CColliderComponent*` 캐시를 선택할 수 있다. 이 문서의 기본안은 수명 안정성과 일관성을 위해 static도 `buildingId` 중심으로 작성한다.

## 현재 서버 상태

관련 위치:

- `GameServer/GameServer/Room.h`
- `GameServer/GameServer/Room.Grid.cpp`
- `GameServer/GameServer/Room.Collision.cpp`
- `GameServer/GameServer/Room.Combat.cpp`
- `GameServer/GameServer/Room.Packet.cpp`
- `GameServer/GameServer/CollisionSystem.cpp`
- `GameServer/GameServer/CollisionSystem.h`

서버에는 이미 다음 구조가 있다.

- `Room::WorldToGridCell`
- `Room::RegisterStaticBuildingToGrid`
- `Room::UpdateDynamicGridState`
- `Room::UpdateMegaGridState`
- `m_gridStaticCells`
- `m_gridDynamicCells`
- `m_megaGridCells`

하지만 현재 cell은 count 중심이다.

```cpp
struct GridStaticCell
{
    uint16_t buildingCount = 0;
    float floorHeight = 0.0f;
};

struct GridDynamicCell
{
    uint16_t playerCount = 0;
    uint16_t monsterCount = 0;
    uint16_t arrowCount = 0;
    uint16_t bulletCount = 0;
};
```

이 구조만으로는 "이 셀에 어떤 객체가 있는가"를 알 수 없으므로, 실제 충돌/패킷 후보 선별에는 부족하다.

## 현재 병목 후보

### 1. 전체 collider pair 검사

`CCollisionSystem::OnUpdate()`는 전체 collider pair를 모두 검사한다.

```cpp
for (size_t i = 0; i < n; ++i)
{
    for (size_t j = i + 1; j < n; ++j)
    {
        HandlePair(a, b);
    }
}
```

맵 static building이 약 1800개 이상이고 monster가 수백 개면 pair 수가 폭발한다.

```text
1848 buildings + 727 enemies + players + projectiles
=> 2500 colliders 이상
=> 약 300만 pair / tick
```

다만 이 항목은 구조 변경 폭이 크므로 후순위로 둔다. 먼저 이미 별도 로직으로 처리되는 world static collision과 packet 작성 비용부터 줄인다.

### 2. 월드 static 충돌 검사

`Room::ResolveWorldStaticCollision()`은 player/enemy마다 `CCollisionSystem::HasCollisionWithWorldStatic()`을 호출한다.

현재 `HasCollisionWithWorldStatic()`은 전체 collider를 순회하며 static collider만 검사한다.

```cpp
for (CColliderComponent* other : mColliders)
{
    if (other->GetLayer() != kCollisionLayerWorldStatic) continue;
    if (IsPairIntersecting(subject, other))
        return true;
}
```

full spawn 기준 대략:

```text
727 enemies * 1848 static buildings
=> 약 134만 static collision checks / tick
```

가장 먼저 줄일 대상이다.

### 3. MakeFrameState 전체 enemy 순회

`Room::MakeFrameState()`는 viewer마다 모든 enemy를 순회한 뒤 거리 200 검사로 걸러낸다.

```text
players * enemies
```

현재 `MaxPlayers = 1`이면 제한적이지만, 4명 이상이면 비용이 커진다. 또한 protobuf 객체 생성/직렬화 비용이 뒤따른다.

현재 서버 기준:

```text
enemy packet range: 200
bullet/arrow packet range: 100
AI active range: 100
```

enemy는 XZ 거리 200 이하이면 `S_FRAME_STATE`에 담는 것이 현재 원칙이다. 바라보는 방향이나 frustum은 서버에서 고려하지 않는다.

### 4. projectile vs enemy 전체 순회

현재 active projectile마다 전체 enemy를 검사한다.

```text
active projectiles * enemies
```

pool 크기 기준 최악:

```text
128 projectiles * 727 enemies
=> 약 9만 checks / tick
```

이 항목은 FineGrid dynamic id bucket이 준비된 뒤 처리한다.

## 클라이언트 코드와의 관계

클라이언트에서 확인된 관련 구조:

- `PracticeProject08-9-1/Grid.h`
- `PracticeProject08-9-1/Grid.cpp`
- `PracticeProject08-9-1/CollisionSystem.h`
- `PracticeProject08-9-1/CollisionSystem.cpp`
- `PracticeProject08-9-1/GameScene.cpp`
- `PracticeProject08-9-1/GameSceneLOD.cpp`
- `PracticeProject08-9-1/GameSceneOcclusion.cpp`

클라이언트에 이미 있는 것:

- `CSceneGrid::StampBuildingCellsFromOOBB`
- `CSceneGrid::AddStaticTouchedCells`
- `CSceneGrid::AddDynamicCount`
- `CCollisionSystem::OnUpdateFiltered`
- `CGameScene::ShouldKeepCollisionPairByMegaGrid`
- static/skinned distance culling, frustum culling, occlusion culling 흐름

중요한 차이:

```text
클라이언트 collision 최적화:
  OnUpdateFiltered(filter)
  -> ShouldKeepCollisionPairByMegaGrid
  -> 두 collider owner의 MegaGrid mask가 겹치는 pair만 유지

서버 1차 collision 수정안:
  FineGrid에서 nearby static building 후보를 직접 수집
  -> subject collider와 후보 static collider만 정밀 intersection
```

따라서 1번 static collision 최적화는 클라이언트 로직의 1:1 이식이 아니다. 다만 static OOBB를 FineGrid cell에 stamp하는 방식은 클라이언트와 서버가 이미 거의 같은 흐름을 가진다.

2번 packet 후보 최적화는 클라이언트에 없던 서버 전용 로직이다. 서버 packet 작성 비용을 줄이기 위한 `MegaGrid enemyId index` 방식으로 새로 설계한다.

클라이언트 카메라 projection은 `near=1.01`, `far=5000`, `FOV=60`으로 확인된다. 하지만 skinned monster distance cull은 대략 다음 수준이다.

```text
Ghoul: 120
SwordMan: 90
BowMan: 100
Mutant: 110
Boss: 160
```

서버의 enemy packet range `200`은 클라이언트 렌더 거리보다 넉넉하므로 1차 구현에서는 기존 `200`을 유지한다.

## 자료구조 수정안

### Room.h forward declaration

`Room.h`에서 static collider pointer 캐시를 선택한다면 `CColliderComponent` 전방 선언이 필요하다.

```cpp
class CColliderComponent;
```

기본안이 `buildingId` 기반이면 전방 선언은 새 자료구조 때문에 필수는 아니지만, collision helper 선언에서 사용할 수 있다.

### FineGrid static cell

기본안:

```cpp
struct GridStaticCell
{
    uint16_t buildingCount = 0;
    float floorHeight = 0.0f;
    std::vector<uint64> buildingIds;
};
```

성능 최우선 대안:

```cpp
struct GridStaticCell
{
    uint16_t buildingCount = 0;
    float floorHeight = 0.0f;
    std::vector<CColliderComponent*> staticColliders;
};
```

선택 기준:

```text
buildingIds:
  + 수명 안전성, Room map key와 일관성
  + static 제거/재등록이 생겨도 검증 쉬움
  - 후보마다 buildings.find + GetComponent 필요

staticColliders:
  + collision 검사에 바로 사용 가능
  - pointer cache 수명 관리 필요
```

현재 프로젝트에서는 static building이 `BuildRoom()`에서 생성되고 런타임 제거 흐름이 없으므로 pointer cache도 실용적이다. 다만 이 문서의 기본 구현 순서는 `buildingIds`로 둔다.

### FineGrid dynamic cell

후속 단계에서 projectile/melee/AI를 FineGrid 후보 기반으로 바꿀 때 사용한다.

```cpp
struct GridDynamicCell
{
    std::vector<uint64> playerIds;
    std::vector<uint64> enemyIds;
    std::vector<uint64> arrowIds;
    std::vector<uint64> bulletIds;
};
```

### FineGrid dynamic tracker

```cpp
struct GridDynamicTracker
{
    uint64 objectId = 0;
    CServerObject* object = nullptr;
    int prevCellX = -1;
    int prevCellZ = -1;
    bool occupied = false;
};
```

`object` pointer는 위치 갱신용이다. 후보 조회/검증은 id를 통해 `players`, `enemies`, projectile pool lookup으로 수행한다.

### MegaGrid cell

패킷 후보 수집을 위해 `enemyIds`를 추가한다.

```cpp
struct MegaGridCell
{
    bool hasPlayerApproached = false;
    bool isCleared = false;
    bool hasEventOccurred = false;

    int approachWidthCells = 200;
    int approachHeightCells = 200;

    std::vector<uint64> enemyIds;
};
```

필요 시 이후 `playerIds`, `buildingIds`, `arrowIds`, `bulletIds`를 추가할 수 있다. 1차 목표는 `MakeFrameState()` enemy 후보 축소이므로 `enemyIds`만 추가한다.

## FineGrid static collision 수정안

### RegisterStaticBuildingToGrid

현재는 touched cell마다 count만 올린다.

```cpp
++m_gridStaticCells[cellIndex].buildingCount;
```

기본안은 touched cell마다 `building->GetObjectId()`를 저장한다.

```cpp
for (int cellIndex : touchedCells)
{
    if (cellIndex < 0 || cellIndex >= kGridCellCount) continue;

    GridStaticCell& cell = m_gridStaticCells[static_cast<size_t>(cellIndex)];
    ++cell.buildingCount;
    cell.floorHeight = 0.0f;

    const uint64 buildingId = building->GetObjectId();
    if (std::find(cell.buildingIds.begin(), cell.buildingIds.end(), buildingId) == cell.buildingIds.end())
        cell.buildingIds.push_back(buildingId);
}
```

주의:

- sub OOBB가 여러 cell을 stamp하므로 중복 방지가 필요하다.
- `InitializeSpatialGrid()`는 `assign(kGridCellCount, GridStaticCell{})`로 초기화하면 vector도 비워진다.
- `ShutdownSpatialGrid()`에서 `m_gridStaticCells.clear()`하면 id 목록도 정리된다.

### Static 후보 수집 helper

Room에 다음 helper를 추가한다.

```cpp
bool GetGridCellRangeForBounds(
    const BoundingBox& bounds,
    int& minCellX,
    int& maxCellX,
    int& minCellZ,
    int& maxCellZ) const;

bool GetColliderWorldAABB(
    const CColliderComponent& collider,
    BoundingBox& outBounds) const;

void CollectStaticBuildingIdsForCollider(
    const CColliderComponent& subject,
    std::vector<uint64>& outBuildingIds) const;
```

구현 편의상 `BoundingBox`를 `Room.h` private 함수 시그니처에 노출할 경우 include 의존성이 늘어난다. 이미 `ColliderComponent.h`/`BoundingCapsule.h` 쪽에서 DirectX collision 타입을 쓰고 있지만, 헤더 의존성을 줄이려면 다음처럼 해도 된다.

```text
Room.h에는 HasCollisionWithNearbyWorldStatic/CollectStaticBuildingIdsForCollider만 선언
GetColliderWorldAABB, GetGridCellRangeForBounds는 Room.Collision.cpp 또는 Room.Grid.cpp의 익명 namespace helper로 구현
```

`GetColliderWorldAABB` 방향:

- `AABB`: 그대로 사용
- `OOBB`: corners로 AABB 계산
- `BSphere`: center/radius로 AABB 계산
- `BCapsule`: `p0`, `p1`, `radius`로 AABB 계산

`CollectStaticBuildingIdsForCollider` 동작:

```text
1. subject collider의 world AABB 계산
2. AABB가 걸치는 FineGrid cell range 계산
3. 각 cell.buildingIds 순회
4. unordered_set<uint64>로 중복 제거
5. outBuildingIds에 추가
```

### CollisionSystem public wrapper

`CCollisionSystem::IsPairIntersecting`은 현재 private이다. 외부에서 후보 기반 검사에 쓰기 위해 public wrapper를 추가한다.

```cpp
bool TestIntersection(
    const CColliderComponent* a,
    const CColliderComponent* b) const;
```

구현은 private `IsPairIntersecting(a, b)`를 호출한다.

대안으로 candidate 전용 API를 둘 수도 있다.

```cpp
bool HasCollisionWithWorldStaticCandidates(
    const CColliderComponent* subject,
    const std::vector<CColliderComponent*>& candidates) const;
```

기본안은 `Room`이 id 후보를 map으로 검증해야 하므로 `TestIntersection` wrapper가 더 단순하다.

### HasCollisionWithNearbyWorldStatic

Room에 helper를 추가한다.

```cpp
bool HasCollisionWithNearbyWorldStatic(const CColliderComponent* subject) const;
```

기본 흐름:

```cpp
bool Room::HasCollisionWithNearbyWorldStatic(const CColliderComponent* subject) const
{
    if (!_collision || !subject)
        return false;

    std::vector<uint64> buildingIds;
    CollectStaticBuildingIdsForCollider(*subject, buildingIds);

    for (uint64 buildingId : buildingIds)
    {
        auto it = buildings.find(buildingId);
        if (it == buildings.end()) continue;

        const BuildingRef& building = it->second;
        if (!building) continue;

        auto* candidate = building->GetComponent<CColliderComponent>();
        if (!candidate) continue;

        if (_collision->TestIntersection(subject, candidate))
            return true;
    }

    return false;
}
```

이 helper는 "사후 되돌리기"와 "사전 이동 차단" 양쪽에서 공통으로 사용한다. 기존 충돌 정책을 바꾸는 것이 아니라, 기존 정책의 static 후보 수집 방식만 전체 순회에서 FineGrid 후보 조회로 바꾸는 것이다.

### ResolveWorldStaticCollision 교체

현재:

```cpp
collider->OnUpdate(0.0f);
if (_collision->HasCollisionWithWorldStatic(collider))
{
    obj->SetPosition(previousPos);
    collider->OnUpdate(0.0f);
}
```

수정:

```cpp
collider->OnUpdate(0.0f);
if (HasCollisionWithNearbyWorldStatic(collider))
{
    obj->SetPosition(previousPos);
    collider->OnUpdate(0.0f);
}
```

`ResolvePreBlockedShift()` 내부의 `WouldCollideAt`도 같은 helper를 사용하도록 바꾼다.

### 현재 이동 예방 정책 보존

현재 서버는 world static 충돌을 한 가지 방식만 쓰지 않는다.

1. `Room::ProcessInput()`에서 player의 desired movement를 계산한 뒤 `ResolvePreBlockedShift(player, desiredShift)`를 호출한다.
2. `ResolvePreBlockedShift()`는 원하는 위치, x축만 이동한 위치, z축만 이동한 위치를 임시로 대입해 충돌 여부를 먼저 검사한다.
3. 충돌이 예상되면 이동량을 줄이거나 0으로 만든다.
4. 이후 `TickAdvance()`에서 player/enemy `Update(tick)` 후 `ResolveWorldStaticCollision(obj, prevPos)`로 사후 보정도 수행한다.

따라서 FineGrid static collision 최적화는 다음 원칙을 지킨다.

```text
기존 정책:
  이동 전 test position으로 static 충돌을 미리 검사
  가능한 축 이동만 허용
  그래도 충돌하면 previousPos로 복구

수정 후 정책:
  위 흐름은 유지
  단, HasCollisionWithWorldStatic 전체 순회만
  HasCollisionWithNearbyWorldStatic FineGrid 후보 검사로 대체
```

`ResolvePreBlockedShift()`에서 임시로 `obj->SetPosition(testPos)`를 호출하는 흐름은 유지한다. 각 test position마다 `collider->OnUpdate(0.0f)` 후 `HasCollisionWithNearbyWorldStatic(collider)`를 호출해야 한다.

예상 형태:

```cpp
auto WouldCollideAt = [&](const GameMath::Vec3& testPos) -> bool
{
    obj->SetPosition(testPos);
    collider->OnUpdate(0.0f);
    return HasCollisionWithNearbyWorldStatic(collider);
};
```

마지막에 원래 위치로 복구하는 기존 흐름도 반드시 유지한다.

```cpp
obj->SetPosition(originPos);
collider->OnUpdate(0.0f);
```

이 단계의 목표는 "이동 정책 변경"이 아니라 "같은 위치 테스트를 더 적은 static 후보로 수행"하는 것이다.

## MegaGrid packet 후보 수정안

### 목표

`MakeFrameState()`에서 viewer마다 모든 enemy를 순회하지 않는다.

현재:

```text
for viewer
  for all enemies
    if DistSqXZ(viewer, enemy) <= 200^2
      packet에 추가
```

수정:

```text
for viewer
  viewer position 기준 반경 200 bounds 계산
  bounds와 겹치는 MegaGrid 목록 계산
  각 MegaGrid.enemyIds 합치기
  중복 제거
  enemies map 조회
  최종 DistSqXZ <= 200^2 검사
  packet에 추가
```

중요:

```text
MegaGrid 후보 수집은 coarse filter이다.
최종 200m 거리 검사는 반드시 유지한다.
```

### MegaGrid 크기

현재 FineGrid bounds:

```text
X: -600 to 600
Z: -200 to 1000
FineGrid: 1200 x 1200
MegaGrid: 3 x 3
MegaGrid 1칸: 400 x 400
```

enemy packet range `200`은 MegaGrid 한 변의 절반과 같다. 플레이어가 MegaGrid 경계 근처에 있으면 인접 MegaGrid가 후보가 된다.

사용할 방식은 "항상 8방향"이 아니라 다음 방식이다.

```text
player.x +/- 200
player.z +/- 200
이 bounds와 겹치는 MegaGrid만 선택
```

### MegaGrid helper

Room에 다음 helper를 추가한다.

```cpp
bool WorldToMegaGridCell(
    float worldX,
    float worldZ,
    int& outMegaX,
    int& outMegaZ) const;

bool GetMegaGridRangeForCircle(
    const GameMath::Vec3& center,
    float radius,
    int& minMegaX,
    int& maxMegaX,
    int& minMegaZ,
    int& maxMegaZ) const;

void CollectEnemyIdsInMegaGridRadius(
    const GameMath::Vec3& center,
    float radius,
    std::vector<uint64>& outEnemyIds) const;
```

`GetMegaGridRangeForCircle` 구현 방향:

```text
minWorldX = center.x - radius
maxWorldX = center.x + radius
minWorldZ = center.z - radius
maxWorldZ = center.z + radius

world bounds를 FineGrid bounds로 clamp
world coordinate -> FineGrid cell
FineGrid cell -> MegaGrid cell
min/max MegaGrid range 산출
```

`CollectEnemyIdsInMegaGridRadius` 동작:

```text
1. GetMegaGridRangeForCircle(center, radius)
2. range 안 MegaGridCell 순회
3. cell.enemyIds를 unordered_set으로 중복 제거
4. outEnemyIds에 추가
```

이 helper는 최종 거리 검사를 하지 않아도 된다. 최종 거리 검사는 `MakeFrameState()`에서 기존 packet 작성 조건과 함께 수행한다.

### MegaGrid enemy id 갱신

1차 단순 구현:

```cpp
void RebuildMegaGridEnemyIds()
{
    for (MegaGridCell& cell : m_megaGridCells)
        cell.enemyIds.clear();

    for (auto& [enemyId, enemy] : enemies)
    {
        if (!enemy) continue;

        int megaX = -1;
        int megaZ = -1;
        const GameMath::Vec3 pos = enemy->GetPosition();
        if (!WorldToMegaGridCell(pos.x, pos.z, megaX, megaZ))
            continue;

        m_megaGridCells[static_cast<size_t>(MegaGridIndex(megaX, megaZ))].enemyIds.push_back(enemyId);
    }
}
```

이 방식은 enemy 전체 순회가 남는다. 하지만 viewer마다 전체 enemy를 다시 순회하지 않으므로 player 수가 늘수록 이득이 있다. 구현 위험도도 낮다.

중요한 기존 동작 보존 사항:

- 현재 enemy spawn 흐름에서는 `enemy->SetActive(true)`가 호출되는 코드가 보이지 않는다.
- 현재 `MakeFrameState()`는 `enemy->IsDead()`도 제외하지 않는다. 죽은 enemy도 거리 안이면 `ANIMATION_TYPE_DIE` state로 전송될 수 있다.
- 따라서 `RebuildMegaGridEnemyIds()`와 packet용 MegaGrid 후보 수집에서는 `IsActive()`/`IsDead()`로 enemy를 제외하면 안 된다.
- dead enemy를 packet에서 제외하는 것은 별도 gameplay/network 정책 변경이다. 이 최적화 작업에 섞지 않는다.

후속 개선:

```cpp
struct MegaGridDynamicTracker
{
    uint64 objectId = 0;
    CServerObject* object = nullptr;
    int prevMegaX = -1;
    int prevMegaZ = -1;
    bool occupied = false;
};
```

enemy가 MegaGrid를 이동할 때만 old cell에서 remove, new cell에 add 한다. 1차 구현에서는 필요하지 않다.

### MakeFrameState 변경

현재 enemy loop:

```cpp
for (auto& enemyMap : enemies)
{
    EnemyRef& enemy = enemyMap.second;
    if (!enemy)
        continue;

    if (abs(GameMath::DistSqXZ(viewerPos, enemy->GetPosition())) > kEnemyViewRangeSq)
        continue;

    ...
}
```

수정:

```cpp
std::vector<uint64> visibleEnemyIds;
CollectEnemyIdsInMegaGridRadius(viewerPos, kEnemyViewRange, visibleEnemyIds);

for (uint64 enemyId : visibleEnemyIds)
{
    auto it = enemies.find(enemyId);
    if (it == enemies.end()) continue;

    EnemyRef& enemy = it->second;
    if (!enemy) continue;

    if (GameMath::DistSqXZ(viewerPos, enemy->GetPosition()) > kEnemyViewRangeSq)
        continue;

    ...
}
```

`abs(GameMath::DistSqXZ(...))`는 의미가 없다. 제곱 거리는 음수가 될 수 없으므로 수정 시 `abs`는 제거한다.

주의:

- 여기서도 `enemy->IsDead()`를 추가로 거르지 않는다.
- 기존 `BuildEnemyStateCode()`는 dead animation flag를 만들 수 있으므로, packet visibility 최적화는 기존 전송 대상을 유지해야 한다.
- `BuildEnemyStateCode()` 내부에는 `static std::unordered_map<uint64, GameMath::Vec3> s_prevEnemyPos`가 있다. MegaGrid 후보 밖 enemy는 이 함수가 호출되지 않아 이전 위치 cache 갱신도 건너뛴다. 현재도 거리 200 밖 enemy는 함수가 호출되지 않으므로 큰 의미 변화는 없지만, 후보 수집 변경 시 "거리 200 안인데 MegaGrid 후보 누락"이 생기면 animation move flag가 틀어질 수 있다. 따라서 MegaGrid range 계산은 반드시 반경 200 bounds와 겹치는 모든 MegaGrid를 포함해야 한다.

## FineGrid dynamic id bucket 후속안

Projectile hit, melee hit, AI active enemy까지 최적화하려면 FineGrid dynamic id bucket이 필요하다.

### AddDynamicCount 대체

기존:

```cpp
void AddDynamicCount(int cellX, int cellZ, EGridDynamicKind kind, int delta);
```

대체:

```cpp
void AddDynamicObjectToGridCell(
    int cellX,
    int cellZ,
    EGridDynamicKind kind,
    uint64 objectId);

void RemoveDynamicObjectFromGridCell(
    int cellX,
    int cellZ,
    EGridDynamicKind kind,
    uint64 objectId);

std::vector<uint64>* GetDynamicIdBucket(
    GridDynamicCell& cell,
    EGridDynamicKind kind);
```

중복 추가 방지:

```cpp
if (std::find(bucket.begin(), bucket.end(), objectId) == bucket.end())
    bucket.push_back(objectId);
```

제거는 순서가 중요하지 않으므로 swap-remove를 사용한다.

```cpp
template<typename T>
void RemoveUnordered(std::vector<T>& values, const T& value)
{
    auto it = std::find(values.begin(), values.end(), value);
    if (it == values.end()) return;

    *it = values.back();
    values.pop_back();
}
```

### RefreshDynamicTracker

기존 count 증감 대신 id bucket 이동을 한다.

```cpp
void Room::RefreshDynamicTracker(GridDynamicTracker& tracker, EGridDynamicKind kind)
{
    int currentCellX = -1;
    int currentCellZ = -1;
    const bool hasCurrentCell =
        TryGetTrackedCell(tracker.object, currentCellX, currentCellZ);

    if (tracker.occupied)
    {
        if (!hasCurrentCell ||
            tracker.prevCellX != currentCellX ||
            tracker.prevCellZ != currentCellZ)
        {
            RemoveDynamicObjectFromGridCell(
                tracker.prevCellX,
                tracker.prevCellZ,
                kind,
                tracker.objectId);

            tracker.occupied = false;
        }
    }

    if (hasCurrentCell)
    {
        if (!tracker.occupied ||
            tracker.prevCellX != currentCellX ||
            tracker.prevCellZ != currentCellZ)
        {
            AddDynamicObjectToGridCell(
                currentCellX,
                currentCellZ,
                kind,
                tracker.objectId);

            tracker.prevCellX = currentCellX;
            tracker.prevCellZ = currentCellZ;
            tracker.occupied = true;
        }
    }
}
```

주의:

- 현재 `TryGetTrackedCell()`은 `obj->IsActive()`가 false이면 false를 반환한다.
- 현재 enemy spawn 코드에서는 enemy를 active로 설정하는 흐름이 확인되지 않는다.
- 따라서 FineGrid dynamic id bucket 단계에서 enemy 후보를 grid에 올리려면 둘 중 하나를 선택해야 한다.

```text
선택 A:
  enemy spawn 시 SetActive(true)를 명시한다.

선택 B:
  TryGetTrackedCell/RefreshDynamicTracker에서 kind == Monster인 경우 active flag를 요구하지 않는다.
```

현재 packet/AI/combat 로직은 active flag 없이 `enemies` map을 순회하므로, dynamic grid 전환 시 active 조건을 그대로 적용하면 enemy가 후보에서 빠질 수 있다. 이 작업은 별도 semantics 변경 없이 진행해야 한다.

### Projectile lookup

Projectile은 pool에 있으므로 id lookup helper가 필요하다.

```cpp
ProjectileRef FindArrowById(uint64 id) const;
ProjectileRef FindBulletById(uint64 id) const;
```

1차 구현에서는 pool이 작으므로 linear search도 가능하다. 장기적으로는 id lookup map을 둘 수 있다.

## 적용 단계

### 1단계: FineGrid 기반 world static collision 최적화

가장 먼저 적용한다.

```text
GridStaticCell에 buildingIds 추가
RegisterStaticBuildingToGrid에서 touched cell마다 buildingId 등록
Collider bounds -> FineGrid range helper 추가
CollectStaticBuildingIdsForCollider 추가
CollisionSystem::TestIntersection public wrapper 추가
ResolveWorldStaticCollision / ResolvePreBlockedShift를 nearby static 후보 기반으로 교체
```

이 단계는 로직 변경 위험이 낮고 병목 개선 가능성이 크다.

### 2단계: MegaGrid enemyId 기반 MakeFrameState 최적화

```text
MegaGridCell에 enemyIds 추가
WorldToMegaGridCell 추가
GetMegaGridRangeForCircle 추가
CollectEnemyIdsInMegaGridRadius 추가
RebuildMegaGridEnemyIds 추가
MakeFrameState enemy loop를 MegaGrid 후보 기반으로 변경
최종 거리 200 검사는 유지
```

이 단계는 클라이언트에 없던 서버 전용 packet interest 최적화이다.

### 3단계: FineGrid dynamic id bucket 준비

```text
GridDynamicCell을 id bucket 기반으로 변경
GridDynamicTracker에 objectId 추가
AddDynamicCount를 Add/RemoveDynamicObjectToGridCell로 대체
RefreshDynamicTracker를 id 이동 방식으로 변경
```

이후 projectile/melee/AI 후보 조회의 기반이다.

### 4단계: projectile/melee 후보 최적화

```text
projectile vs enemy:
  all enemies scan
  -> FineGrid nearby enemyIds
  -> 최종 거리/높이 검사

player melee:
  all enemies scan
  -> FineGrid nearby enemyIds
  -> 기존 IsInArcXZ 유지

enemy attack:
  all players scan
  -> player 수가 작으므로 후순위
```

### 5단계: ProcessEnemyAI active enemy 최적화

현재:

```text
for all enemies
  IsEnemyNearAnyPlayer(...)
```

후속:

```text
for each player
  FineGrid에서 AI range 100 안 enemyIds 수집
  activeEnemyIdSet 구성
  active enemy만 UpdateAI
```

주의:

현재 범위 밖 enemy는 `SetVelocity(Vec3::Zero())` 처리한다. 전체 enemy 순회를 없애면 이 처리가 빠질 수 있으므로 이전 active set을 보관한다.

```cpp
std::unordered_set<uint64> m_lastActiveEnemyIds;
```

처리:

```text
이번 active set에 없음
이전 active set에는 있음
=> velocity zero
```

### 6단계: CollisionSystem full pair 축소 또는 제거

가장 큰 구조 변경이므로 마지막에 진행한다.

현재 서버는 이미 별도 로직으로 주요 충돌을 처리한다.

- player/enemy vs world static: `ResolveWorldStaticCollision`
- projectile vs enemy: 직접 거리 검사
- player melee vs enemy: arc 검사
- enemy attack vs player: arc 검사

따라서 `_collision->OnUpdate()`가 매 tick 전체 pair를 도는 것은 중복일 가능성이 높다.

권장:

```text
1차: `_collision->OnUpdate()` 호출 목적을 조사한다.
2차: 필요한 pair만 Grid/MegaGrid 후보로 생성해 처리한다.
3차: 불필요하면 TickAdvance에서 제거한다.
```

클라이언트에는 `OnUpdateFiltered(filter)`와 `ShouldKeepCollisionPairByMegaGrid`가 있다. 서버에 동일한 filter 방식을 도입할 수도 있지만, 서버 1차 목표는 full pair 자체에 의존하지 않는 방향이다.

## Tick 순서 주의

현재 `TickAdvance()` 흐름은 대략 다음과 같다.

```text
MakeFrameState
player update + ResolveWorldStaticCollision
weapon fire
enemy update + ResolveWorldStaticCollision
projectile update/hit
melee hit
enemy attack hit
_collision->OnUpdate()
UpdateDynamicGridState()
```

문제:

```text
UpdateDynamicGridState()가 tick 끝에 있어,
같은 tick 안에서 dynamic grid 후보를 쓰면 이전 위치 기준일 수 있다.
```

1단계 FineGrid static collision은 static cell만 쓰므로 dynamic grid 순서 영향이 작다.

2단계 MegaGrid packet 후보는 `MakeFrameState()` 전에 enemyIds가 최신이어야 한다. 1차 구현에서 `RebuildMegaGridEnemyIds()`를 사용한다면 `MakeFrameState()` 직전에 호출하거나, enemy update 이후 다음 frame packet에 반영되도록 명시적으로 선택해야 한다.

권장 1차:

```text
MakeFrameState() 직전에 RebuildMegaGridEnemyIds()
```

단, 현재 `MakeFrameState()`가 tick 시작에 호출되므로 enemy movement 반영은 이전 tick 위치 기준이다. 현재 전체 enemy 순회도 같은 시점의 위치를 쓰므로 동작 의미는 크게 바뀌지 않는다.

후속으로 packet 생성 시점을 tick 후반으로 옮기는 것은 별도 변경으로 둔다.

## 검증 기준

빌드하지 않는다. 다음 방식으로만 확인한다.

```text
git diff
정적 코드 확인
관련 함수 호출 경로 확인
중복 id 제거 로직 확인
null/dead/inactive 검증 확인
```

특히 확인할 점:

- `GridStaticCell::buildingIds`가 중복 추가되지 않는지
- `RegisterStaticBuildingToGrid(building)` 호출 시점에는 `buildings[buildingId] = building` 등록 전이지만, `building->SetObjectId(buildingId)`는 이미 끝난 상태인지
- `CollectStaticBuildingIdsForCollider`가 cell range clamp를 올바르게 하는지
- `HasCollisionWithNearbyWorldStatic`이 기존 `HasCollisionWithWorldStatic`과 같은 필터/정밀 검사 결과를 쓰는지
- `ResolvePreBlockedShift`의 임시 위치 변경 후 원위치 복구가 유지되는지
- `MegaGridCell::enemyIds`가 clear/rebuild 되는지
- `MakeFrameState`에서 최종 거리 200 검사가 유지되는지
- packet 후보 수집에서 기존과 달리 `enemy->IsDead()` 또는 `enemy->IsActive()`로 제외하지 않는지
- FineGrid dynamic id bucket 단계에서 inactive enemy가 grid 후보에서 누락되지 않는지

## 최종 요약

이번 작업은 새 소유권 구조를 만드는 것이 아니다.

```text
Room:
  실제 객체 소유권 유지

FineGrid:
  충돌 후보 인덱스
  1차로 world static collision 최적화

MegaGrid:
  패킷/관심 영역 후보 인덱스
  1차로 MakeFrameState enemy 후보 최적화

최종 판정:
  항상 Room map/pool에서 id 재조회
  null/dead/inactive 확인
  거리/충돌 정밀 검사 유지
```

우선순위:

```text
1. FineGrid static collision 최적화
2. MegaGrid enemyId 기반 MakeFrameState 최적화
3. FineGrid dynamic id bucket 준비
4. projectile/melee/AI 후보 최적화
5. CollisionSystem full pair 축소 또는 제거
```
