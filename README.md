# ABE_Requiem

画映1 ABE Requiem チームのリポジトリです。

## ディレクトリ構造

```text
src/  ← ソースコード
└── third_party/  ← 外部ライブラリ
    ├── argparse.hpp　← コマンドライン解析
    ├── decoterm　    ← 自作カラー出力ライブラリ(ドヤ)
    └── delaunator　  ← ドロネー三角形分割

```

## コンパイル

> [!WARNING]
> Windowsでは`g++`, `mingw32-make`, `cmake` が必要です。
>
> ```
> g++ --version
> mingw32-make --version
> cmake --version
> ```
> で確認してください。

- Windowsでは

```powershell
./build.bat
```

- Linux(POSIX)では

```sh
./build.sh
```

## 使い方

```sh
./genk --help
```
