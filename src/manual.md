# マニュアル

## 目次

- [ファイル構造](#ファイル構造)  
- [コマンド](#コマンド)
    - [新しいコマンドを登録する](#新しいコマンドを登録する)
    - [testコマンド](#testコマンド)

## ファイル構造

### 外殻

```text
cli.hpp
cli.cpp
main.cpp
```

プログラムの外殻部分（Command Line Interface)  
`main`関数がコマンドライン引数を解析し、それに基づいて**コア部分**の関数を呼び出す  
エラーメッセージやヘルプメッセージの出力なども行う

### コア

```text
genk.hpp
algorithm.cpp
input.cpp
vrml.cpp
(その他)
```

データ入力、データ処理、VRML出力などの**コアロジック**が含まれている**最も重要な部分**  

## コマンド

一つのプログラム(`genk`)に対し、**複数の機能**(高さ度数表を表示、地面生成、...)を入れたいので、
これらを**コマンド**で区別します。   
`genk`を実行するときにコマンド名を指定することで、それに対応した関数だけが実行されます。  
(イメージとして`main`関数が複数存在する感じです)

```sh
./genk report-heights ...   # 高さ度数表を表示
./genk test-ground    ...   # 地面判定アルゴリズムをテスト
...
```

> [!NOTE]
> コマンド一覧は`genk --help`で確認できます。  

### 新しいコマンドを登録する

`main`関数の最初に

```cpp
add_command(コマンド名, コマンドの説明, 実行する関数)
```
を追加することでコマンドを登録できます。  

> [!WARNING]
> `実行する関数`は`const Option&`を受け取り、`void`を返す関数でないといけない

これによって、`run_command()`は登録されたコマンドから、
`argv`で指定されたコマンドに対応する関数を実行します。  

**例**

`algorithm.cpp`で新たなアルゴリズム`find_building(const MapData& data)`を実装し、それをテストしたい  
⇛ **新たなコマンド `test-building`を登録する**

`main.cpp`に`test-building`用の関数を追加:

```cpp
void test_building(const Option& opt) {
    MapData data = read_map(opt.input_file);
    find_building(data);
    /* ...... */
}
```

`main`関数でこれを登録する:

```cpp
int main() {
    /* ... */
    add_command("test-building", "建物判定アルゴリズムをテスト", test_building);
}
```

コンパイルして実行:

```sh
./genk test-building ...
```

### testコマンド

流石に毎回何かしらをテストするために新しいコマンドを登録するのは面倒なので、  
`main.cpp`では`test`コマンドが事前に登録されています  

なにか一時的に実行したいものがあるときは`test()`関数を編集して、`./genk test ...`で実行してください

## 複数ファイルでのC++プログラミング

### なぜヘッダファイルが必要なのか

コンパイルの流れ:

```text

main.cpp  input.cpp  vrml.cpp ....
   ↓          ↓         ↓　　コンパイル
  main.o  input.o    vrml.o   ....
        \     |     /
         \    |    /
          \   |   /  リンク
           \  |  /
            genk
```

つまり、それぞれの`.cpp`は別々にコンパイルされ、最後のリンク段階でリンカーによって1つの実行可能ファイルを出力する  
リンクによって、例えば`main.cpp`は`vrml.cpp`の中にある`write_elevation_grid()`を使うことが出来る  
(1つの`.cpp`は別の`.cpp`で定義された内容が使える)  

しかし、これだけではうまく行きません。なぜなら:  

コンパイル段階では`.cpp`ごとに**別々**にコンパイルされる  
⇛ main.cpp をコンパイルするとき、コンパイラーは**他の.cppの関数の存在を知らない**  
⇛ コンパイルエラー  

**解決方法: コンパイラーに関数の存在を知らせる**  

例えば, `main.cpp`の最初に

```cpp
void write_elevation_grid(std::ostream& os, const MapData& data);
```

のような**関数宣言**を追加することで、コンパイラーに  
`"このような関数が存在します(定義は違う場所にあるかもしれないけど)"`  
ということを伝えることができる。これを`main.cpp`内の全ての外部関数/クラスに行うことで、コンパイルが成功する  

ただし、これを`input.cpp`, `vrml.cpp` ... 全部にやる必要があるためかなり面倒  
**⇛ こういうものを全てヘッダファイル(.hpp)にまとめて、`.cpp`でそれをincludeする**

> スタンダードライブラリでも同じで、例えば`stdio.h`には`printf()`の宣言(定義ではない)が書いてあって、
> cpp内に`#include <stdio.h>`をすることで`printf()`が使えるようになります

### genk.hppに関数を公開する

このプロジェクトでは、簡単なために`genk.hpp`にほとんどの関数/クラス/その他宣言を乗せる  

**例**  
`algorithm.cpp`に新たな関数を実装:

```cpp
std::vector<Point> find_building(const MapData& data) {
    /* ..... */
}
```

それを他の`.cpp`に**公開**したい場合, `genk.hpp`にその宣言を追加する:

```cpp
std::vector<Point> find_building(const MapData& data);
```

これによって他の`genk.hpp`をincludeした`.cpp`は`find_building()`関数が使えるようになる

### 公開しない関数は`static`をつける

**例**  

`max2()`は`algorithm.hpp`内部でのみ使う関数 ⇛ **前頭に`static`をつける**

```cpp
static std::pair<int, int> max2(const std::map<int, int>& data) {
    /* ...... */
}
```

これによって**他の`vrml.cpp`で別の`max2()`関数を定義してもエラーになりません**

### 新たな.cppファイルを追加する

基本的に`src/`下の`.cpp`ファイルは全て`genk`のコンパイル対象なので、目的に応じて新しい`.cpp`を作成してもいい

全てのアルゴリズムを全部`algorithm.cpp`に入れるとごちゃごちゃになるかもしれない  
⇛ **1つのアルゴリズムにつき違う`.cpp`に置く**と良いかもしれません  

**例**

(ここに欠損点補間のアルゴリズムを新たなcppに分割する説明を入れる
1. 新.cpp下でアルゴリズムをまとめる
2. genk.hppに公開したい関数の宣言を乗せる
)

