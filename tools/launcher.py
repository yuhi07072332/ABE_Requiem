# python ./tools/launcher.py
from pathlib import Path
import subprocess
import threading
import tkinter as tk
from tkinter import scrolledtext, ttk, filedialog
import os

ROOT_DIR = Path(__file__).resolve().parent.parent
BUILD_BAT = ROOT_DIR / "build.bat"
BUILD_DIR = ROOT_DIR / "build"

COMMAND_CANDIDATES = [
    "all",
    "dry-run",
    "show-heights",
    "show-classify",
    "show-buildings",
    "report-heights",
    "gen-points",
    "inspect",
    "test",
]

MAP_CANDIDATES = [
    "",
    "53394610_dsm_1m.dat",
    "53394611_dsm_1m.dat",
    "53394620_dsm_1m.dat",
    "53394621_dsm_1m.dat",
    "53394630_dsm_1m.dat",
    "53394631_dsm_1m.dat",
    "53394640_dsm_1m.dat",
    "53394641_dsm_1m.dat",
]

def browse_directory() -> None:
    """フォルダ選択ダイアログを開き、選んだパスを -d 入力欄に入れる"""
    # 今入力欄に何か入っていれば、その場所を初期フォルダにする
    current = dir_entry.get().strip()
    initial_dir = current if Path(current).is_dir() else str(ROOT_DIR)

    selected_dir = filedialog.askdirectory(
        title="読み込むディレクトリを選択",
        initialdir=initial_dir,
    )

    # キャンセルされずにフォルダが選ばれた場合だけ反映する
    if selected_dir:
        dir_entry.delete(0, tk.END)
        dir_entry.insert(0, selected_dir)

def build_extra_args() -> list[str]:
    """画面のオプション欄から、genk に渡す追加引数のリストを作る"""
    args: list[str] = []

    # 1. -d / --directory
    directory = dir_entry.get().strip()
    if directory:
        args.extend(["-d", directory])

    # 2. -o（出力ファイル名）
    output_file = out_entry.get().strip()
    if output_file:
        args.extend(["-o", output_file])

    # 3. --smooth-ground <N>
    smooth_ground = smooth_ground_entry.get().strip()
    if smooth_ground:
        args.extend(["--smooth-ground", smooth_ground])

    # 4. --smooth-building <N>
    smooth_building = smooth_building_entry.get().strip()
    if smooth_building:
        args.extend(["--smooth-building", smooth_building])

    # 5. -a / --append（チェックボックス）
    if append_var.get():
        args.append("--append")

    # 6. --colorful-buildings（チェックボックス）
    if colorful_var.get():
        args.append("--colorful-buildings")

    # 7. -h / --help（チェックボックス）
    if help_var.get():
        args.append("--help")

    # 既存の引数欄（.dat ファイル名など）も後ろにつなげる
    manual_args = args_entry.get().strip().split()
    args.extend(manual_args)

    return args

current_proc: subprocess.Popen | None = None
proc_lock = threading.Lock()


def stop_running() -> None:
    """実行中のプロセスを強制停止する"""
    with proc_lock:
        proc = current_proc

    if proc is None or proc.poll() is not None:
        append_log("実行中のコマンドはありません")
        return

    append_log("停止要求を送信しました...")
    if os.name == "nt":
        # Windows: 子プロセス（ビルドツールや genk.exe）も含めてツリーごと終了
        subprocess.run(
            ["taskkill", "/F", "/T", "/PID", str(proc.pid)],
            capture_output=True,
            text=True,
            errors="replace",
        )
    else:
        proc.terminate()


def find_genk_executable() -> Path | None:
    candidates = [
        BUILD_DIR / "genk.exe",
        BUILD_DIR / "genk",
        ROOT_DIR / "genk.exe",
        ROOT_DIR / "genk",
    ]
    for path in candidates:
        if path.exists():
            return path
    return None


def append_log(message: str) -> None:
    """どのスレッドから呼ばれても安全にログ欄へ追記する"""
    def _write() -> None:
        log_area.configure(state="normal")
        log_area.insert(tk.END, message + "\n")
        log_area.see(tk.END)
        log_area.configure(state="disabled")

    root.after(0, _write)


def set_running_state(running: bool) -> None:
    """実行中は実行ボタンを無効化し、停止ボタンを有効化する"""
    def _apply() -> None:
        run_btn.configure(state="disabled" if running else "normal")
        stop_btn.configure(state="normal" if running else "disabled")

    root.after(0, _apply)



def clear_log() -> None:
    """ログ表示欄の中身をすべて消去する"""
    log_area.configure(state="normal")
    log_area.delete("1.0", tk.END)
    log_area.configure(state="disabled")


def run_command_with_log(cmd_list: list[str]) -> int:
    """外部コマンドを実行し、出力を1行ずつリアルタイムにログ欄へ流す"""
    global current_proc
    append_log("$ " + " ".join(cmd_list))

    proc = subprocess.Popen(
        cmd_list,
        cwd=ROOT_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,
    )

    with proc_lock:
        current_proc = proc

    try:
        if proc.stdout is not None:
            for line in proc.stdout:
                append_log(line.rstrip("\r\n"))
        return proc.wait()
    finally:
        with proc_lock:
            if current_proc is proc:
                current_proc = None


