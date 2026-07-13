import json
import math
from datetime import datetime
from pathlib import Path
import tkinter as tk


TRANSPARENT_KEY = "#ff00ff"
CONFIG_PATH = Path(__file__).with_name("transparent_clock_settings.json")


class TransparentClockApp:
    def __init__(self) -> None:
        self.root = tk.Tk()
        self.root.title("Transparent Clock")
        self.root.overrideredirect(True)
        self.root.attributes("-topmost", True)
        self.root.configure(bg=TRANSPARENT_KEY)
        self.root.wm_attributes("-transparentcolor", TRANSPARENT_KEY)

        self.settings = {
            "size": 300,
            "window_opacity": 80,
            "show_date": True,
            "always_on_top": True,
        }
        self._load_settings()

        self.drag_offset_x = 0
        self.drag_offset_y = 0
        self.center = 0
        self.radius = 0
        self.topmost_var = tk.BooleanVar(value=self.settings["always_on_top"])
        self.show_date_var = tk.BooleanVar(value=self.settings["show_date"])

        self.canvas = tk.Canvas(
            self.root,
            width=self.settings["size"],
            height=self.settings["size"],
            bg=TRANSPARENT_KEY,
            bd=0,
            highlightthickness=0,
        )
        self.canvas.pack()

        self.menu = tk.Menu(self.root, tearoff=False)
        self._build_menu()
        self._bind_events()
        self._apply_window_state()
        self._set_initial_position()
        self._update_clock()

    def _load_settings(self) -> None:
        if not CONFIG_PATH.exists():
            return

        try:
            stored = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            return

        for key in self.settings:
            if key in stored:
                self.settings[key] = stored[key]

    def _save_settings(self) -> None:
        try:
            CONFIG_PATH.write_text(
                json.dumps(self.settings, ensure_ascii=False, indent=2),
                encoding="utf-8",
            )
        except OSError:
            pass

    def _bind_events(self) -> None:
        self.canvas.bind("<ButtonPress-1>", self._start_drag)
        self.canvas.bind("<B1-Motion>", self._on_drag)
        self.canvas.bind("<Button-3>", self._show_menu)
        self.canvas.bind("<MouseWheel>", self._resize_with_wheel)

    def _build_menu(self) -> None:
        self.menu.delete(0, tk.END)

        opacity_menu = tk.Menu(self.menu, tearoff=False)
        for percent in (100, 80, 60, 40, 20):
            opacity_menu.add_command(
                label=f"{percent}%",
                command=lambda value=percent: self._set_window_opacity(value),
            )

        size_menu = tk.Menu(self.menu, tearoff=False)
        for size in (220, 260, 300, 360, 420):
            size_menu.add_command(
                label=f"{size}px",
                command=lambda value=size: self._set_size(value),
            )

        self.menu.add_checkbutton(
            label="常に手前に表示",
            onvalue=True,
            offvalue=False,
            variable=self.topmost_var,
            command=self._toggle_topmost,
        )
        self.menu.add_checkbutton(
            label="日付を表示",
            onvalue=True,
            offvalue=False,
            variable=self.show_date_var,
            command=self._toggle_date,
        )
        self.menu.add_cascade(label="不透明度", menu=opacity_menu)
        self.menu.add_cascade(label="サイズ", menu=size_menu)
        self.menu.add_separator()
        self.menu.add_command(label="終了", command=self.root.destroy)

    def _apply_window_state(self) -> None:
        self.root.attributes("-topmost", self.settings["always_on_top"])
        self.root.attributes("-alpha", self.settings["window_opacity"] / 100)
        size = self.settings["size"]
        self.canvas.configure(width=size, height=size)
        self.root.geometry(f"{size}x{size}")
        self._save_settings()

    def _set_initial_position(self) -> None:
        size = self.settings["size"]
        screen_width = self.root.winfo_screenwidth()
        screen_height = self.root.winfo_screenheight()
        x = max(40, screen_width - size - 80)
        y = max(40, screen_height // 8)
        self.root.geometry(f"{size}x{size}+{x}+{y}")

    def _show_menu(self, event: tk.Event) -> None:
        self._build_menu()
        try:
            self.menu.tk_popup(event.x_root, event.y_root)
        finally:
            self.menu.grab_release()

    def _toggle_topmost(self) -> None:
        self.settings["always_on_top"] = bool(self.topmost_var.get())
        self._apply_window_state()

    def _toggle_date(self) -> None:
        self.settings["show_date"] = bool(self.show_date_var.get())
        self._save_settings()

    def _set_window_opacity(self, percent: int) -> None:
        self.settings["window_opacity"] = percent
        self._apply_window_state()

    def _set_size(self, size: int) -> None:
        self.settings["size"] = size
        current_x = self.root.winfo_x()
        current_y = self.root.winfo_y()
        self._apply_window_state()
        self.root.geometry(f"{size}x{size}+{current_x}+{current_y}")

    def _resize_with_wheel(self, event: tk.Event) -> None:
        step = 20 if event.delta > 0 else -20
        new_size = min(420, max(220, self.settings["size"] + step))
        if new_size != self.settings["size"]:
            self._set_size(new_size)

    def _start_drag(self, event: tk.Event) -> None:
        self.drag_offset_x = event.x_root - self.root.winfo_x()
        self.drag_offset_y = event.y_root - self.root.winfo_y()

    def _on_drag(self, event: tk.Event) -> None:
        x = event.x_root - self.drag_offset_x
        y = event.y_root - self.drag_offset_y
        self.root.geometry(f"+{x}+{y}")

    def _draw_dial(self) -> None:
        size = self.settings["size"]
        self.center = size / 2
        self.radius = size * 0.44
        inset = size * 0.06

        self.canvas.create_oval(
            inset,
            inset,
            size - inset,
            size - inset,
            fill="#16222e",
            outline="#d8e3ec",
            width=max(2, size // 75),
        )

        inner_inset = inset + size * 0.06
        self.canvas.create_oval(
            inner_inset,
            inner_inset,
            size - inner_inset,
            size - inner_inset,
            outline="#4c6278",
            width=max(1, size // 120),
        )

        for i in range(60):
            angle = math.radians(i * 6 - 90)
            outer = self.radius * 0.96
            inner = self.radius * (0.80 if i % 5 == 0 else 0.88)
            width = max(1, size // 110) if i % 5 else max(2, size // 90)
            color = "#f2f6fa" if i % 5 == 0 else "#6f869c"
            x1 = self.center + math.cos(angle) * inner
            y1 = self.center + math.sin(angle) * inner
            x2 = self.center + math.cos(angle) * outer
            y2 = self.center + math.sin(angle) * outer
            self.canvas.create_line(x1, y1, x2, y2, fill=color, width=width)

        for label, deg in (("12", -90), ("3", 0), ("6", 90), ("9", 180)):
            angle = math.radians(deg)
            distance = self.radius * 0.64
            x = self.center + math.cos(angle) * distance
            y = self.center + math.sin(angle) * distance
            self.canvas.create_text(
                x,
                y,
                text=label,
                fill="#f7fbff",
                font=("Yu Gothic UI", max(14, size // 14), "bold"),
            )

    def _draw_hand(self, angle_deg: float, length_ratio: float, width_ratio: int, color: str) -> None:
        angle = math.radians(angle_deg - 90)
        x = self.center + math.cos(angle) * self.radius * length_ratio
        y = self.center + math.sin(angle) * self.radius * length_ratio
        self.canvas.create_line(
            self.center,
            self.center,
            x,
            y,
            fill=color,
            width=max(2, self.settings["size"] // width_ratio),
            capstyle=tk.ROUND,
        )

    def _update_clock(self) -> None:
        now = datetime.now()
        self.canvas.delete("all")
        self._draw_dial()

        hour = (now.hour % 12) + now.minute / 60
        minute = now.minute + now.second / 60
        second = now.second + now.microsecond / 1_000_000

        self._draw_hand(hour * 30, 0.50, 42, "#f6fafc")
        self._draw_hand(minute * 6, 0.72, 70, "#9fd3ff")
        self._draw_hand(second * 6, 0.82, 120, "#ff6b6b")

        center_size = self.settings["size"] * 0.028
        self.canvas.create_oval(
            self.center - center_size,
            self.center - center_size,
            self.center + center_size,
            self.center + center_size,
            fill="#f6fafc",
            outline="",
        )

        if self.settings["show_date"]:
            self.canvas.create_text(
                self.center,
                self.center + self.radius * 0.34,
                text=now.strftime("%Y-%m-%d"),
                fill="#dce7f0",
                font=("Yu Gothic UI", max(11, self.settings["size"] // 24), "bold"),
            )

        self.root.after(100, self._update_clock)

    def run(self) -> None:
        self.root.mainloop()


if __name__ == "__main__":
    TransparentClockApp().run()
