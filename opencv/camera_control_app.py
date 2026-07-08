import argparse
import tkinter as tk
from tkinter import messagebox, ttk

import cv2


RESOLUTIONS = {
    "VGA (640 x 480)": (640, 480),
    "720p (1280 x 720)": (1280, 720),
    "1080p (1920 x 1080)": (1920, 1080),
    "4K (3840 x 2160)": (3840, 2160),
}


BACKENDS = {
    "auto": 0,
    "dshow": cv2.CAP_DSHOW,
    "msmf": cv2.CAP_MSMF,
}


MJPEG_FOURCC = cv2.VideoWriter_fourcc(*"MJPG")


CAMERA_CONTROLS = {
    "brightness": {
        "label": "Brightness",
        "prop": cv2.CAP_PROP_BRIGHTNESS,
        "default_min": 1,
        "default_max": 12,
        "initial": 6,
    },
    "exposure": {
        "label": "Exposure",
        "prop": cv2.CAP_PROP_EXPOSURE,
        "default_min": -8,
        "default_max": 0,
        "initial": -4,
    },
    "zoom": {
        "label": "Zoom",
        "prop": cv2.CAP_PROP_ZOOM,
        "default_min": 0,
        "default_max": 16,
        "initial": 0,
    },
    "focus": {
        "label": "Focus",
        "prop": cv2.CAP_PROP_FOCUS,
        "default_min": 0,
        "default_max": 1023,
        "initial": 256,
    },
}


def open_camera(index: int, backend_name: str) -> cv2.VideoCapture:
    backend = BACKENDS[backend_name]
    if backend == 0:
        cap = cv2.VideoCapture(index)
    else:
        cap = cv2.VideoCapture(index, backend)

    if cap.isOpened():
        force_mjpeg(cap)

    return cap


def force_mjpeg(cap: cv2.VideoCapture) -> bool:
    return cap.set(cv2.CAP_PROP_FOURCC, MJPEG_FOURCC)


def get_fourcc(cap: cv2.VideoCapture) -> str:
    value = int(cap.get(cv2.CAP_PROP_FOURCC))
    chars = [chr((value >> 8 * index) & 0xFF) for index in range(4)]
    return "".join(chars).strip()


def probe_camera(index: int, backend_name: str) -> dict:
    cap = open_camera(index, backend_name)
    opened = cap.isOpened()
    frame_ok = False
    width = 0
    height = 0

    if opened:
        frame_ok, frame = cap.read()
        if frame_ok:
            height, width = frame.shape[:2]

    cap.release()
    return {
        "index": index,
        "backend": backend_name,
        "opened": opened,
        "frame_ok": frame_ok,
        "width": width,
        "height": height,
    }


def list_cameras(max_index: int, backend_name: str):
    for index in range(max_index + 1):
        result = probe_camera(index, backend_name)
        size = f"{result['width']}x{result['height']}" if result["frame_ok"] else "-"
        print(
            f"camera={result['index']} backend={result['backend']} "
            f"opened={result['opened']} frame={result['frame_ok']} size={size}"
        )


