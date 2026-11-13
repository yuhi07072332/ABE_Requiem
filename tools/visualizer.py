"""
Usage: visualizer.py <MapDataのデータファイル>
"""

import matplotlib.pyplot as plt
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap, BoundaryNorm

data = np.loadtxt(sys.argv[1], dtype=np.uint8)

# 値ごとの色
cmap = ListedColormap(["white", "blue", "red"])

fig, ax = plt.subplots(figsize=(10, 10))

im = ax.imshow(
    data,
    cmap=cmap,
    vmin=-0.5,
    vmax=3.5,
    interpolation="nearest"
)

# カラーバー
fig.colorbar(im, ax=ax, ticks=[0, 1, 2])

plt.show()
