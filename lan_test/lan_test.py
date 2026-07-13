import socket
import sys
import threading
import time
import tkinter as tk
from pathlib import Path
from tkinter import messagebox, ttk


COMMANDS = {
    "電源オフ": "7F 08 99 A2 B3 C4 02 FF 01 01 CF",
    "電源オン": "7F 08 99 A2 B3 C4 02 FF 01 00 CF",
    "HDMI1画面切替": "7F 08 99 A2 B3 C4 02 FF 01 0A CF",
    "TYPE-C画面切替": "7F 08 99 A2 B3 C4 02 FF 01 54 CF",
    "OPS画面切替": "7F 08 99 A2 B3 C4 02 FF 01 38 CF",
    "Home画面切替": "7F 08 99 A2 B3 C4 02 FF 01 1C CF",
}


def resource_path(filename):
    if hasattr(sys, "_MEIPASS"):
        return Path(sys._MEIPASS) / filename
    return Path(__file__).resolve().parent / filename


class LanCommandTester:
    def __init__(self, root):
        self.root = root
        self.root.title("LANテストアプリ")
        self.root.geometry("520x560")
        self.root.minsize(460, 500)

        self.is_testing = False
        self.worker_thread = None

        self._build_ui()

    def _build_ui(self):
        main = ttk.Frame(self.root, padding=16)
        main.pack(fill=tk.BOTH, expand=True)

        connection_frame = ttk.LabelFrame(main, text="接続先")
        connection_frame.pack(fill=tk.X)
        connection_frame.columnconfigure(1, weight=1)
        connection_frame.columnconfigure(2, weight=0)

        ttk.Label(connection_frame, text="IPアドレス").grid(row=0, column=0, padx=8, pady=8, sticky=tk.W)
        self.ip_entry = ttk.Entry(connection_frame)
        self.ip_entry.insert(0, "192.168.1.100")
        self.ip_entry.grid(row=0, column=1, padx=8, pady=8, sticky=tk.EW)

        ttk.Label(connection_frame, text="ポート").grid(row=1, column=0, padx=8, pady=8, sticky=tk.W)
        self.port_entry = ttk.Entry(connection_frame, width=12)
        self.port_entry.insert(0, "8000")
        self.port_entry.grid(row=1, column=1, padx=8, pady=8, sticky=tk.W)

        ttk.Label(connection_frame, text="方式").grid(row=2, column=0, padx=8, pady=8, sticky=tk.W)
        self.protocol_var = tk.StringVar(value="UDP")
        protocol = ttk.Combobox(
            connection_frame,
            textvariable=self.protocol_var,
            values=("UDP", "TCP"),
            width=10,
            state="readonly",
        )
        protocol.grid(row=2, column=1, padx=8, pady=8, sticky=tk.W)

        command_frame = ttk.LabelFrame(main, text="手動送信")
        command_frame.pack(fill=tk.X, pady=(14, 0))
        command_frame.columnconfigure(0, weight=1)
        command_frame.columnconfigure(1, weight=1)

        for index, command_name in enumerate(COMMANDS):
            button = ttk.Button(
                command_frame,
                text=command_name,
                command=lambda name=command_name: self.send_command(name),
            )
            button.grid(
                row=index // 2,
                column=index % 2,
                padx=8,
                pady=8,
                sticky=tk.EW,
            )

        auto_frame = ttk.LabelFrame(main, text="起動試験")
        auto_frame.pack(fill=tk.X, pady=(14, 0))
        auto_frame.columnconfigure(1, weight=1)

        ttk.Label(auto_frame, text="間隔(秒)").grid(row=0, column=0, padx=8, pady=8, sticky=tk.W)
        self.interval_entry = ttk.Entry(auto_frame, width=12)
        self.interval_entry.insert(0, "60")
        self.interval_entry.grid(row=0, column=1, padx=8, pady=8, sticky=tk.W)

        self.test_button = ttk.Button(auto_frame, text="起動試験開始", command=self.toggle_test)
        self.test_button.grid(row=0, column=2, padx=8, pady=8)

        log_frame = ttk.LabelFrame(main, text="ログ")
        log_frame.pack(fill=tk.BOTH, expand=True, pady=(14, 0))
        log_frame.rowconfigure(0, weight=1)
        log_frame.columnconfigure(0, weight=1)

        self.log_text = tk.Text(log_frame, height=12, wrap=tk.WORD)
        self.log_text.grid(row=0, column=0, sticky=tk.NSEW)

        scrollbar = ttk.Scrollbar(log_frame, orient=tk.VERTICAL, command=self.log_text.yview)
        scrollbar.grid(row=0, column=1, sticky=tk.NS)
        self.log_text.configure(yscrollcommand=scrollbar.set)

    def validate_connection_settings(self):
        ip = self.ip_entry.get().strip()
        if not ip:
            messagebox.showerror("入力エラー", "IPアドレスを入力してください。")
            return None

        try:
            port = int(self.port_entry.get().strip())
        except ValueError:
            messagebox.showerror("入力エラー", "ポート番号は数値で入力してください。")
            return None

        if not 1 <= port <= 65535:
            messagebox.showerror("入力エラー", "ポート番号は 1 から 65535 の範囲で入力してください。")
            return None

        return ip, port, self.protocol_var.get()

    def log(self, message):
        timestamp = time.strftime("%H:%M:%S")
        self.root.after(0, self._append_log, f"[{timestamp}] {message}\n")

    def _append_log(self, message):
        self.log_text.insert(tk.END, message)
        self.log_text.see(tk.END)

    def send_command(self, command_name):
        settings = self.validate_connection_settings()
        if settings is None:
            return

        command_hex = COMMANDS[command_name]
        command_bytes = bytes.fromhex(command_hex)
        ip, port, protocol = settings

        threading.Thread(
            target=self._send_command_worker,
            args=(ip, port, protocol, command_name, command_hex, command_bytes),
            daemon=True,
        ).start()

    def _send_command_worker(self, ip, port, protocol, command_name, command_hex, command_bytes):
        try:
            if protocol == "TCP":
                with socket.create_connection((ip, port), timeout=3.0) as sock:
                    sock.sendall(command_bytes)
            else:
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
                    sock.settimeout(3.0)
                    sock.sendto(command_bytes, (ip, port))

            self.log(f"{command_name} 送信成功: {command_hex}")
        except OSError as exc:
            self.log(f"{command_name} 送信失敗: {exc}")

    def toggle_test(self):
        if self.is_testing:
            self.is_testing = False
            self.test_button.configure(text="起動試験開始")
            self.log("起動試験を停止しました。")
            return

        try:
            interval = int(self.interval_entry.get().strip())
        except ValueError:
            messagebox.showerror("入力エラー", "間隔は秒数で入力してください。")
            return

        if interval < 1:
            messagebox.showerror("入力エラー", "間隔は1秒以上で入力してください。")
            return

        if self.validate_connection_settings() is None:
            return

        self.is_testing = True
        self.test_button.configure(text="起動試験停止")
        self.log("起動試験を開始しました。")
        self.worker_thread = threading.Thread(target=self._auto_test_loop, args=(interval,), daemon=True)
        self.worker_thread.start()

    def _auto_test_loop(self, interval):
        command_order = ("電源オン", "電源オフ")
        index = 0

        while self.is_testing:
            self.send_command(command_order[index])
            index = 1 - index

            for _ in range(interval):
                if not self.is_testing:
                    return
                time.sleep(1)


if __name__ == "__main__":
    root = tk.Tk()
    app = LanCommandTester(root)
    root.mainloop()
