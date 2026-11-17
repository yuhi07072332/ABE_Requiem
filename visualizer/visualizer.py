"""
Usage:
    python visualizer.py types <data_file>
    python visualizer.py heights <data_file>
    python visualizer.py buildings <data_file>
"""

import argparse
import colorsys

import numpy as np
import pyqtgraph as pg
from PySide6.QtWidgets import QMainWindow


TYPE_COLORS = np.array([
    [255, 255, 255],  # Unknown(0)
    [73, 77, 100],    # Ground(1)
    [138, 173, 244],  # Building(2)
    [198, 160, 246], # BuildingOutline(3)
    [238, 212, 159], # Other(4)
], dtype=np.uint8)

HEIGHT_MIN = -30.0
HEIGHT_MAX = 150.0
HEIGHT_STEP = 5.0

GOLDEN_ANGLE = 137.50776405003785


def load_data(path, dtype):
    with open(path) as f:
        rows, cols = map(int, f.readline().split())
        data = np.loadtxt(f, dtype=dtype)

    return data.reshape(rows, cols)


def colorize_types(data):
    if np.any(data >= len(TYPE_COLORS)):
        raise ValueError("Invalid type value")

    return TYPE_COLORS[data]


def colorize_heights(data):
    # Clamp heights to [-30, 150].
    heights = np.clip(data, HEIGHT_MIN, HEIGHT_MAX)

    # Quantize into 5m intervals.
    bins = np.floor(
        (heights - HEIGHT_MIN) / HEIGHT_STEP
    )

    num_bins = int((HEIGHT_MAX - HEIGHT_MIN) / HEIGHT_STEP)

    # Normalize to [0, 1].
    t = np.clip(bins / num_bins, 0.0, 1.0)

    # Low: light blue, High: dark blue.
    low_color = np.array([220, 235, 250])
    high_color = np.array([15, 35, 95])

    rgb = (
        low_color[None, None, :] * (1.0 - t[..., None])
        + high_color[None, None, :] * t[..., None]
    )

    return rgb.astype(np.uint8)


def colorize_buildings(data):
    # 0 = Background (white)
    # 1, 2, ... = Building IDs
    max_id = int(data.max())

    colors = np.empty((max_id + 1, 3), dtype=np.uint8)
    colors[0] = [255, 255, 255]

    for building_id in range(1, max_id + 1):
        hue = ((building_id - 1) * GOLDEN_ANGLE) % 360.0
        hue /= 360.0

        r, g, b = colorsys.hsv_to_rgb(hue, 0.65, 0.90)

        colors[building_id] = [
            round(r * 255),
            round(g * 255),
            round(b * 255),
        ]

    return colors[data]


def main():
    parser = argparse.ArgumentParser(
        description="MapData Visualizer"
    )
    parser.add_argument(
        "mode",
        choices=["types", "heights", "buildings"],
        help="Visualization mode",
    )
    parser.add_argument(
        "file",
        help="MapData file",
    )

    args = parser.parse_args()

    if args.mode == "types":
        data = load_data(args.file, np.uint8)
        rgb = colorize_types(data)

    elif args.mode == "heights":
        data = load_data(args.file, np.float64)
        rgb = colorize_heights(data)

    else:
        data = load_data(args.file, np.int64)

        if np.any(data < 0):
            raise ValueError("Building IDs must be non-negative")

        rgb = colorize_buildings(data)

    rows, cols = data.shape

    pg.setConfigOptions(imageAxisOrder="row-major")
    app = pg.mkQApp("MapData Visualizer")

    window = QMainWindow()
    window.setWindowTitle(f"MapData Visualizer - {args.mode}")

    graphics = pg.GraphicsLayoutWidget()
    window.setCentralWidget(graphics)

    view = graphics.addViewBox()
    view.setAspectLocked(True)
    view.invertY(True)
    view.setMouseMode(pg.ViewBox.PanMode)
    view.setMouseEnabled(x=True, y=True)

    image = pg.ImageItem(rgb, axisOrder="row-major")
    view.addItem(image)

    view.autoRange()

    def on_mouse_moved(pos):
        if not view.sceneBoundingRect().contains(pos):
            window.statusBar().clearMessage()
            return

        point = image.mapFromScene(pos)

        col = int(np.floor(point.x()))
        row = int(np.floor(point.y()))

        if 0 <= row < rows and 0 <= col < cols:
            value = data[row, col]

            if args.mode == "heights":
                value_text = f"{value:.2f} m"
            else:
                value_text = str(value)

            window.statusBar().showMessage(
                f"Row: {row}    Col: {col}    Value: {value_text}"
            )
        else:
            window.statusBar().clearMessage()

    graphics.scene().sigMouseMoved.connect(on_mouse_moved)

    window.resize(1000, 800)
    window.show()

    pg.exec()


if __name__ == "__main__":
    main()
