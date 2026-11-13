"""
Usage: visualizer.py <MapDataのデータファイル>
"""

import sys
import napari
import numpy as np

with open(sys.argv[1]) as f:
    rows, cols = map(int, f.readline().split())
    data = np.loadtxt(f, dtype=np.uint8)

data = data.reshape(rows, cols)

# 値と色の対応
colors = {
    1: "blue",
    2: "red",
    3: "green",
}

viewer = napari.Viewer()

# 0を白色で表示するための背景
viewer.add_image(
    np.full(data.shape, 255, dtype=np.uint8),
    name="Background",
    colormap="gray",
    contrast_limits=(0, 255),
)

# 整数値による色分け
viewer.add_labels(
    data,
    name="MapData Types",
    colormap=colors,
    opacity=1.0,
)

napari.run()
