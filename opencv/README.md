# OpenCV Camera Control

OpenCV と Tkinter を使った最小のカメラ制御アプリです。

## Controls

- Brightness: `CAP_PROP_BRIGHTNESS`
- Exposure: `CAP_PROP_EXPOSURE`
- Zoom: `CAP_PROP_ZOOM`
- Focus: `CAP_PROP_FOCUS`
- Autofocus: `CAP_PROP_AUTOFOCUS`
- Format: MJPEG via `CAP_PROP_FOURCC`
- Resolution:
  - VGA: `640 x 480`
  - 720p: `1280 x 720`
  - 1080p: `1920 x 1080`
  - 4K: `3840 x 2160`

Current slider ranges:

- Brightness: `1` to `12`
- Zoom: `0` to `16`
- Focus: `0` to `1023`
- Exposure: `-8` to `0`

## Setup

```powershell
python -m pip install opencv-python
```

Tkinter is included with standard Python distributions on Windows.

## Run

```powershell
python camera_control_app.py
```

The app scans camera indexes `0` to `5` and shows readable cameras in the Camera selector.
Use the Scan button after connecting or disconnecting cameras.

Use another camera index when needed:

```powershell
python camera_control_app.py --camera 1
```

Probe available camera indexes:

```powershell
python camera_control_app.py --list-cameras
```

Try another Windows backend:

```powershell
python camera_control_app.py --backend msmf
```

## Notes

Camera properties depend on the camera model, OS, driver, and OpenCV backend.
Some controls may report a value but not actually change the camera behavior.
The app shows the requested value and the actual value reported by OpenCV after each change.
The app requests MJPEG (`MJPG`) and does not expose YUV format selection.

If the window opens but no image appears, check Windows camera privacy settings:

- Settings > Privacy & security > Camera
- Enable camera access
- Enable desktop apps camera access

Also close other apps that may be using the camera, such as Teams, Zoom, Camera, or a browser tab.
