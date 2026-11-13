"""
Usage: visualizer.py <MapDataのデータファイル>
"""
import matplotlib.pyplot as plt
import numpy as np
import sys
from matplotlib.colors import ListedColormap

with open(sys.argv[1]) as f:
    rows, cols = map(int, f.readline().split())
    data = np.loadtxt(f, dtype=np.uint8)

data = data.reshape(rows, cols)

cmap = ListedColormap(["white", "blue", "red", "green"])

fig, ax = plt.subplots(figsize=(10, 10))

im = ax.imshow(
    data,
    cmap=cmap,
    vmin=-0.5,
    vmax=3.5,
    interpolation="nearest"
)

fig.colorbar(im, ax=ax, ticks=[0, 1, 2, 3])

plt.show()
