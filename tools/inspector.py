"""2D elevation and classification viewer for ABE_Requiem.

Usage:
    python tools\\inspector.py <mapdata-file>
"""

import sys

import numpy as np
import pyqtgraph as pg
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QAbstractItemView,
    QComboBox,
    QFileDialog,
    QFormLayout,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QStackedWidget,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
    QHeaderView,
)

TYPE_NAMES = {0: "Unknown", 1: "Ground", 2: "Building", 3: "BuildingOutline"}
TYPE_COLORS = np.array(
    [[255, 255, 255], [0, 0, 255], [255, 0, 0], [0, 180, 0]], dtype=np.uint8
)


def load_points(path):
    with open(path, encoding="utf-8-sig") as source:
        first_line = source.readline().split()
        if not first_line or first_line[0].lstrip("\uFEFF") != "MAPDATA":
            raise ValueError(f"{path}: C++ inspectが出力したMAPDATA形式が必要です")
        rows, cols, origin_x, origin_z = map(int, source.readline().split()[:4])
        values = np.loadtxt(source, dtype=float)
        values = np.atleast_2d(values)
        if values.shape[1] not in (2, 3):
            raise ValueError(f"{path}: MAPDATAの列数が不正です")
        values = values.reshape(rows * cols, values.shape[1])
        heights = values[:, 0].reshape(rows, cols)
        embedded_types = values[:, 1].astype(np.uint8).reshape(rows, cols)
        missing = (
            values[:, 2].astype(bool).reshape(rows, cols)
            if values.shape[1] == 3
            else ~np.isfinite(heights)
        )
        return heights, origin_x, origin_z, embedded_types, missing


def classification_image(types):
    return TYPE_COLORS[np.clip(types, 0, len(TYPE_COLORS) - 1)]


def elevation_image(heights):
    valid = np.isfinite(heights)
    if not valid.any():
        return np.zeros((*heights.shape, 3), dtype=np.uint8)
    values = heights[valid]
    low, high = np.percentile(values, [5, 95])
    normalized = np.clip(np.nan_to_num((heights - low) / max(high - low, 1e-12)), 0, 1)
    stops = np.array([[0, 0, 128], [0, 160, 255], [255, 255, 0], [220, 0, 0]])
    positions = np.linspace(0, 1, len(stops))
    image = np.empty((*heights.shape, 3), dtype=np.uint8)
    for channel in range(3):
        image[..., channel] = np.interp(normalized, positions, stops[:, channel])
    image[~valid] = [80, 80, 80]
    return image


