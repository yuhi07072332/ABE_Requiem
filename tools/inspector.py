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


TYPE_NAMES = {0: "Unknown", 1: "Ground", 2: "Building", 3: "BuildingOutline", 4: "Other"}
TYPE_COLORS = np.array(
    [[255, 255, 0], [0, 0, 255], [255, 0, 0], [0, 180, 0], [255, 255, 255]], dtype=np.uint8
)


# データの読み込み
def load_points(path):
    with open(path, encoding="utf-8-sig") as source:
        first_line = source.readline().split()
        if (
            len(first_line) != 2
            or first_line[0] != "MAPDATA"
            or first_line[1] != "1"
        ):
            raise ValueError(f"{path}: MAPDATA 1が必要です")

        header = source.readline().split()
        if len(header) < 4:
            raise ValueError(f"{path}: ヘッダーが不正です")

        rows, cols, origin_x, origin_z = map(int, header[:4])

        data_rows = []
        smoothing = None

        for line in source:
            parts = line.split()
            if not parts:
                continue

            if parts[0] == "REDUCED_MAP":
                break

            if parts[0] == "SMOOTHING":
                if len(parts) != 5:
                    raise ValueError(f"{path}: SMOOTHING行が不正です")
                smoothing = tuple(map(int, parts[1:]))
                continue

            if len(parts) not in (2, 3):
                raise ValueError(f"{path}: MAPDATAの列数が不正です")
            data_rows.append([float(value) for value in parts])

        if len(data_rows) != rows * cols:
            raise ValueError(
                f"{path}: データ件数が不正です"
            )

        values = np.asarray(data_rows, dtype=float)
        heights = values[:, 0].reshape(rows, cols)
        embedded_types = values[:, 1].astype(np.uint8).reshape(rows, cols)

        missing = (
            values[:, 2].astype(bool).reshape(rows, cols)
            if values.shape[1] == 3
            else ~np.isfinite(heights)
        )

        if smoothing is None:
            smoothing = (0, 0, 0, 0)

        return (
            heights,
            origin_x,
            origin_z,
            embedded_types,
            missing,
            *smoothing,
        )


def load_reduced_points(path):
    with open(path, encoding="utf-8-sig") as source:
        for line in source:
            parts = line.split()
            if parts[:2] == ["REDUCED_MAP", "1"]:
                break
        else:
            return None

        header = source.readline().split()
        if len(header) < 4:
            raise ValueError(f"{path}: 削減後MAPDATAヘッダーが不正です")

        rows, cols, _origin_x, _origin_z = map(int, header[:4])
        data_rows = []
        for line in source:
            parts = line.split()
            if not parts:
                continue
            if len(parts) not in (2, 3):
                raise ValueError(f"{path}: 削減後MAPDATAの列数が不正です")
            data_rows.append([float(value) for value in parts])

        if len(data_rows) != rows * cols:
            raise ValueError(f"{path}: 削減後データ件数が不正です")

        values = np.asarray(data_rows, dtype=float)
        heights = values[:, 0].reshape(rows, cols)
        types = values[:, 1].astype(np.uint8).reshape(rows, cols)
        missing = (
            values[:, 2].astype(bool).reshape(rows, cols)
            if values.shape[1] == 3
            else ~np.isfinite(heights)
        )
        return heights, types, missing


# 分類画像の生成
def classification_image(types):
    return TYPE_COLORS[np.clip(types, 0, len(TYPE_COLORS) - 1)]


def reduced_classification_image(types, heights):
    image = classification_image(types).copy()
    image[~np.isfinite(heights)] = [80, 80, 80]
    return image



# 標高画像の生成
def elevation_image(heights):
    # 例外処理: 有効な標高値が1つもない場合はグレー画像を返す
    valid = np.isfinite(heights)
    if not valid.any():
        return np.zeros((*heights.shape, 3), dtype=np.uint8)

    # 有効な標高値を抽出
    values = heights[valid]

    # 極端な値を除外して正規化
    low, high = np.percentile(values, [5, 95])
    normalized = np.clip(np.nan_to_num((heights - low) / max(high - low, 1e-12)), 0, 1)

    # カラーマップの生成
    stops = np.array([[0, 0, 128], [0, 160, 255], [255, 255, 0], [220, 0, 0]])
    positions = np.linspace(0, 1, len(stops))

    # 標高値をカラーマップに変換
    image = np.empty((*heights.shape, 3), dtype=np.uint8)
    for channel in range(3):
        image[..., channel] = np.interp(normalized, positions, stops[:, channel])

    # 欠損点をグレーで表示
    image[~valid] = [80, 80, 80]

    return image



