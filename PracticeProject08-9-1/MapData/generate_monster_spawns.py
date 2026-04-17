#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
generate_monster_spawns.py

목적
- DumpStaticGridOccupancyLog()가 출력한 1200 x 1200 정적 점유 로그(0/1)를 읽는다.
- 각 메가그리드(3 x 3, 각 400 x 400) 중앙 200 x 200 구역 안에서,
  값이 0인 셀만 골라 몬스터 시작 위치를 만든다.
- Boss는 예외적으로 점유 로그를 무시하고 해당 메가그리드 정중앙에 배치한다.
- 결과는 DX12 쪽에서 나중에 파싱하기 쉬운 txt 파일로 저장한다.

메가그리드 번호 규칙
- 내부 좌표: mega_x = 0..2, mega_z = 0..2
- 1-based ID: mega_id = mega_z * 3 + mega_x + 1
- 월드 공간에서 +Z를 위쪽으로 보면 배치는 다음과 같다.

    7 8 9
    4 5 6
    1 2 3

주의
- 텍스트 로그의 첫 번째 데이터 줄은 z = kGridMinZ 쪽(가장 작은 Z)부터 시작한다.
  즉, 로그 파일의 "위에서 아래" 순서와 일반적인 월드맵의 "아래에서 위" 감각이 다를 수 있다.
- 현재 DX12 코드 기준 상수:
    kGridMinX = -600, kGridMaxX = 600
    kGridMinZ = -200, kGridMaxZ = 1000
    kMegaGridCols = 3, kMegaGridRows = 3
    approach zone = 중앙 200 x 200
