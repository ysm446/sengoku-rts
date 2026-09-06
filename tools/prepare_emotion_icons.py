"""承認済みの画像案から市松背景を除去し、吹き出し内の32px PNGを作る。"""
from collections import deque
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "assets/ui/emotions"
PALETTES = {
    "motivation": [(53, 29, 12), (234, 80, 0), (255, 139, 0), (255, 183, 0), (255, 230, 110)],
    "anxiety": [(53, 34, 15), (212, 148, 24), (255, 201, 57), (27, 62, 152), (54, 132, 207), (129, 200, 239), (239, 248, 252)],
    "fear": [(53, 37, 24), (188, 217, 232), (239, 248, 252)],
}


def pixel_values(image):
    return zip(*(iter(image.tobytes()),) * len(image.getbands()))


def prepare(name):
    source = Image.open(DEST / "concepts" / f"{name}-v2.png").convert("RGB")
    width, height = source.size
    rgb = list(pixel_values(source))
    # 外周につながる明るい無彩色だけを除去。顔の白目は残す。
    removable = bytearray(min(p) >= 220 and max(p) - min(p) <= 24 for p in rgb)
    removed = bytearray(width * height)
    queue = deque()
    for y in range(height):
        queue.extend((y * width, y * width + width - 1))
    for x in range(width):
        queue.extend((x, (height - 1) * width + x))
    while queue:
        index = queue.popleft()
        if removed[index] or not removable[index]:
            continue
        removed[index] = 1
        x, y = index % width, index // width
        if x: queue.append(index - 1)
        if x + 1 < width: queue.append(index + 1)
        if y: queue.append(index - width)
        if y + 1 < height: queue.append(index + width)
    cutout = source.convert("RGBA")
    cutout.putalpha(Image.frombytes("L", source.size, bytes(0 if r else 255 for r in removed)))
    bounds = cutout.getbbox()
    if not bounds:
        raise ValueError(f"{name}: empty foreground")
    cutout = cutout.crop(bounds)
    cutout.thumbnail((22, 20), Image.Resampling.NEAREST)
    palette = PALETTES[name]
    pixels = []
    for r, g, b, a in pixel_values(cutout):
        if not a:
            pixels.append((0, 0, 0, 0))
        else:
            color = min(palette, key=lambda p: (p[0]-r)**2 + (p[1]-g)**2 + (p[2]-b)**2)
            pixels.append((*color, 255))
    cutout.putdata(pixels)
    icon = Image.new("RGBA", (32, 32))
    icon.paste(cutout, ((32 - cutout.width) // 2, 4 + (20 - cutout.height) // 2))
    icon.save(DEST / f"{name}.png")
    print(f"{name}: {source.size} -> {icon.size}, bounds={icon.getbbox()}, colors={len(set(pixel_values(icon)))}")
    return icon


def main():
    icons = [prepare(name) for name in PALETTES]
    preview = Image.new("RGB", (384, 256), (88, 103, 69))
    for i, icon in enumerate(icons):
        bubble = Image.new("RGBA", (32, 32))
        draw = ImageDraw.Draw(bubble)
        for bounds, color in [((3, 2, 28, 25), (43, 39, 32)), ((6, 24, 11, 29), (43, 39, 32)),
                              ((4, 3, 27, 24), (220, 213, 184)), ((7, 24, 10, 27), (220, 213, 184))]:
            draw.rectangle(bounds, fill=(*color, 255))
        bubble.alpha_composite(icon)
        for y, scale in [(16, 2), (96, 4)]:
            enlarged = bubble.resize((32 * scale, 32 * scale), Image.Resampling.NEAREST)
            preview.paste(enlarged, (i * 128, y), enlarged)
    preview_path = ROOT / "build/emotion-icons-preview.png"
    preview_path.parent.mkdir(exist_ok=True)
    preview.save(preview_path)


if __name__ == "__main__":
    main()