class CameraControlApp:
    def __init__(self, root: tk.Tk, camera_index: int, backend_name: str):
        self.root = root
        self.root.title("OpenCV Camera Control")
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

        self.cap = open_camera(camera_index, backend_name)
        if not self.cap.isOpened():
            messagebox.showerror(
                "Camera Error",
                f"Camera {camera_index} could not be opened with backend '{backend_name}'.",
            )
            self.root.destroy()
            return

        self.camera_index = camera_index
        self.backend_name = backend_name
        self.running = True
        self.photo = None
        self.control_vars = {}
        self.value_labels = {}
        self.camera_var = tk.StringVar(value=self.format_camera_label(camera_index))
        self.camera_options = [camera_index]
        self.status_var = tk.StringVar(value=f"Camera {camera_index} opened with backend {backend_name}")
        self.autofocus_var = tk.BooleanVar(value=bool(round(self.cap.get(cv2.CAP_PROP_AUTOFOCUS))))
        self.resolution_var = tk.StringVar(value=self.detect_current_resolution_label())

        self.build_ui()
        self.refresh_camera_list()
        self.sync_controls_from_camera()
        self.apply_autofocus_state()
        self.update_frame()

    def build_ui(self):
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)

        main = ttk.Frame(self.root, padding=10)
        main.grid(row=0, column=0, sticky="nsew")
        main.columnconfigure(0, weight=1)
        main.columnconfigure(1, weight=0)
        main.rowconfigure(0, weight=1)

        preview_frame = ttk.Frame(main, width=960, height=540)
        preview_frame.grid(row=0, column=0, sticky="nsew", padx=(0, 10))
        preview_frame.grid_propagate(False)
        preview_frame.columnconfigure(0, weight=1)
        preview_frame.rowconfigure(0, weight=1)

        self.preview = ttk.Label(preview_frame, anchor="center", background="black")
        self.preview.grid(row=0, column=0, sticky="nsew")

        panel = ttk.Frame(main, width=320)
        panel.grid(row=0, column=1, sticky="ns")
        panel.columnconfigure(0, weight=1)

        camera_frame = ttk.LabelFrame(panel, text="Camera", padding=8)
        camera_frame.grid(row=0, column=0, sticky="ew", pady=(0, 8))
        camera_frame.columnconfigure(0, weight=1)

        self.camera_combo = ttk.Combobox(
            camera_frame,
            textvariable=self.camera_var,
            values=[self.format_camera_label(self.camera_index)],
            state="readonly",
        )
        self.camera_combo.grid(row=0, column=0, sticky="ew", padx=(0, 6))
        self.camera_combo.bind("<<ComboboxSelected>>", self.on_camera_changed)

        ttk.Button(camera_frame, text="Scan", command=self.refresh_camera_list).grid(
            row=0, column=1, sticky="e"
        )

        resolution_frame = ttk.LabelFrame(panel, text="Resolution", padding=8)
        resolution_frame.grid(row=1, column=0, sticky="ew", pady=(0, 8))
        resolution_frame.columnconfigure(0, weight=1)

        resolution_combo = ttk.Combobox(
            resolution_frame,
            textvariable=self.resolution_var,
            values=list(RESOLUTIONS.keys()),
            state="readonly",
        )
        resolution_combo.grid(row=0, column=0, sticky="ew")
        resolution_combo.bind("<<ComboboxSelected>>", self.on_resolution_changed)

        autofocus_frame = ttk.LabelFrame(panel, text="Autofocus", padding=8)
        autofocus_frame.grid(row=2, column=0, sticky="ew", pady=(0, 8))
        autofocus_frame.columnconfigure(0, weight=1)

        self.autofocus_check = ttk.Checkbutton(
            autofocus_frame,
            text="Enable autofocus",
            variable=self.autofocus_var,
            command=self.on_autofocus_changed,
        )
        self.autofocus_check.grid(row=0, column=0, sticky="w")

        controls_frame = ttk.LabelFrame(panel, text="Camera Controls", padding=8)
        controls_frame.grid(row=3, column=0, sticky="ew")
        controls_frame.columnconfigure(1, weight=1)

        for row, (key, config) in enumerate(CAMERA_CONTROLS.items()):
            ttk.Label(controls_frame, text=config["label"]).grid(row=row, column=0, sticky="w", padx=(0, 8))

            var = tk.DoubleVar(value=config["initial"])
            scale = ttk.Scale(
                controls_frame,
                from_=config["default_min"],
                to=config["default_max"],
                orient="horizontal",
                variable=var,
                command=lambda _value, control_key=key: self.on_control_changed(control_key),
            )
            scale.grid(row=row, column=1, sticky="ew", pady=4)

            value_label = ttk.Label(controls_frame, text="--", width=12, anchor="e")
            value_label.grid(row=row, column=2, sticky="e", padx=(8, 0))

            self.control_vars[key] = {"var": var, "scale": scale}
            self.value_labels[key] = value_label

        action_frame = ttk.Frame(panel)
        action_frame.grid(row=4, column=0, sticky="ew", pady=(10, 0))
        action_frame.columnconfigure(0, weight=1)
        action_frame.columnconfigure(1, weight=1)

        ttk.Button(action_frame, text="Refresh", command=self.sync_controls_from_camera).grid(
            row=0, column=0, sticky="ew", padx=(0, 4)
        )
        ttk.Button(action_frame, text="Apply", command=self.apply_all_controls).grid(
            row=0, column=1, sticky="ew", padx=(4, 0)
        )

        status = ttk.Label(panel, textvariable=self.status_var, wraplength=300)
        status.grid(row=5, column=0, sticky="ew", pady=(12, 0))

    def refresh_camera_list(self):
        found = []
        for index in range(6):
            if index == self.camera_index:
                found.append(index)
                continue

            result = probe_camera(index, self.backend_name)
            if result["opened"] and result["frame_ok"]:
                found.append(index)

        if not found:
            found = [self.camera_index]

        self.camera_options = sorted(set(found))
        values = [self.format_camera_label(index) for index in self.camera_options]
        self.camera_combo.configure(values=values)
        self.camera_var.set(self.format_camera_label(self.camera_index))
        self.update_status(f"Detected cameras: {', '.join(str(index) for index in self.camera_options)}")

    def on_camera_changed(self, _event=None):
        selected_index = self.parse_camera_label(self.camera_var.get())
        if selected_index is None or selected_index == self.camera_index:
            return

        previous_index = self.camera_index
        previous_cap = self.cap
        previous_cap.release()

        next_cap = open_camera(selected_index, self.backend_name)
        if not next_cap.isOpened():
            self.cap = open_camera(previous_index, self.backend_name)
            self.camera_var.set(self.format_camera_label(previous_index))
            messagebox.showerror("Camera Error", f"Camera {selected_index} could not be opened.")
            self.update_status(f"Failed to switch to camera {selected_index}")
            return

        ok, _frame = next_cap.read()
        if not ok:
            next_cap.release()
            self.cap = open_camera(previous_index, self.backend_name)
            self.camera_var.set(self.format_camera_label(previous_index))
            messagebox.showerror("Camera Error", f"Camera {selected_index} opened but could not read frames.")
            self.update_status(f"Failed to read from camera {selected_index}")
            return

        self.cap = next_cap
        self.camera_index = selected_index
        self.resolution_var.set(self.detect_current_resolution_label())
        self.sync_controls_from_camera()
        self.update_status(f"Switched to camera {selected_index}, format {get_fourcc(self.cap)}")

    def detect_current_resolution_label(self) -> str:
        width = int(round(self.cap.get(cv2.CAP_PROP_FRAME_WIDTH)))
        height = int(round(self.cap.get(cv2.CAP_PROP_FRAME_HEIGHT)))
        for label, size in RESOLUTIONS.items():
            if size == (width, height):
                return label
        return "VGA (640 x 480)"

    def sync_controls_from_camera(self):
        for key, config in CAMERA_CONTROLS.items():
            current = self.cap.get(config["prop"])
            if current < 0 and key != "exposure":
                current = config["initial"]
            self.control_vars[key]["var"].set(current)
            self.value_labels[key].configure(text=self.format_value(current))

        self.autofocus_var.set(bool(round(self.cap.get(cv2.CAP_PROP_AUTOFOCUS))))
        self.apply_autofocus_state()
        self.update_status("Read current camera values")

    def on_resolution_changed(self, _event=None):
        label = self.resolution_var.get()
        width, height = RESOLUTIONS[label]

        force_mjpeg(self.cap)
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        force_mjpeg(self.cap)

        actual_width = int(round(self.cap.get(cv2.CAP_PROP_FRAME_WIDTH)))
        actual_height = int(round(self.cap.get(cv2.CAP_PROP_FRAME_HEIGHT)))
        self.update_status(
            f"Resolution requested {width}x{height}, actual {actual_width}x{actual_height}, "
            f"format {get_fourcc(self.cap)}"
        )

    def on_autofocus_changed(self):
        requested = 1 if self.autofocus_var.get() else 0
        ok = self.cap.set(cv2.CAP_PROP_AUTOFOCUS, requested)
        actual = self.cap.get(cv2.CAP_PROP_AUTOFOCUS)
        self.autofocus_var.set(bool(round(actual)))
        self.apply_autofocus_state()
        self.update_status(f"Autofocus requested {requested}, actual {self.format_value(actual)}, set={ok}")

    def on_control_changed(self, key: str):
        if key == "focus" and self.autofocus_var.get():
            return

        config = CAMERA_CONTROLS[key]
        requested = self.control_vars[key]["var"].get()
        ok = self.cap.set(config["prop"], requested)
        actual = self.cap.get(config["prop"])
        self.value_labels[key].configure(text=self.format_value(actual))
        self.update_status(f"{config['label']} requested {self.format_value(requested)}, actual {self.format_value(actual)}, set={ok}")

    def apply_all_controls(self):
        for key in CAMERA_CONTROLS:
            self.on_control_changed(key)

    def apply_autofocus_state(self):
        state = "disabled" if self.autofocus_var.get() else "normal"
        self.control_vars["focus"]["scale"].configure(state=state)

    def update_frame(self):
        if not self.running:
            return

        try:
            ok, frame = self.cap.read()
            if ok:
                frame = self.fit_frame_to_preview(frame)
                frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                height, width = frame.shape[:2]
                ppm = b"P6\n%d %d\n255\n" % (width, height) + frame.tobytes()
                self.photo = tk.PhotoImage(data=ppm, format="PPM")
                self.preview.configure(image=self.photo)
            else:
                self.update_status(
                    f"Frame read failed. camera={self.camera_index}, backend={self.backend_name}"
                )
        except tk.TclError as error:
            self.update_status(f"Preview update failed: {error}")
        except cv2.error as error:
            self.update_status(f"OpenCV frame update failed: {error}")

        self.root.after(15, self.update_frame)

    def fit_frame_to_preview(self, frame):
        max_width = max(self.preview.winfo_width(), 1)
        max_height = max(self.preview.winfo_height(), 1)
        height, width = frame.shape[:2]
        scale = min(max_width / width, max_height / height, 1.0)

        if scale < 1.0:
            frame = cv2.resize(frame, (int(width * scale), int(height * scale)), interpolation=cv2.INTER_AREA)

        return frame

    def update_status(self, text: str):
        self.status_var.set(text)

    @staticmethod
    def format_camera_label(index: int) -> str:
        return f"Camera {index}"

    @staticmethod
    def parse_camera_label(label: str):
        try:
            return int(label.replace("Camera", "").strip())
        except ValueError:
            return None

    @staticmethod
    def format_value(value: float) -> str:
        return f"{value:.2f}"

    def on_close(self):
        self.running = False
        if self.cap.isOpened():
            self.cap.release()
        self.root.destroy()


def parse_args():
    parser = argparse.ArgumentParser(description="OpenCV camera control GUI")
    parser.add_argument("--camera", type=int, default=0, help="Camera index. Default: 0")
    parser.add_argument(
        "--backend",
        choices=BACKENDS.keys(),
        default="dshow",
        help="OpenCV capture backend. Default: dshow",
    )
    parser.add_argument(
        "--list-cameras",
        action="store_true",
        help="Probe camera indexes and print whether each can read a frame.",
    )
    parser.add_argument(
        "--max-camera",
        type=int,
        default=5,
        help="Highest camera index to probe with --list-cameras. Default: 5",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    if args.list_cameras:
        list_cameras(args.max_camera, args.backend)
        return

    root = tk.Tk()
    CameraControlApp(root, args.camera, args.backend)
    root.mainloop()


if __name__ == "__main__":
    main()
