"""
Usage: visualizer.py <MapDataのデータファイル>
"""

import sys

import numpy as np
import pyqtgraph as pg
from PySide6.QtCore import Qt
from PySide6.QtWidgets import QMainWindow


def load_data(path):
    with open(path) as f:
        rows, cols = map(int, f.readline().split())
        data = np.loadtxt(f, dtype=np.uint8)

    return data.reshape(rows, cols)


def main():
    data = load_data(sys.argv[1])
    rows, cols = data.shape

    colors = np.array([
        [235, 238, 240],  # Unknown(0)         : グレー
        [100, 149, 190],  # Ground(1)          : 青
        [210, 115, 110],  # Building(2)        : 赤
        [110, 175, 135],  # BuildingOutline(3) : 緑
        [220, 175, 100],  # Other(4)           : オレンジ
    ], dtype=np.uint8)

    rgb = colors[data]

    pg.setConfigOptions(imageAxisOrder="row-major")
    app = pg.mkQApp("MapData Visualizer")

    window = QMainWindow()
    window.setWindowTitle("MapData Visualizer")

    # 描画領域
    graphics = pg.GraphicsLayoutWidget()
    window.setCentralWidget(graphics)

    # ViewBox: ズーム・パンを管理
    view = graphics.addViewBox()
    view.setAspectLocked(True)
    view.invertY(True)

    # 左ドラッグでパン
    view.setMouseMode(pg.ViewBox.PanMode)
    view.setMouseEnabled(x=True, y=True)

    # ImageItem: 画像そのもの
    image = pg.ImageItem(rgb, axisOrder="row-major")
    view.addItem(image)

    # 初期表示範囲
    view.autoRange()

    def on_mouse_moved(pos):
        if not view.sceneBoundingRect().contains(pos):
            window.statusBar().clearMessage()
            return

        point = image.mapFromScene(pos)

        col = int(np.floor(point.x()))
        row = int(np.floor(point.y()))

        if 0 <= row < rows and 0 <= col < cols:
            window.statusBar().showMessage(
                f"Row: {row}    Col: {col}    Value: {data[row, col]}"
            )
        else:
            window.statusBar().clearMessage()

    graphics.scene().sigMouseMoved.connect(on_mouse_moved)

    window.resize(1000, 800)
    window.show()

    pg.exec()


if __name__ == "__main__":
    main()

