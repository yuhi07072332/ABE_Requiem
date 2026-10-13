# ABE_Requiem

画映1 ABE Requiem チームのリポジトリです。

## ディレクトリ構造

```text
src/  ← ソースコード
└── third_party/  ← 外部ライブラリ
    ├── argparse.hpp　← コマンドライン解析
    ├── decoterm/　    ← 自作カラー出力ライブラリ(ドヤ)
    └── delaunator/　  ← ドロネー三角形分割

```

## コンパイル

> [!WARNING]
> Windows環境では`g++`(バージョン16.1.0以上), `mingw32-make`, `cmake` が必要です。
>
> ```sh
> g++ --version
> mingw32-make --version
> cmake --version
> ```
>
> で確認してください。
> `cmake`をインストールするには:
> ```sh
> winget install CMake
> ```

- Windowsでは

```powershell
./build.bat
```

- Linux(POSIX)では

```sh
./build.sh
```

> [!NOTE]
> ビルドするときに`build`ディレクトリが作成されますが、特に気にする必要はないです。
> コンパイルされた実行ファイルは`build/`下ではなく現在のディレクトリに生成されます。

## 使い方

```sh
./genk --help
```

## 注意点

- `src/`下の全ての`.cpp`ファイルはコンパイルの対象となるので、テストなどの用途で
臨時的なcppファイルを作成したい場合は`src/`下に置かないでください
