//-----------------------------------------------------------------------------
// File: MapData.h
// 서버/클라이언트 공용 맵 데이터 정의
//-----------------------------------------------------------------------------
#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace MapData
{
    //=========================================================================
    // 객체 타입 열거형
    //=========================================================================
    enum class EntityType : uint8_t
    {
        Player = 0,
        Zombie = 1,
        Fighter = 2,  // NPC Fighter (적)
    };

    //=========================================================================
    // 스폰 포인트 (플레이어/적 공통)
    //=========================================================================
    struct SpawnPoint
    {
        uint32_t    id;
        EntityType  type;
        float       posX, posY, posZ;
        float       yaw;            // Y축 회전 (도 단위)

        // 에셋 경로 (클라이언트용, 서버는 무시 가능)
        const char* meshPath;
        const char* texturePath;
        const char* idleAnimPath;
        const char* moveAnimPath;   // nullptr이면 idleAnim 사용
    };

    //=========================================================================
    // 월드 설정
    //=========================================================================
    struct WorldConfig
    {
        const char* meshPath;
        const char* texturePath;
        float       posX, posY, posZ;
    };

    //=========================================================================
    // 타일 시스템 (서버 충돌/이동 검증용)
    //=========================================================================
    struct TileConfig
    {
        int     countX;
        int     countZ;
        float   tileSize;
        float   originX;
        float   originZ;

        int WorldToTileX(float worldX) const
        {
            return static_cast<int>((worldX - originX) / tileSize);
        }

        int WorldToTileZ(float worldZ) const
        {
            return static_cast<int>((worldZ - originZ) / tileSize);
        }

        float TileToWorldX(int tileX) const
        {
            return originX + tileX * tileSize + tileSize * 0.5f;
        }

        float TileToWorldZ(int tileZ) const
        {
            return originZ + tileZ * tileSize + tileSize * 0.5f;
        }

        bool IsValidTile(int tileX, int tileZ) const
        {
            return tileX >= 0 && tileX < countX && tileZ >= 0 && tileZ < countZ;
        }
    };

    //=========================================================================
    // 전체 맵 설정
    //=========================================================================
    struct MapConfig
    {
        const char* mapName;
        WorldConfig             world;
        TileConfig              tiles;
        std::vector<SpawnPoint> spawns;     // 모든 스폰 포인트 (플레이어 + 적)

        // 헬퍼: 타입별 스폰 포인트 가져오기
        std::vector<SpawnPoint> GetSpawnsByType(EntityType type) const
        {
            std::vector<SpawnPoint> result;
            for (const auto& sp : spawns)
            {
                if (sp.type == type)
                    result.push_back(sp);
            }
            return result;
        }

        const SpawnPoint* GetSpawnById(uint32_t id) const
        {
            for (const auto& sp : spawns)
            {
                if (sp.id == id)
                    return &sp;
            }
            return nullptr;
        }

        size_t GetPlayerSpawnCount() const
        {
            size_t count = 0;
            for (const auto& sp : spawns)
                if (sp.type == EntityType::Player) ++count;
            return count;
        }

        size_t GetEnemyCount() const
        {
            size_t count = 0;
            for (const auto& sp : spawns)
                if (sp.type != EntityType::Player) ++count;
            return count;
        }
    };

    //=========================================================================
    // 기본 맵 데이터 (하드코딩 - 나중에 JSON/Binary 로드로 교체 가능)
    //=========================================================================
    inline MapConfig GetDefaultMap()
    {
        MapConfig cfg;
        cfg.mapName = "NoirCity";

        // 월드 설정
        cfg.world = {
            "Assets/World/Mesh/StartWorld.bin",
            "Assets/World/Texture",
            0.0f, 0.0f, 30.0f
        };

        // 타일 시스템
        cfg.tiles = {
            400,        // countX
            400,        // countZ
            1.0f,       // tileSize
            -200.0f,    // originX
            -200.0f     // originZ
        };

        // 에셋 경로 상수
        constexpr const char* FIGHTER_MESH = "Assets/Fighter/Mesh/Fighter.bin";
        constexpr const char* FIGHTER_TEX = "Assets/Fighter/Texture";
        constexpr const char* FIGHTER_IDLE = "Assets/Fighter/Animation/FighterIdle.bin";
        constexpr const char* FIGHTER_RUN = "Assets/Fighter/Animation/FighterRun.bin";

        constexpr const char* ZOMBIE_MESH = "Assets/Zombie/Mesh/Zombie.bin";
        constexpr const char* ZOMBIE_TEX = "Assets/Zombie/Texture";
        constexpr const char* ZOMBIE_IDLE = "Assets/Zombie/Animation/ZombieAttack.bin";

        // 스폰 포인트 정의
        cfg.spawns = {
            //=================================================================
            // 플레이어 스폰 (ID 0~3)
            //=================================================================
            { 0, EntityType::Player,    0.0f, 0.0f,   0.0f,   0.0f,
              FIGHTER_MESH, FIGHTER_TEX, FIGHTER_IDLE, FIGHTER_RUN },

            { 1, EntityType::Player,    5.0f, 0.0f,   0.0f,  45.0f,
              FIGHTER_MESH, FIGHTER_TEX, FIGHTER_IDLE, FIGHTER_RUN },

            { 2, EntityType::Player,   -5.0f, 0.0f,   0.0f, -45.0f,
              FIGHTER_MESH, FIGHTER_TEX, FIGHTER_IDLE, FIGHTER_RUN },

            { 3, EntityType::Player,    0.0f, 0.0f,   5.0f, 180.0f,
              FIGHTER_MESH, FIGHTER_TEX, FIGHTER_IDLE, FIGHTER_RUN },

              //=================================================================
              // Zombie 스폰 (ID 100~)
              //=================================================================
              { 100, EntityType::Zombie, -30.0f, 0.0f, 20.0f,  45.0f,
                ZOMBIE_MESH, ZOMBIE_TEX, ZOMBIE_IDLE, nullptr },

              { 101, EntityType::Zombie,  15.0f, 0.0f, 50.0f,  90.0f,
                ZOMBIE_MESH, ZOMBIE_TEX, ZOMBIE_IDLE, nullptr },

              { 102, EntityType::Zombie, -20.0f, 0.0f, 60.0f, 180.0f,
                ZOMBIE_MESH, ZOMBIE_TEX, ZOMBIE_IDLE, nullptr },

              { 103, EntityType::Zombie,  25.0f, 0.0f, 30.0f, 270.0f,
                ZOMBIE_MESH, ZOMBIE_TEX, ZOMBIE_IDLE, nullptr },

              { 104, EntityType::Zombie,  10.0f, 0.0f, 70.0f,   0.0f,
                ZOMBIE_MESH, ZOMBIE_TEX, ZOMBIE_IDLE, nullptr },

              { 105, EntityType::Zombie, -35.0f, 0.0f, 40.0f, 135.0f,
                ZOMBIE_MESH, ZOMBIE_TEX, ZOMBIE_IDLE, nullptr },

                //=================================================================
                // Fighter NPC 스폰 (ID 200~)
                //=================================================================
                { 200, EntityType::Fighter, -10.0f, 0.0f, 25.0f,  60.0f,
                  FIGHTER_MESH, FIGHTER_TEX, FIGHTER_IDLE, nullptr },

                { 201, EntityType::Fighter,  20.0f, 0.0f, 45.0f, 120.0f,
                  FIGHTER_MESH, FIGHTER_TEX, FIGHTER_IDLE, nullptr },

                { 202, EntityType::Fighter, -25.0f, 0.0f, 55.0f, 240.0f,
                  FIGHTER_MESH, FIGHTER_TEX, FIGHTER_IDLE, nullptr },

                { 203, EntityType::Fighter,  30.0f, 0.0f, 15.0f, 300.0f,
                  FIGHTER_MESH, FIGHTER_TEX, FIGHTER_IDLE, nullptr },

                { 204, EntityType::Fighter,   5.0f, 0.0f, 65.0f,  30.0f,
                  FIGHTER_MESH, FIGHTER_TEX, FIGHTER_IDLE, nullptr },

                { 205, EntityType::Fighter, -15.0f, 0.0f, 35.0f, 210.0f,
                  FIGHTER_MESH, FIGHTER_TEX, FIGHTER_IDLE, nullptr },
        };

        return cfg;
    }

    //=========================================================================
    // 엔티티 타입 문자열 변환 (디버그/로깅용)
    //=========================================================================
    inline const char* EntityTypeToString(EntityType type)
    {
        switch (type)
        {
        case EntityType::Player:  return "Player";
        case EntityType::Zombie:  return "Zombie";
        case EntityType::Fighter: return "Fighter";
        default:                  return "Unknown";
        }
    }

    inline EntityType StringToEntityType(const char* str)
    {
        if (strcmp(str, "Player") == 0)  return EntityType::Player;
        if (strcmp(str, "Zombie") == 0)  return EntityType::Zombie;
        if (strcmp(str, "Fighter") == 0) return EntityType::Fighter;
        return EntityType::Player;
    }

} // namespace MapData