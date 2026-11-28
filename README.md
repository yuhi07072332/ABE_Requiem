# ABE_Requiem

画映1 ABE Requiem チームのリポジトリ

[Gitチュートリアル](/git_tutor.md)  
[コーディングマニュアル](/manual.md)  

## ディレクトリ構造

```text
src/  ← ソースコード
third_party/  ← 外部ライブラリ
   ├── argparse.hpp　← コマンドライン解析
   ├── decoterm.hpp  ← 自作カラー出力ライブラリ(ドヤ)
   └── CDT/　        ← (制約付き)ドロネー三角形分割
tools/      ← inspectorなどの`genk`が必要なツール

```

## コンパイル

> [!WARNING]
> `g++`(バージョン16以上), `mingw32-make`(Windows), `cmake` が必要です。

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

## ツール

### Inspector

genkでのアルゴリズム実行結果を図式化するプログラム(genk内で起動)

#### 使い方

必要な依頼項をインストール: 

```
pip install -r tools/requirement.txt
```

起動:

```
./genk inspect <Args...>
```

### Launcher

genkのGUIラッパー

> [!NOTE]
> tkinterがpython環境に必要です

#### 使い方

```
./tools/launcher.py
```

## 注意点

- `src/`下の全ての`.cpp`ファイルは自動的にコンパイルの対象となるので、テストなどの用途で
臨時的なcppファイルを作成したい場合は`src/`下に置かないでください


