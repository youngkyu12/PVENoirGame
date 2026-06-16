from pathlib import Path
from PIL import Image
import sys

SOURCE_WIDTH = 3600
SOURCE_HEIGHT = 2700
GRID_COLS = 4
GRID_ROWS = 3

CELL_WIDTH = SOURCE_WIDTH // GRID_COLS
CELL_HEIGHT = SOURCE_HEIGHT // GRID_ROWS

OUTPUT_MAP = {
    "SkyBox_Front_0.png": 6,
    "SkyBox_Back_0.png": 8,
    "SkyBox_Left_0.png": 5,
    "SkyBox_Right_0.png": 7,
    "SkyBox_Top_0.png": 2,
    "SkyBox_Bottom_0.png": 10,
}

IMAGE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".webp"}


def get_cell_box(index):
    zero_based = index - 1
    col = zero_based % GRID_COLS
    row = zero_based // GRID_COLS

    left = col * CELL_WIDTH
    top = row * CELL_HEIGHT
    right = left + CELL_WIDTH
    bottom = top + CELL_HEIGHT

    return left, top, right, bottom


def find_source_image(directory):
    candidates = []

    for path in directory.iterdir():
        if not path.is_file():
            continue

        if path.suffix.lower() not in IMAGE_EXTENSIONS:
            continue

        if path.name.startswith("SkyBox_"):
            continue

        try:
            with Image.open(path) as img:
                if img.size == (SOURCE_WIDTH, SOURCE_HEIGHT):
                    candidates.append(path)
        except Exception:
            continue

    if len(candidates) == 0:
        raise FileNotFoundError(f"{directory} 에서 {SOURCE_WIDTH}x{SOURCE_HEIGHT} 이미지를 찾지 못했습니다.")

    if len(candidates) > 1:
        names = "\n".join(f"  - {p.name}" for p in candidates)
        raise RuntimeError(f"{SOURCE_WIDTH}x{SOURCE_HEIGHT} 이미지가 여러 개 있습니다. 하나만 남기거나 실행 인자로 파일명을 지정하세요:\n{names}")

    return candidates[0]


def main():
    script_dir = Path(__file__).resolve().parent

    if len(sys.argv) >= 2:
        source_path = script_dir / sys.argv[1]
        if not source_path.exists():
            raise FileNotFoundError(f"지정한 이미지가 없습니다: {source_path}")
    else:
        source_path = find_source_image(script_dir)

    with Image.open(source_path) as img:
        if img.size != (SOURCE_WIDTH, SOURCE_HEIGHT):
            raise ValueError(f"이미지 크기가 {SOURCE_WIDTH}x{SOURCE_HEIGHT}가 아닙니다: {img.size}")

        img = img.convert("RGBA")

        for output_name, cell_index in OUTPUT_MAP.items():
            box = get_cell_box(cell_index)
            cropped = img.crop(box)
            output_path = script_dir / output_name
            cropped.save(output_path, "PNG")
            print(f"{output_name} <- cell {cell_index}, box={box}")

    print(f"완료: {source_path.name}")


if __name__ == "__main__":
    main()