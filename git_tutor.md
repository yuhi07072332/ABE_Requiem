# Git/Github チュートリエル

Gitの基本的な概念と使い方をまとめた資料です  

## Gitの基本概念

このあとのgitの操作が理解できるように、`commit`と`branch`を頭に入れといてください

- **Git**: ファイルの変更履歴を記録・追跡するためのツール
- **Github**: Gitの仕組みを利用して、ネット上でコードを共有・公開できるウェブサービス

### Commitとは？

⇛ ファイルの変更を**履歴として保存すること**  
Gitはリポジトリに対して一連の`commit`を保存しています。  

```text
時間 →
main   ●------------------●--------------------●-------------------●      ......
     「初期状態」　　  「機能Aを追加」　     「バグBを修正」        「機能Cを追加」


(それぞれの ● が commit です)
```
- それぞれの`commit`では**commitメッセージ**、**変更した内容**などが保存してある
- 過去の状態を確認したり、必要に応じて戻したりできる

> 特に、共同開発では誰が何をやったのかが明白になるため、管理が楽になります

### Branchとは?

⇛ **枝分かれした変更履歴**  

Gitでは、変更履歴を途中から分けて作業できます。
この分かれた履歴を **branch（ブランチ）** と呼びます。

```
                            　　　↓自分の作業をcommitしていく

　　　       自分のbranch    D---------E---------F
                        　 /　← new branch
main   A---------B--------C--------D'  ......
```

#### なぜbranchを使うの？

⇛ **それぞれ別々で同時に作業ができるようになるため**  

```text
                         D----------E----------F    木判定アルゴリズム
                        /
main A--------B--------C
                        \            
                         D'---------E'  バグ修正
```

なお、最終的に複数のbranchのcommitをmainに統合(merge)することになりますが、
そこはリポジトリ管理者に任せてください

```text
                         D----------E----------F
                        /                       \  ← merge
main A--------B--------C--------------D''--------E''
                        \            /  ← merge
                         D'---------E'
```

## GitとGithubを使った共同開発の流れ

```
    Step0: リポジトリをGithubからcloneする
      ↓
[新たに機能作成 / コードの修正をしたい]
    Step1: 新しいbranchを作る
      ↓
    Step2: 作業する
    Step3: 変更をcommitする
    Step4: Githubにpushする
      ↓ (変更がまだある場合はStep2に戻る)
    Step5: Pull Requestを行う
      ↓
    Step1に戻る

```

> やり方の説明はすべてコマンドライン上での説明ですが、VSCodeとかのGitツールでも同じことが出来ます。
> しかし、最初ではむしろコマンドでやる方が簡単な場合がありますので、慣れてからVSCodeなどを使うことをおすすめします。

### Step0: リポジトリをGithubからcloneする

今回では
```sh
git clone https://github.com/yuhi07072332/ABE_Requiem.git
```

これで、ディレクトリにABE_Requiemのフォルダが作成されて、中にはGithub上にある全てのデータが入っている

つぎに、リポジトリのフォルダに移動してください

```sh
cd ABE_Requiem
```

### Step1: 新しいbranchを作る

まず`main`ブランチに移動する  
> すでに`main`ブランチにいれば無視してください

```sh
git switch main
```

次に、最新の`main`から新しいbranchを作成したいので、ローカルの`main`ブランチをリモート(Github)から更新する  
> すでに`main`ブランチが最新であれば大丈夫です

```sh
git pull
```
> もしエラーが出た場合は管理者に聞いてください

そして、新しいブランチを作ります

```sh
git switch -c <新しいブランチ名>
```

これによって`main`から新しいブランチを作成し、その新しいブランチに移動します。
ブランチ名は**作成したい機能/修正したいbug**などにするとわかりやすいです。

### Step2: 作業する

ファイルを編集してください

### Step3: 変更をcommitする

```
ファイルを編集
    ↓
git add (commitするファイルを選択)
    ↓
git commit
    ↓
履歴として保存
```

#### (1) git addでcommitしたいファイルを選択

```sh
git add <ファイル名>
```

変更したファイルを **stage に追加**します。
(stageとは **commit待ちの場所**)

> ```
> **例**  
> 変更したファイル
> ├─ main.cpp   ← commitしたい
> ├─ README.md  ← commitしたい
> └─ test.cpp   ← まだcommitしたくない
> ```
> なら、
> ```sh
> git add main.cpp README.md
> ```
> とすると、`main.cpp`と`README.md`だけが次のcommitの対象になります

- すべての変更したファイルを選択したいときは`git add .`
- `git status`でaddしたファイル/変更されたファイルなどを確認できます

#### (2) git commitで変更をコミット

stage に入っている変更をコミットします

```sh
git commit -m "<変更内容を記載>"
```

> これで`main.cpp`と`README.md`の変更が新しいcommitに保存されます
> `git log --graph`または`git log --graph --oneline`でcommit履歴を確認できます

### Step4: Githubにpushする

新しいbranchで初回:
```sh
git push -u origin HEAD
```

2回目以降:

```sh
git push
```

これで自分のbranchでの変更がGithubに反映されます
(Github上で、自分のbranchを選択することで反映されてることがチェックできます)

> 今までの操作はすべてローカルでの変更で、このステップではじめてリモートを変更します

### Step5: Pull Requestを行う

ここからはGithub上で操作します。
Pull RequestとはGithub上で「**この branch の変更を main に取り込んでください**」と依頼する機能

やり方は[こちら](https://docs.github.com/ja/pull-requests/how-tos/create-pull-requests/creating-a-pull-request)

> [!WARNING] 
> `git pull`と`Pull Request`は別物です