def main():
    # コマンドライン引数の処理
    if len(sys.argv) > 2:
        raise SystemExit(__doc__)

    # PyQtGraphの設定とアプリケーションの初期化
    pg.setConfigOptions(imageAxisOrder="row-major")
    pg.mkQApp("ABE_Requiem Inspector")

    # ファイル選択ダイアログの表示
    if len(sys.argv) == 1:
        input_path, _ = QFileDialog.getOpenFileName(
            None, "点群データを選択", "", "Point data (*.*)"
        )
        if not input_path:
            return
    else:
        input_path = sys.argv[1]
    
    # データの読み込み
    (
        heights, origin_x, origin_z, types, missing_mask,
        ground_before, ground_erased, building_before, building_erased
    ) = load_points(input_path)
    reduced_data = load_reduced_points(input_path)
    if reduced_data is None:
        reduced_heights = None
        reduced_types = None
    else:
        reduced_heights, reduced_types, _reduced_missing = reduced_data

    source_name = "MapData"

    # GUIの構築
    window = QMainWindow()

    # ウィンドウタイトルの設定
    window.setWindowTitle("Omnispect 2.1 Requiem")
    central = QWidget()
    layout = QHBoxLayout(central)
    window.setCentralWidget(central)

    # 左側のレイアウトと出力切り替え用のコンボボックス
    left = QVBoxLayout()
    output_selector = QComboBox()
    output_selector.addItems([
        "分類（削減前）",
        "分類（削減後）",
        "標高（削減前）",
        "標高（削減後）",
        "欠損点表示",
        "ヒストグラム",
    ])
    left.addWidget(output_selector)
    stack = QStackedWidget()
    left.addWidget(stack, 1)
    layout.addLayout(left, 1)

    # 分類ビュー（削減前）
    graphics = pg.GraphicsLayoutWidget()
    view = graphics.addViewBox()
    view.setAspectLocked(True)
    view.invertY(True)
    view.setMouseMode(pg.ViewBox.PanMode)
    image = pg.ImageItem(classification_image(types), axisOrder="row-major")
    view.addItem(image)
    view.autoRange()
    stack.addWidget(graphics)

    # 分類ビュー（削減後）
    reduced_graphics = pg.GraphicsLayoutWidget()
    reduced_view = reduced_graphics.addViewBox()
    reduced_view.setAspectLocked(True)
    reduced_view.invertY(True)
    reduced_view.setMouseMode(pg.ViewBox.PanMode)
    if reduced_heights is None:
        reduced_classification = classification_image(types)
        reduced_status = "削減後マップを読み込めません"
    else:
        reduced_classification = reduced_classification_image(reduced_types, reduced_heights)
        reduced_status = None
    reduced_image = pg.ImageItem(reduced_classification, axisOrder="row-major")
    reduced_view.addItem(reduced_image)
    reduced_view.autoRange()
    stack.addWidget(reduced_graphics)

    if reduced_status is not None:
        message = pg.TextItem(reduced_status, anchor=(0.5, 0.5))
        message.setPos(reduced_view.viewRange()[0][0], reduced_view.viewRange()[1][0])
        reduced_view.addItem(message)

    # 標高ビュー
    elevation_graphics = pg.GraphicsLayoutWidget()
    elevation_view = elevation_graphics.addViewBox()
    elevation_view.setAspectLocked(True)
    elevation_view.invertY(True)
    elevation_view.setMouseMode(pg.ViewBox.PanMode)
    elevation_image_item = pg.ImageItem(elevation_image(heights), axisOrder="row-major")
    elevation_view.addItem(elevation_image_item)
    elevation_view.autoRange()
    stack.addWidget(elevation_graphics)

    # 標高ビュー（削減後）
    reduced_elevation_graphics = pg.GraphicsLayoutWidget()
    reduced_elevation_view = reduced_elevation_graphics.addViewBox()
    reduced_elevation_view.setAspectLocked(True)
    reduced_elevation_view.invertY(True)
    reduced_elevation_view.setMouseMode(pg.ViewBox.PanMode)
    if reduced_heights is None:
        reduced_elevation = elevation_image(heights)
    else:
        reduced_elevation = elevation_image(reduced_heights)
    reduced_elevation_image_item = pg.ImageItem(
        reduced_elevation,
        axisOrder="row-major",
    )
    reduced_elevation_view.addItem(reduced_elevation_image_item)
    reduced_elevation_view.autoRange()
    stack.addWidget(reduced_elevation_graphics)

    # 欠損点ビュー
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

    # ヒストグラムビュー
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

    # 削減率の計算
    def reduction_rate(before, erased):
        if before <= 0:
            return 0.0
        return erased / before * 100

    ground_rate = reduction_rate(ground_before, ground_erased)
    building_rate = reduction_rate(building_before, building_erased)

    overall_before = ground_before + building_before
    overall_erased = ground_erased + building_erased
    overall_rate = reduction_rate(overall_before, overall_erased)

    # 右側のレイアウトと概要表示
    right = QWidget()
    right_layout = QVBoxLayout(right)
    right.setMinimumWidth(255)
    right.setMaximumWidth(290)
    right_layout.addWidget(QLabel("概要"))

    # 右側の概要表示の構築
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

    # 分類結果のテーブル表示
    class_table = QTableWidget(len(TYPE_NAMES), 3)
    class_table.setHorizontalHeaderLabels(["分類", "点数", "割合"])
    class_table.verticalHeader().setVisible(False)
    class_table.setEditTriggers(QAbstractItemView.NoEditTriggers)
    class_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
    class_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeToContents)
    class_table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeToContents)

    # 分類ごとの点数と割合を計算してテーブルに表示
    for row, (value, name) in enumerate(TYPE_NAMES.items()):
        count = int(np.count_nonzero(types == value))
        class_table.setItem(row, 0, QTableWidgetItem(name))
        class_table.setItem(row, 1, QTableWidgetItem(f"{count:,}"))
        class_table.setItem(row, 2, QTableWidgetItem(f"{count / heights.size * 100:.1f}%"))
    right_layout.addWidget(class_table)
    
    # 削減率の表示
    right_layout.addWidget(QLabel("削減率"))

    reduction_form = QFormLayout()
    reduction_form.addRow("地面削減率", QLabel(f"{ground_rate:.1f}%"))
    reduction_form.addRow("建物削減率", QLabel(f"{building_rate:.1f}%"))
    right_layout.addLayout(reduction_form)

    overall_label = QLabel(f"全体削減率  {overall_rate:.1f}%")
    overall_label.setStyleSheet(
        "font-size: 18px; font-weight: bold; color: #b45309;"
    )
    overall_label.setAlignment(Qt.AlignCenter)
    right_layout.addWidget(overall_label)

    right_layout.addStretch()
    layout.addWidget(right)


    # 出力切り替え時の処理
    def update_output(index):
        stack.setCurrentIndex(index)
        if index == 0:
            image.setImage(classification_image(types), autoLevels=False)
        elif index == 1:
            if reduced_heights is not None:
                reduced_image.setImage(
                    reduced_classification_image(reduced_types, reduced_heights),
                    autoLevels=False,
                )
        elif index == 2:
            elevation_image_item.setImage(elevation_image(heights), autoLevels=False)
        elif index == 3:
            if reduced_heights is not None:
                reduced_elevation_image_item.setImage(
                    elevation_image(reduced_heights),
                    autoLevels=False,
                )
        elif index == 4:
            missing_image_item.setImage(missing_image, autoLevels=False)

    # ビューア上でのマウス移動時の処理
    def on_mouse_moved(
        position,
        target_view,
        target_image,
        target_heights,
        target_types,
        target_missing,
        view_mode,
    ):
        if not target_view.sceneBoundingRect().contains(position):
            return
        point = target_image.mapFromScene(position)
        col, row = int(np.floor(point.x())), int(np.floor(point.y()))
        if 0 <= row < target_heights.shape[0] and 0 <= col < target_heights.shape[1]:
            height = target_heights[row, col]
            original_missing = target_missing[row, col]
            original_height = heights[row, col]

            if view_mode == "before":
                if original_missing:
                    if np.isfinite(original_height):
                        height_text = f"元の欠損点（補間値: {original_height:.2f} m）"
                    else:
                        height_text = "元の欠損点（補間不能）"
                else:
                    height_text = f"{height:.2f} m"
            elif view_mode == "reduced":
                if not np.isfinite(height):
                    if np.isfinite(original_height):
                        height_text = f"削減済み（元の高さ: {original_height:.2f} m）"
                    else:
                        height_text = "削減済み"
                elif original_missing:
                    height_text = f"元の欠損点（補間値: {height:.2f} m）"
                else:
                    height_text = f"{height:.2f} m"
            else:
                if original_missing:
                    height_text = f"元の欠損点（補間値: {height:.2f} m）"
                elif not np.isfinite(height):
                    height_text = "削減済み"
                else:
                    height_text = f"{height:.2f} m"
            z = origin_z + heights.shape[0] - 1 - row
            window.statusBar().showMessage(
                f"Row: {row}  Col: {col}  x: {origin_x + col}  z: {z}  "
                f"高さ: {height_text}  分類: {TYPE_NAMES.get(int(target_types[row, col]), 'Unknown')}"
            )


    # イベントの接続
    output_selector.currentIndexChanged.connect(update_output)
    graphics.scene().sigMouseMoved.connect(lambda position: on_mouse_moved(
        position, view, image, heights, types, missing_mask, "before"
    ))
    reduced_graphics.scene().sigMouseMoved.connect(lambda position: on_mouse_moved(
        position,
        reduced_view,
        reduced_image,
        reduced_heights if reduced_heights is not None else heights,
        reduced_types if reduced_types is not None else types,
        missing_mask,
        "reduced",
    ))
    elevation_graphics.scene().sigMouseMoved.connect(lambda position: on_mouse_moved(
        position, elevation_view, elevation_image_item, heights, types, missing_mask, "before"
    ))
    reduced_elevation_graphics.scene().sigMouseMoved.connect(lambda position: on_mouse_moved(
        position,
        reduced_elevation_view,
        reduced_elevation_image_item,
        reduced_heights if reduced_heights is not None else heights,
        reduced_types if reduced_types is not None else types,
        missing_mask,
        "reduced",
    ))
    missing_graphics.scene().sigMouseMoved.connect(lambda position: on_mouse_moved(
        position, missing_view, missing_image_item, heights, types, missing_mask, "before"
    ))

    # メインウィンドウのサイズを設定して表示
    window.resize(1100, 750)
    window.show()
    pg.exec()


if __name__ == "__main__":
    main()