def main():
    if len(sys.argv) > 2:
        raise SystemExit(__doc__)
    pg.setConfigOptions(imageAxisOrder="row-major")
    pg.mkQApp("ABE_Requiem Inspector")
    if len(sys.argv) == 1:
        input_path, _ = QFileDialog.getOpenFileName(
            None, "点群データを選択", "", "Point data (*.*)"
        )
        if not input_path:
            return
    else:
        input_path = sys.argv[1]

    heights, origin_x, origin_z, types, missing_mask = load_points(input_path)
    source_name = "C++ MapData"

    window = QMainWindow()

    window.setWindowTitle("ABE_Requiem 総合ビューア")
    central = QWidget()
    layout = QHBoxLayout(central)
    window.setCentralWidget(central)

    left = QVBoxLayout()
    output_selector = QComboBox()
    output_selector.addItems(["分類", "標高", "ヒストグラム", "欠損点表示"])
    left.addWidget(output_selector)
    stack = QStackedWidget()
    left.addWidget(stack, 1)
    layout.addLayout(left, 1)

    graphics = pg.GraphicsLayoutWidget()
    view = graphics.addViewBox()
    view.setAspectLocked(True)
    view.invertY(True)
    view.setMouseMode(pg.ViewBox.PanMode)
    image = pg.ImageItem(classification_image(types), axisOrder="row-major")
    view.addItem(image)
    view.autoRange()
    stack.addWidget(graphics)

    elevation_graphics = pg.GraphicsLayoutWidget()
    elevation_view = elevation_graphics.addViewBox()
    elevation_view.setAspectLocked(True)
    elevation_view.invertY(True)
    elevation_view.setMouseMode(pg.ViewBox.PanMode)
    elevation_image_item = pg.ImageItem(elevation_image(heights), axisOrder="row-major")
    elevation_view.addItem(elevation_image_item)
    elevation_view.autoRange()
    stack.addWidget(elevation_graphics)

    histogram = pg.PlotWidget(title="標高分布 (1m間隔)")
    histogram.setLabel("bottom", "標高", units="m")
    histogram.setLabel("left", "点数")
    histogram.showGrid(x=True, y=True, alpha=0.25)
    values = heights[np.isfinite(heights)]
    if values.size:
        lower = np.floor(np.min(values))
        upper = np.ceil(np.max(values))
        edges = np.arange(lower, upper + 1.0, 1.0)
        if edges.size < 2:
            edges = np.array([lower, lower + 1.0])
        counts, edges = np.histogram(values, bins=edges)
        bars = pg.BarGraphItem(
            x=edges[:-1] + np.diff(edges) / 2,
            height=counts,
            width=np.diff(edges) * 0.9,
            brush=(80, 140, 220, 180),
            pen=pg.mkPen((30, 60, 100), width=1),
        )
        histogram.addItem(bars)
    stack.addWidget(histogram)

    missing_graphics = pg.GraphicsLayoutWidget()
    missing_view = missing_graphics.addViewBox()
    missing_view.setAspectLocked(True)
    missing_view.invertY(True)
    missing_image = np.where(
        missing_mask[..., None], [255, 0, 0], [245, 245, 245]
    ).astype(np.uint8)
    missing_image_item = pg.ImageItem(missing_image, axisOrder="row-major")
    missing_view.addItem(missing_image_item)
    missing_view.autoRange()
    stack.addWidget(missing_graphics)

    right = QWidget()
    right_layout = QVBoxLayout(right)
    right.setMinimumWidth(255)
    right.setMaximumWidth(290)
    right_layout.addWidget(QLabel("概要"))

    summary_form = QFormLayout()
    valid = np.isfinite(heights)
    summary_form.addRow("サイズ", QLabel(f"{heights.shape[0]} x {heights.shape[1]}"))
    summary_form.addRow("有効点", QLabel(f"{int(valid.sum()):,}"))
    summary_form.addRow("欠損点", QLabel(f"{int(missing_mask.sum()):,}"))
    summary_form.addRow("分類元", QLabel(source_name))
    if values.size:
        summary_form.addRow("標高 最小", QLabel(f"{np.min(values):.2f}"))
        summary_form.addRow("標高 最大", QLabel(f"{np.max(values):.2f}"))
        summary_form.addRow("標高 平均", QLabel(f"{np.mean(values):.2f}"))
        summary_form.addRow("標高 中央値", QLabel(f"{np.median(values):.2f}"))
    right_layout.addLayout(summary_form)
    right_layout.addWidget(QLabel("分類結果"))

    class_table = QTableWidget(len(TYPE_NAMES), 3)
    class_table.setHorizontalHeaderLabels(["分類", "点数", "割合"])
    class_table.verticalHeader().setVisible(False)
    class_table.setEditTriggers(QAbstractItemView.NoEditTriggers)
    class_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
    class_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeToContents)
    class_table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeToContents)
    for row, (value, name) in enumerate(TYPE_NAMES.items()):
        count = int(np.count_nonzero(types == value))
        class_table.setItem(row, 0, QTableWidgetItem(name))
        class_table.setItem(row, 1, QTableWidgetItem(f"{count:,}"))
        class_table.setItem(row, 2, QTableWidgetItem(f"{count / heights.size * 100:.1f}%"))
    right_layout.addWidget(class_table)
    right_layout.addStretch()
    layout.addWidget(right)
    def update_output(index):
        stack.setCurrentIndex(index)
        if index == 0:
            image.setImage(classification_image(types), autoLevels=False)
        elif index == 1:
            elevation_image_item.setImage(elevation_image(heights), autoLevels=False)
        elif index == 3:
            missing_image_item.setImage(missing_image, autoLevels=False)

    def on_mouse_moved(position, target_view, target_image):
        if not target_view.sceneBoundingRect().contains(position):
            return
        point = target_image.mapFromScene(position)
        col, row = int(np.floor(point.x())), int(np.floor(point.y()))
        if 0 <= row < heights.shape[0] and 0 <= col < heights.shape[1]:
            height = heights[row, col]
            height_text = (
                "欠損（補間値）"
                if missing_mask[row, col]
                else f"{height:.2f} m"
            )
            z = origin_z + heights.shape[0] - 1 - row
            window.statusBar().showMessage(
                f"Row: {row}  Col: {col}  x: {origin_x + col}  z: {z}  "
                f"高さ: {height_text}  分類: {TYPE_NAMES.get(int(types[row, col]), 'Unknown')}"
            )

    output_selector.currentIndexChanged.connect(update_output)
    graphics.scene().sigMouseMoved.connect(lambda position: on_mouse_moved(position, view, image))
    elevation_graphics.scene().sigMouseMoved.connect(lambda position: on_mouse_moved(position, elevation_view, elevation_image_item))
    window.resize(1100, 750)
    window.show()
    pg.exec()


if __name__ == "__main__":
    main()