"""

from __future__ import annotations

import math
import random
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Tuple


# =============================================================================
# 편집용 설정
# =============================================================================

INPUT_GRID_PATH = "그리드 지도.txt"
OUTPUT_SPAWN_PATH = "monster_spawn_points_little.txt"

# None이면 시스템 랜덤, 정수를 넣으면 재현 가능한 결과
RNG_SEED = 20260416

# DX12 쪽 월드/그리드 상수
GRID_MIN_X = -600
GRID_MAX_X = 600
GRID_MIN_Z = -200
GRID_MAX_Z = 1000

GRID_WIDTH = GRID_MAX_X - GRID_MIN_X      # 1200
GRID_HEIGHT = GRID_MAX_Z - GRID_MIN_Z     # 1200

MEGA_GRID_COLS = 3
MEGA_GRID_ROWS = 3
MEGA_GRID_COUNT = MEGA_GRID_COLS * MEGA_GRID_ROWS

MEGA_GRID_CELL_WIDTH = GRID_WIDTH // MEGA_GRID_COLS       # 400
MEGA_GRID_CELL_HEIGHT = GRID_HEIGHT // MEGA_GRID_ROWS     # 400

APPROACH_WIDTH = 200
APPROACH_HEIGHT = 200

# 점유 가능한 0 셀을 여러 몬스터가 공유할지 여부.
# False면 non-boss 몬스터끼리는 같은 좌표를 쓰지 않는다.
ALLOW_SHARED_NON_BOSS_CELLS = False

# 결과 TXT에 기록할 기본 y 값. 필요하면 몬스터별로 바꾸면 된다.
MONSTER_Y: Dict[str, int] = {
    "Ghoul": 0,
    "BowMan": 0,
    "SwordMan": 0,
    "Mutant": 0,
    "Boss": 0,
}

# 몬스터 출력 순서
MONSTER_ORDER: Tuple[str, ...] = (
    "Ghoul",
    "BowMan",
    "SwordMan",
    "Mutant",
    "Boss",
)

# 수동 편집용 몬스터 수 설정.
# 여기 값을 바꾸면 된다.
# Boss도 여기에 그대로 둔다. Boss는 count만큼 "해당 메가그리드 중심점"에 겹쳐서 생성된다.
MEGA_GRID_MONSTER_COUNTS: Dict[int, Dict[str, int]] = {
    1: {"Ghoul": 0, "BowMan": 0, "SwordMan": 0, "Mutant": 0, "Boss": 0},
    2: {"Ghoul": 100, "BowMan": 0, "SwordMan": 0, "Mutant": 0, "Boss": 0},
    3: {"Ghoul": 95, "BowMan": 5, "SwordMan": 5, "Mutant": 0, "Boss": 0},
    4: {"Ghoul": 0, "BowMan": 0, "SwordMan": 0, "Mutant": 0, "Boss": 0},
    5: {"Ghoul": 0, "BowMan": 0, "SwordMan": 0, "Mutant": 0, "Boss": 1},
    6: {"Ghoul": 50, "BowMan": 0, "SwordMan": 10, "Mutant": 1, "Boss": 0},
    7: {"Ghoul": 0, "BowMan": 0, "SwordMan": 0, "Mutant": 0, "Boss": 0},
    8: {"Ghoul": 0, "BowMan": 0, "SwordMan": 0, "Mutant": 0, "Boss": 0},
    9: {"Ghoul": 0, "BowMan": 0, "SwordMan": 0, "Mutant": 0, "Boss": 0},
}

# 아래처럼 오타/대소문자 차이를 어느 정도 흡수한다.
MONSTER_NAME_ALIASES: Dict[str, str] = {
    "ghoul": "Ghoul",
    "bowman": "BowMan",
    "swordman": "SwordMan",
    "mutant": "Mutant",
    "boss": "Boss",
}

# 혹시 사용자가 BowMan / SwordMan / Ghoul 그대로 적어도 허용
for _name in MONSTER_ORDER:
    MONSTER_NAME_ALIASES[_name.lower()] = _name


# =============================================================================
# 데이터 구조
# =============================================================================

@dataclass(frozen=True)
class MegaGridInfo:
    mega_id: int
    mega_x: int
    mega_z: int
    world_x_min: int
    world_x_max_exclusive: int
    world_z_min: int
    world_z_max_exclusive: int
    spawn_x_min: int
    spawn_x_max_exclusive: int
    spawn_z_min: int
    spawn_z_max_exclusive: int
    center_x: int
    center_z: int


@dataclass(frozen=True)
class SpawnEntry:
    monster_type: str
    mega_id: int
    mega_x: int
    mega_z: int
    x: int
    y: int
    z: int
    yaw_deg: float
    qx: float
    qy: float
    qz: float
    qw: float
    spawn_rule: str


# =============================================================================
# 유틸
# =============================================================================

def canonicalize_monster_name(name: str) -> str:
    key = name.strip().lower()
    canonical = MONSTER_NAME_ALIASES.get(key)
    if canonical is None:
        raise ValueError(
            f"알 수 없는 몬스터 이름: {name!r}. "
            f"허용: {', '.join(MONSTER_ORDER)}"
        )
    return canonical


def yaw_deg_to_quaternion_y(yaw_deg: float) -> Tuple[float, float, float, float]:
    yaw_rad = math.radians(yaw_deg)
    half = yaw_rad * 0.5
    # Y축 회전만 사용
    return (0.0, math.sin(half), 0.0, math.cos(half))


def build_rng() -> random.Random:
    return random.Random(RNG_SEED)


def mega_id_from_coords(mega_x: int, mega_z: int) -> int:
    return mega_z * MEGA_GRID_COLS + mega_x + 1


def mega_coords_from_id(mega_id: int) -> Tuple[int, int]:
    if mega_id < 1 or mega_id > MEGA_GRID_COUNT:
        raise ValueError(f"mega_id 범위가 잘못됨: {mega_id}")
    zero_based = mega_id - 1
    mega_x = zero_based % MEGA_GRID_COLS
    mega_z = zero_based // MEGA_GRID_COLS
    return mega_x, mega_z


def world_x_from_cell_x(cell_x: int) -> int:
    return GRID_MIN_X + cell_x


def world_z_from_cell_z(cell_z: int) -> int:
    return GRID_MIN_Z + cell_z


def cell_x_from_world_x(world_x: int) -> int:
    return world_x - GRID_MIN_X


def cell_z_from_world_z(world_z: int) -> int:
    return world_z - GRID_MIN_Z


def build_mega_grid_info(mega_id: int) -> MegaGridInfo:
    mega_x, mega_z = mega_coords_from_id(mega_id)

    world_x_min = GRID_MIN_X + mega_x * MEGA_GRID_CELL_WIDTH
    world_x_max_exclusive = world_x_min + MEGA_GRID_CELL_WIDTH

    world_z_min = GRID_MIN_Z + mega_z * MEGA_GRID_CELL_HEIGHT
    world_z_max_exclusive = world_z_min + MEGA_GRID_CELL_HEIGHT

    spawn_x_min = world_x_min + (MEGA_GRID_CELL_WIDTH - APPROACH_WIDTH) // 2
    spawn_x_max_exclusive = spawn_x_min + APPROACH_WIDTH

    spawn_z_min = world_z_min + (MEGA_GRID_CELL_HEIGHT - APPROACH_HEIGHT) // 2
    spawn_z_max_exclusive = spawn_z_min + APPROACH_HEIGHT

    center_x = world_x_min + (MEGA_GRID_CELL_WIDTH // 2)
    center_z = world_z_min + (MEGA_GRID_CELL_HEIGHT // 2)

    return MegaGridInfo(
        mega_id=mega_id,
        mega_x=mega_x,
        mega_z=mega_z,
        world_x_min=world_x_min,
        world_x_max_exclusive=world_x_max_exclusive,
        world_z_min=world_z_min,
        world_z_max_exclusive=world_z_max_exclusive,
        spawn_x_min=spawn_x_min,
        spawn_x_max_exclusive=spawn_x_max_exclusive,
        spawn_z_min=spawn_z_min,
        spawn_z_max_exclusive=spawn_z_max_exclusive,
        center_x=center_x,
        center_z=center_z,
    )


def normalize_counts(raw_counts: Dict[int, Dict[str, int]]) -> Dict[int, Dict[str, int]]:
    normalized: Dict[int, Dict[str, int]] = {}

    for mega_id in range(1, MEGA_GRID_COUNT + 1):
        normalized[mega_id] = {monster: 0 for monster in MONSTER_ORDER}

    for mega_id, monster_counts in raw_counts.items():
        if mega_id < 1 or mega_id > MEGA_GRID_COUNT:
            raise ValueError(f"존재하지 않는 mega_id 설정: {mega_id}")

        for raw_name, raw_count in monster_counts.items():
            name = canonicalize_monster_name(raw_name)

            if not isinstance(raw_count, int):
                raise TypeError(
                    f"mega_id={mega_id}, monster={raw_name!r} count는 int여야 함. "
                    f"현재 타입: {type(raw_count).__name__}"
                )
            if raw_count < 0:
                raise ValueError(
                    f"mega_id={mega_id}, monster={raw_name!r} count는 0 이상이어야 함."
                )

            normalized[mega_id][name] = raw_count

    return normalized


# =============================================================================
# 그리드 입력
# =============================================================================

def parse_grid_occupancy_file(path: Path) -> List[str]:
    if not path.exists():
        raise FileNotFoundError(f"입력 파일을 찾을 수 없음: {path}")

    raw_lines = path.read_text(encoding="utf-8").splitlines()
    stripped = [line.strip() for line in raw_lines if line.strip()]

    try:
        begin_index = stripped.index("[GridStatic] begin")
        end_index = stripped.index("[GridStatic] end")
    except ValueError as exc:
        raise ValueError(
            "입력 파일에 [GridStatic] begin / [GridStatic] end 마커가 없음."
        ) from exc

    if end_index <= begin_index:
        raise ValueError("GridStatic begin/end 순서가 잘못됨.")

    rows = stripped[begin_index + 1:end_index]

    if len(rows) != GRID_HEIGHT:
        raise ValueError(
            f"그리드 row 수가 맞지 않음. 기대={GRID_HEIGHT}, 실제={len(rows)}"
        )

    for row_index, row in enumerate(rows):
        if len(row) != GRID_WIDTH:
            raise ValueError(
                f"그리드 col 수가 맞지 않음. row={row_index}, 기대={GRID_WIDTH}, 실제={len(row)}"
            )
        if any(ch not in ("0", "1") for ch in row):
            raise ValueError(
                f"그리드에 0/1 외 문자가 있음. row={row_index}"
            )

    return rows


# =============================================================================
# 스폰 좌표 계산
# =============================================================================

def iter_zero_cells_in_mega_spawn_area(
    grid_rows: List[str],
    info: MegaGridInfo,
) -> Iterable[Tuple[int, int]]:
    for world_z in range(info.spawn_z_min, info.spawn_z_max_exclusive):
        cell_z = cell_z_from_world_z(world_z)

        for world_x in range(info.spawn_x_min, info.spawn_x_max_exclusive):
            cell_x = cell_x_from_world_x(world_x)

            if grid_rows[cell_z][cell_x] == "0":
                yield (world_x, world_z)


def random_yaw_deg(rng: random.Random) -> float:
    return rng.uniform(0.0, 360.0)


def build_spawn_entry(
    monster_type: str,
    info: MegaGridInfo,
    x: int,
    y: int,
    z: int,
    rng: random.Random,
    spawn_rule: str,
) -> SpawnEntry:
    yaw_deg = random_yaw_deg(rng)
    qx, qy, qz, qw = yaw_deg_to_quaternion_y(yaw_deg)

    return SpawnEntry(
        monster_type=monster_type,
        mega_id=info.mega_id,
        mega_x=info.mega_x,
        mega_z=info.mega_z,
        x=x,
        y=y,
        z=z,
        yaw_deg=yaw_deg,
        qx=qx,
        qy=qy,
        qz=qz,
        qw=qw,
        spawn_rule=spawn_rule,
    )


def generate_spawns_for_mega_grid(
    grid_rows: List[str],
    info: MegaGridInfo,
    counts: Dict[str, int],
    rng: random.Random,
) -> Tuple[List[SpawnEntry], int]:
    available_zero_cells = list(iter_zero_cells_in_mega_spawn_area(grid_rows, info))
    spawns: List[SpawnEntry] = []

    non_boss_total = sum(counts.get(monster, 0) for monster in MONSTER_ORDER if monster != "Boss")
    boss_total = counts.get("Boss", 0)

    if not ALLOW_SHARED_NON_BOSS_CELLS and non_boss_total > len(available_zero_cells):
        raise ValueError(
            f"mega_id={info.mega_id} 에 non-boss 몬스터를 배치할 0 셀이 부족함. "
            f"요청={non_boss_total}, 사용가능={len(available_zero_cells)}"
        )

    if ALLOW_SHARED_NON_BOSS_CELLS:
        chosen_positions: List[Tuple[int, int]] = [
            rng.choice(available_zero_cells) for _ in range(non_boss_total)
        ] if non_boss_total > 0 else []
    else:
        chosen_positions = rng.sample(available_zero_cells, non_boss_total) if non_boss_total > 0 else []

    cursor = 0
    for monster in MONSTER_ORDER:
        count = counts.get(monster, 0)
        if count <= 0:
            continue

        y = MONSTER_Y.get(monster, 0)

        if monster == "Boss":
            for _ in range(count):
                spawns.append(
                    build_spawn_entry(
                        monster_type=monster,
                        info=info,
                        x=info.center_x,
                        y=y,
                        z=info.center_z,
                        rng=rng,
                        spawn_rule="boss_center_ignore_occupancy",
                    )
                )
            continue

        for _ in range(count):
            x, z = chosen_positions[cursor]
            cursor += 1
            spawns.append(
                build_spawn_entry(
                    monster_type=monster,
                    info=info,
                    x=x,
                    y=y,
                    z=z,
                    rng=rng,
                    spawn_rule="zero_cell_in_center_200x200",
                )
            )

    return spawns, len(available_zero_cells)


# =============================================================================
# 출력
# =============================================================================

def build_txt_output(
    all_spawns: List[SpawnEntry],
    counts: Dict[int, Dict[str, int]],
    available_counts: Dict[int, int],
) -> str:
    lines: List[str] = []

    lines.append("# Monster spawn data generated from DumpStaticGridOccupancyLog()")
    lines.append("#")
    lines.append("# Mega grid layout in world space (+Z upward):")
    lines.append("# 7 8 9")
    lines.append("# 4 5 6")
    lines.append("# 1 2 3")
    lines.append("#")
    lines.append("# ID rule: mega_id = mega_z * 3 + mega_x + 1")
    lines.append("# Internal mega coords are 0-based.")
    lines.append("#")
    lines.append(
        f"CONFIG|grid_min=({GRID_MIN_X},{GRID_MIN_Z})|grid_max_exclusive=({GRID_MAX_X},{GRID_MAX_Z})"
        f"|grid_size=({GRID_WIDTH},{GRID_HEIGHT})|mega_grid_size=({MEGA_GRID_CELL_WIDTH},{MEGA_GRID_CELL_HEIGHT})"
        f"|center_spawn_size=({APPROACH_WIDTH},{APPROACH_HEIGHT})|allow_shared_non_boss_cells={int(ALLOW_SHARED_NON_BOSS_CELLS)}"
        f"|rng_seed={'None' if RNG_SEED is None else RNG_SEED}"
    )

    for mega_id in range(1, MEGA_GRID_COUNT + 1):
        info = build_mega_grid_info(mega_id)
        requested_non_boss = sum(
            counts[mega_id].get(monster, 0)
            for monster in MONSTER_ORDER
            if monster != "Boss"
        )
        requested_boss = counts[mega_id].get("Boss", 0)

        lines.append(
            f"MEGA|id={mega_id}|mega=({info.mega_x},{info.mega_z})"
            f"|world_min=({info.world_x_min},{info.world_z_min})"
            f"|world_max_exclusive=({info.world_x_max_exclusive},{info.world_z_max_exclusive})"
            f"|spawn_min=({info.spawn_x_min},{info.spawn_z_min})"
            f"|spawn_max_exclusive=({info.spawn_x_max_exclusive},{info.spawn_z_max_exclusive})"
            f"|center=({info.center_x},{info.center_z})"
            f"|available_zero_cells={available_counts[mega_id]}"
            f"|requested_non_boss={requested_non_boss}"
            f"|requested_boss={requested_boss}"
        )

    lines.append("")

    for index, spawn in enumerate(all_spawns):
        lines.append(
            f"SPAWN|index={index}"
            f'|type="{spawn.monster_type}"'
            f"|mega_id={spawn.mega_id}|mega=({spawn.mega_x},{spawn.mega_z})"
            f"|pos=({spawn.x},{spawn.y},{spawn.z})"
            f"|yaw_deg={spawn.yaw_deg:.3f}"
            f"|rot=({spawn.qx:.6f},{spawn.qy:.6f},{spawn.qz:.6f},{spawn.qw:.6f})"
            f"|rule={spawn.spawn_rule}"
        )

    return "\n".join(lines) + "\n"


# =============================================================================
# 메인
# =============================================================================

def main() -> None:
    input_path = Path(INPUT_GRID_PATH)
    output_path = Path(OUTPUT_SPAWN_PATH)

    grid_rows = parse_grid_occupancy_file(input_path)
    normalized_counts = normalize_counts(MEGA_GRID_MONSTER_COUNTS)
    rng = build_rng()

    all_spawns: List[SpawnEntry] = []
    available_counts: Dict[int, int] = {}

    for mega_id in range(1, MEGA_GRID_COUNT + 1):
        info = build_mega_grid_info(mega_id)
        spawns, available = generate_spawns_for_mega_grid(
            grid_rows=grid_rows,
            info=info,
            counts=normalized_counts[mega_id],
            rng=rng,
        )
        available_counts[mega_id] = available
        all_spawns.extend(spawns)

    output_text = build_txt_output(
        all_spawns=all_spawns,
        counts=normalized_counts,
        available_counts=available_counts,
    )
    output_path.write_text(output_text, encoding="utf-8")

    print(f"[OK] input : {input_path}")
    print(f"[OK] output: {output_path}")
    print(f"[OK] total spawns: {len(all_spawns)}")
    print("[OK] mega grid layout (+Z upward):")
    print("     7 8 9")
    print("     4 5 6")
    print("     1 2 3")

    for mega_id in range(1, MEGA_GRID_COUNT + 1):
        info = build_mega_grid_info(mega_id)
        print(
            f"  mega_id={mega_id} mega=({info.mega_x},{info.mega_z}) "
            f"world=[{info.world_x_min},{info.world_x_max_exclusive}) x "
            f"[{info.world_z_min},{info.world_z_max_exclusive}) "
            f"spawn_center_area=[{info.spawn_x_min},{info.spawn_x_max_exclusive}) x "
            f"[{info.spawn_z_min},{info.spawn_z_max_exclusive}) "
            f"available_zero_cells={available_counts[mega_id]}"
        )


if __name__ == "__main__":
    main()