def worker_run(cmd: str, extra_args: list[str], should_build: bool) -> None:
    """裏方スレッドで動く処理"""
    try:
        if should_build:
            append_log("ビルドを開始します...")
            bld_code = run_command_with_log(["cmd", "/c", str(BUILD_BAT)])
            if bld_code != 0:
                append_log(f"ビルド失敗 (終了コード: {bld_code})")
                return
            append_log("ビルド成功")
        else:
            append_log("ビルドをスキップします")

        genk_exe = find_genk_executable()
        if genk_exe is None:
            append_log("genk の実行ファイルが見つかりませんでした")
            return

        full_cmd = [str(genk_exe), cmd, *extra_args]
        code = run_command_with_log(full_cmd)
        append_log(f"完了 (終了コード: {code})\n" + "-" * 40)
    finally:
        # 成功でも途中終了でも、最後に必ず実行ボタンを押せるように戻す
        set_running_state(False)


def start_run() -> None:
    """ボタンを押した瞬間に呼ばれる"""
    cmd = cmd_combo.get().strip()
    extra_args = build_extra_args()
    should_build = build_var.get()

    if not cmd:
        append_log("コマンド名を入力または選択してください")
        return

    set_running_state(True)
    thread = threading.Thread(
        target=worker_run,
        args=(cmd, extra_args, should_build),
        daemon=True,
    )
    thread.start()


root = tk.Tk()
root.title("genk ランチャー")
root.geometry("560x640")

# 1. コマンド選択欄
tk.Label(root, text="コマンド:").pack(anchor="w", padx=20, pady=(12, 0))
cmd_combo = ttk.Combobox(root, values=COMMAND_CANDIDATES)
cmd_combo.set(COMMAND_CANDIDATES[0])
cmd_combo.pack(fill="x", padx=20)

# 2. 引数入力欄
tk.Label(root, text="引数（任意・スペース区切り）:").pack(anchor="w", padx=20, pady=(10, 0))
args_entry = ttk.Combobox(root, values=MAP_CANDIDATES)
args_entry.set(MAP_CANDIDATES[0])
args_entry.pack(fill="x", padx=20)

# 3. ビルド実行の切り替えチェックボックス
build_var = tk.BooleanVar(value=True)
build_check = tk.Checkbutton(root, text="実行前にビルドする", variable=build_var)
build_check.pack(anchor="w", padx=20, pady=(8, 0))

# 4. 実行ボタン・停止ボタン・ログクリアボタン
btn_frame = tk.Frame(root)
btn_frame.pack(pady=10)

run_btn = tk.Button(btn_frame, text="実行", command=start_run, width=10)
run_btn.pack(side="left", padx=6)

stop_btn = tk.Button(btn_frame, text="停止", command=stop_running, width=10, state="disabled")
stop_btn.pack(side="left", padx=6)

clear_btn = tk.Button(btn_frame, text="ログクリア", command=clear_log, width=10)
clear_btn.pack(side="left", padx=6)

# オプションをまとめる枠
opt_frame = tk.LabelFrame(root, text="オプション", padx=12, pady=8)
opt_frame.pack(fill="x", padx=20, pady=(10, 0))

# -d ディレクトリ指定（入力欄 + 参照ボタン）
tk.Label(opt_frame, text="-d (ディレクトリ):").grid(row=0, column=0, sticky="w", pady=2)

dir_entry = tk.Entry(opt_frame)
dir_entry.grid(row=0, column=1, sticky="ew", padx=(6, 0), pady=2)

browse_btn = tk.Button(opt_frame, text="参照", command=browse_directory, width=6)
browse_btn.grid(row=0, column=2, padx=(6, 0), pady=2)

# -o 出力ファイル名
tk.Label(opt_frame, text="-o (出力ファイル):").grid(row=1, column=0, sticky="w", pady=2)
out_entry = tk.Entry(opt_frame)
out_entry.grid(row=1, column=1, sticky="ew", padx=(6, 0), pady=2)

# --smooth-ground
tk.Label(opt_frame, text="--smooth-ground:").grid(row=2, column=0, sticky="w", pady=2)
smooth_ground_entry = tk.Entry(opt_frame, width=10)
smooth_ground_entry.insert(0, "16")  # デフォルト値 16 を入れておく
smooth_ground_entry.grid(row=2, column=1, sticky="w", padx=(6, 0), pady=2)

# --smooth-building
tk.Label(opt_frame, text="--smooth-building:").grid(row=3, column=0, sticky="w", pady=2)
smooth_building_entry = tk.Entry(opt_frame, width=10)
smooth_building_entry.insert(0, "16")  # デフォルト値 16 を入れておく
smooth_building_entry.grid(row=3, column=1, sticky="w", padx=(6, 0), pady=2)

# 列の幅を伸ばせるように設定
opt_frame.columnconfigure(1, weight=1)

# フラグ系オプション（チェックボックス）
append_var = tk.BooleanVar(value=False)
tk.Checkbutton(opt_frame, text="--append (-a)", variable=append_var).grid(
    row=4, column=0, sticky="w", pady=(4, 0)
)

colorful_var = tk.BooleanVar(value=False)
tk.Checkbutton(opt_frame, text="--colorful-buildings", variable=colorful_var).grid(
    row=4, column=1, sticky="w", pady=(4, 0)
)

help_var = tk.BooleanVar(value=False)
tk.Checkbutton(opt_frame, text="--help (-h)", variable=help_var).grid(
    row=5, column=0, sticky="w", pady=(2, 0)
)

# 5. 出力ログ表示欄
tk.Label(root, text="実行ログ:").pack(anchor="w", padx=20)
log_area = scrolledtext.ScrolledText(root, height=12, state="disabled", wrap="word")
log_area.pack(fill="both", expand=True, padx=20, pady=(0, 16))



root.mainloop()