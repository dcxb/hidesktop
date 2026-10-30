"""Generate the hidesktop tray icons.

Draws a 256x256 master image at 4x and downsamples it, so the small sizes stay
readable. The "hidden" variant is grey with a slash through the monitor.

Usage:  python tools/make_icons.py
"""

from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 256
SUPERSAMPLE = 4
ICON_SIZES = [16, 20, 24, 32, 40, 48, 64, 256]

VISIBLE_BG = (31, 111, 235, 255)   # blue
HIDDEN_BG = (96, 104, 117, 255)    # grey
WHITE = (255, 255, 255, 255)
HIDDEN_FG = (228, 232, 238, 255)


def draw_icon(hidden: bool) -> Image.Image:
    canvas = Image.new("RGBA", (SIZE * SUPERSAMPLE, SIZE * SUPERSAMPLE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    s = SUPERSAMPLE

    background = HIDDEN_BG if hidden else VISIBLE_BG
    foreground = HIDDEN_FG if hidden else WHITE

    # Rounded square badge.
    draw.rounded_rectangle([8 * s, 8 * s, 248 * s, 248 * s], radius=60 * s, fill=background)

    # Monitor outline + stand.
    draw.rounded_rectangle(
        [56 * s, 66 * s, 200 * s, 166 * s],
        radius=14 * s,
        outline=foreground,
        width=13 * s,
    )
    draw.line([128 * s, 166 * s, 128 * s, 190 * s], fill=foreground, width=13 * s)
    draw.line([96 * s, 196 * s, 160 * s, 196 * s], fill=foreground, width=13 * s)

    if hidden:
        # "Hidden" slash.
        draw.line([48 * s, 208 * s, 208 * s, 48 * s], fill=WHITE, width=17 * s)

    return canvas.resize((SIZE, SIZE), Image.LANCZOS)


def main() -> None:
    output = Path(__file__).resolve().parent.parent / "src" / "res"
    output.mkdir(parents=True, exist_ok=True)

    for hidden, name in ((False, "hidesktop.ico"), (True, "hidesktop_hidden.ico")):
        image = draw_icon(hidden)
        target = output / name
        image.save(target, format="ICO", sizes=[(size, size) for size in ICON_SIZES])
        print(f"wrote {target}")


if __name__ == "__main__":
    main()
