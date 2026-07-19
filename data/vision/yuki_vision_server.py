"""
yuki_vision_server.py
Yuki_1.0 - Real-Time Vision Analysis Daemon

Architecture:
  - Reads commands from STDIN (line-by-line JSON)
  - Writes results to STDOUT (line-by-line JSON)
  - C++ ScreenRuntime / CameraRuntime launch this and talk via pipes
  - Camera preview window with face/object detection runs automatically

Commands:
  {"cmd": "screen"}        - capture and analyse screen
  {"cmd": "camera"}        - capture camera frame + rich analysis + object detection
  {"cmd": "detect_objects"}- explicit object detection with dot-matrix render + memory
  {"cmd": "recall_objects"}- retrieve recent object memory
  {"cmd": "ping"}          - health check
  {"cmd": "show_preview"}  - open live camera preview window
  {"cmd": "hide_preview"}  - close preview window
  {"cmd": "quit"}          - shutdown
"""

import sys
import json
import time
import threading
import hashlib
import os
import datetime

_cv2 = None
_np  = None
_mss = None
_PIL = None

# ── COCO class labels (MobileNet-SSD) ─────────────────────────────────────
COCO_CLASSES = [
    "background","person","bicycle","car","motorcycle","airplane","bus","train",
    "truck","boat","traffic light","fire hydrant","stop sign","parking meter","bench",
    "bird","cat","dog","horse","sheep","cow","elephant","bear","zebra","giraffe",
    "backpack","umbrella","handbag","tie","suitcase","frisbee","skis","snowboard",
    "sports ball","kite","baseball bat","baseball glove","skateboard","surfboard",
    "tennis racket","bottle","wine glass","cup","fork","knife","spoon","bowl",
    "banana","apple","sandwich","orange","broccoli","carrot","hot dog","pizza",
    "donut","cake","chair","couch","potted plant","bed","dining table","toilet",
    "tv","laptop","mouse","remote","keyboard","cell phone","microwave","oven",
    "toaster","sink","refrigerator","book","clock","vase","scissors","teddy bear",
    "hair drier","toothbrush"
]

# ── Object Memory ──────────────────────────────────────────────────────────
MEMORY_FILE   = "data/vision/object_memory.json"
_object_memory = []
_memory_lock  = threading.Lock()

def _load_memory():
    global _object_memory
    try:
        if os.path.exists(MEMORY_FILE):
            with open(MEMORY_FILE, "r", encoding="utf-8") as f:
                _object_memory = json.load(f)
    except Exception:
        _object_memory = []

def _save_memory_entry(entry):
    global _object_memory
    with _memory_lock:
        _object_memory.append(entry)
        if len(_object_memory) > 500:
            _object_memory = _object_memory[-500:]
        try:
            os.makedirs(os.path.dirname(MEMORY_FILE) or ".", exist_ok=True)
            with open(MEMORY_FILE, "w", encoding="utf-8") as f:
                json.dump(_object_memory, f, indent=2, ensure_ascii=False)
        except Exception:
            pass

# ── DNN Object Detection ──────────────────────────────────────────────────
_dnn_net  = None
_dnn_lock = threading.Lock()

MODEL_DIR      = "data/models/mobilenet_ssd"
PROTOTXT       = MODEL_DIR + "/deploy.prototxt"
CAFFEMODEL     = MODEL_DIR + "/mobilenet_iter_73000.caffemodel"
PROTOTXT_URL   = "https://raw.githubusercontent.com/chuanqi305/MobileNet-SSD/master/deploy.prototxt"
CAFFEMODEL_URL = "https://github.com/chuanqi305/MobileNet-SSD/raw/master/mobilenet_iter_73000.caffemodel"

def _ensure_model():
    os.makedirs(MODEL_DIR, exist_ok=True)
    if not os.path.exists(PROTOTXT) or os.path.getsize(PROTOTXT) < 100:
        try:
            import urllib.request
            urllib.request.urlretrieve(PROTOTXT_URL, PROTOTXT)
        except Exception:
            return False
    if not os.path.exists(CAFFEMODEL) or os.path.getsize(CAFFEMODEL) < 1_000_000:
        try:
            import urllib.request
            urllib.request.urlretrieve(CAFFEMODEL_URL, CAFFEMODEL)
        except Exception:
            return False
    return True

def _get_dnn():
    global _dnn_net
    with _dnn_lock:
        if _dnn_net is not None:
            return _dnn_net
        if _cv2 is None or not _ensure_model():
            return None
        try:
            _dnn_net = _cv2.dnn.readNetFromCaffe(PROTOTXT, CAFFEMODEL)
            return _dnn_net
        except Exception:
            return None

def detect_objects(frame):
    """Run MobileNet-SSD. Returns list of {label, confidence, position, box}."""
    net = _get_dnn()
    if net is None or _cv2 is None or _np is None:
        return []
    h, w = frame.shape[:2]
    blob = _cv2.dnn.blobFromImage(
        _cv2.resize(frame, (300, 300)), 0.007843, (300, 300), 127.5)
    net.setInput(blob)
    try:
        dets = net.forward()
    except Exception:
        return []
    results = []
    for i in range(dets.shape[2]):
        conf = float(dets[0, 0, i, 2])
        if conf < 0.45:
            continue
        idx = int(dets[0, 0, i, 1])
        if idx < 0 or idx >= len(COCO_CLASSES):
            continue
        label = COCO_CLASSES[idx]
        if label == "background":
            continue
        box = dets[0, 0, i, 3:7] * _np.array([w, h, w, h])
        x1, y1, x2, y2 = [int(v) for v in box.tolist()]
        cx = (x1 + x2) // 2
        cy = (y1 + y2) // 2
        hpos = "left" if cx < w // 3 else ("right" if cx > 2 * w // 3 else "centre")
        vpos = "top"  if cy < h // 3 else ("bottom" if cy > 2 * h // 3 else "middle")
        results.append({
            "label": str(label),
            "confidence": round(float(conf), 2),
            "position": str(hpos + "-" + vpos),
            "box": [int(x1), int(y1), int(x2), int(y2)]
        })
    return results

def render_dot_matrix(frame, objects, cols=52, rows=20):
    """Render ASCII dot-matrix of camera frame with object markers."""
    if _cv2 is None or _np is None:
        return ""
    h, w = frame.shape[:2]
    gray  = _cv2.cvtColor(frame, _cv2.COLOR_BGR2GRAY)
    small = _cv2.resize(gray, (cols, rows))
    ramp  = " .:-=+*#%@"
    grid  = []
    for row in small:
        grid.append([ramp[int(px / 256 * len(ramp))] for px in row])
    for obj in objects:
        x1, y1, x2, y2 = obj["box"]
        cx = max(0, min(cols - 1, int((x1 + x2) / 2 / w * cols)))
        cy = max(0, min(rows - 1, int((y1 + y2) / 2 / h * rows)))
        grid[cy][cx] = obj["label"][0].upper()
    return "\n".join("".join(row) for row in grid)


# ── Lazy lib loader ────────────────────────────────────────────────────────

def _load_libs():
    global _cv2, _np, _mss, _PIL
    if _cv2 is not None:
        return True
    try:
        import cv2
        import numpy as np
        import mss as mss_lib
        from PIL import Image
        _cv2 = cv2
        _np  = np
        _mss = mss_lib
        _PIL = Image
        return True
    except ImportError as e:
        _emit({"ok": False, "error": "Vision library not available: " + str(e)})
        return False

def _emit(obj):
    line = json.dumps(obj, ensure_ascii=False)
    sys.stdout.write(line + "\n")
    sys.stdout.flush()

def _emit_error(msg):
    _emit({"ok": False, "error": msg})


# ── Screen capture ─────────────────────────────────────────────────────────

_mss_inst = None

def _get_mss():
    global _mss_inst
    if _mss_inst is None and _mss is not None:
        _mss_inst = _mss.MSS()
    return _mss_inst

def capture_screen():
    if not _load_libs():
        return {"ok": False, "error": "libs unavailable"}
    try:
        sct     = _get_mss()
        monitor = sct.monitors[0]
        img     = sct.grab(monitor)

        frame   = _np.frombuffer(img.raw, dtype=_np.uint8)
        frame   = frame.reshape((img.height, img.width, 4))
        bgr     = _cv2.cvtColor(frame, _cv2.COLOR_BGRA2BGR)
        h, w    = bgr.shape[:2]
        gray    = _cv2.cvtColor(bgr, _cv2.COLOR_BGR2GRAY)

        brightness   = float(_np.mean(gray))
        edges        = _cv2.Canny(gray, 50, 150)
        edge_density = float(_np.count_nonzero(edges)) / (h * w)

        cy2, cx2 = h // 4, w // 4
        region   = _cv2.resize(bgr[cy2:cy2*3, cx2:cx2*3], (64, 64))
        pixels   = region.reshape(-1, 3).astype(_np.float32)
        _, labels, centers = _cv2.kmeans(
            pixels, 3, None,
            (_cv2.TERM_CRITERIA_EPS + _cv2.TERM_CRITERIA_MAX_ITER, 10, 1.0),
            3, _cv2.KMEANS_RANDOM_CENTERS)
        counts  = _np.bincount(labels.flatten())
        dom_bgr = centers[_np.argmax(counts)].astype(int).tolist()
        dom_hex = "#{:02X}{:02X}{:02X}".format(int(dom_bgr[2]), int(dom_bgr[1]), int(dom_bgr[0]))

        px_hash = hashlib.md5(_cv2.resize(gray, (32, 32)).tobytes()).hexdigest()[:16]

        focused_title = ""
        focused_proc  = ""
        try:
            import win32gui
            import win32process
            import psutil
            hwnd  = win32gui.GetForegroundWindow()
            focused_title = win32gui.GetWindowText(hwnd)
            _, pid = win32process.GetWindowThreadProcessId(hwnd)
            focused_proc  = psutil.Process(pid).name()
        except Exception:
            pass

        ocr_text = ""
        try:
            import pytesseract
            pil_img  = _PIL.fromarray(_cv2.cvtColor(
                bgr[h//4:3*h//4, w//4:3*w//4], _cv2.COLOR_BGR2RGB))
            ocr_text = pytesseract.image_to_string(pil_img, timeout=2).strip()[:500]
        except Exception:
            pass

        activity = ("HIGH_ACTIVITY" if edge_density > 0.15 else
                    "MODERATE_ACTIVITY" if edge_density > 0.05 else "LOW_ACTIVITY")
        activity_desc = {
            "HIGH_ACTIVITY":    "Dense interface - many elements or active media.",
            "MODERATE_ACTIVITY":"Normal desktop or document content.",
            "LOW_ACTIVITY":     "Minimal content - blank screen or simple background."
        }[activity]

        return {
            "ok": True, "type": "screen", "ts": float(time.time()),
            "data": {
                "width": int(w), "height": int(h),
                "brightness": round(float(brightness), 1),
                "edge_density": round(float(edge_density), 4),
                "activity_level": str(activity),
                "activity_description": str(activity_desc),
                "dominant_colour": str(dom_hex),
                "pixel_hash": str(px_hash),
                "focused_window_title": str(focused_title),
                "focused_process": str(focused_proc),
                "ocr_text": str(ocr_text),
                "summary": str(
                    "Screen {}x{}. {} ({:.0f}/255). {}".format(
                        w, h,
                        "Bright" if brightness > 128 else "Dark",
                        brightness, activity_desc)
                    + (" Focused: '{}' ({}).".format(focused_title, focused_proc) if focused_title else "")
                    + (" Text: \"{}...\"".format(ocr_text[:80]) if len(ocr_text) > 20 else ""))
            }
        }
    except Exception as e:
        return {"ok": False, "error": "Screen capture failed: " + str(e)}


# ── Camera ─────────────────────────────────────────────────────────────────

_cam_cap      = None
_cam_lock     = threading.Lock()
_face_cascade = None

def _get_face_cascade():
    global _face_cascade
    if _face_cascade is None and _cv2 is not None:
        _face_cascade = _cv2.CascadeClassifier(
            _cv2.data.haarcascades + "haarcascade_frontalface_default.xml")
    return _face_cascade

def _get_camera():
    global _cam_cap
    if _cam_cap is None or not _cam_cap.isOpened():
        cap = _cv2.VideoCapture(0, _cv2.CAP_DSHOW)
        if cap.isOpened():
            cap.set(_cv2.CAP_PROP_FRAME_WIDTH,  640)
            cap.set(_cv2.CAP_PROP_FRAME_HEIGHT, 480)
            cap.set(_cv2.CAP_PROP_FPS, 30)
            _cam_cap = cap
    return _cam_cap

def capture_camera():
    if not _load_libs():
        return {"ok": False, "error": "libs unavailable"}

    with _cam_lock:
        cap = _get_camera()
        if cap is None or not cap.isOpened():
            return {
                "ok": True, "type": "camera", "ts": float(time.time()),
                "data": {
                    "hardware_present": False, "width": 0, "height": 0,
                    "summary": "No camera hardware detected.",
                    "face_count": 0, "motion_detected": False,
                    "brightness": 0, "pixel_hash": "",
                    "lighting": "unknown", "face_positions": [],
                    "objects": [], "dot_matrix": ""
                }
            }

        ret, frame = cap.read()
        if not ret:
            return {"ok": False, "error": "Camera read failed"}

        h, w = frame.shape[:2]
        gray = _cv2.cvtColor(frame, _cv2.COLOR_BGR2GRAY)

        cascade    = _get_face_cascade()
        faces      = cascade.detectMultiScale(gray, 1.1, 5, minSize=(30, 30))
        face_count = int(len(faces))
        face_positions = []
        for (fx, fy, fw, fh) in faces:
            cx2, cy2 = fx + fw // 2, fy + fh // 2
            hpos = "left" if cx2 < w // 3 else ("right" if cx2 > 2 * w // 3 else "centre")
            vpos = "top"  if cy2 < h // 3 else ("bottom" if cy2 > 2 * h // 3 else "middle")
            face_positions.append(str(hpos + "-" + vpos))

        motion_detected = False
        ret2, frame2 = cap.read()
        if ret2:
            gray2  = _cv2.cvtColor(frame2, _cv2.COLOR_BGR2GRAY)
            diff   = _cv2.absdiff(gray, gray2)
            _, thr = _cv2.threshold(diff, 25, 255, _cv2.THRESH_BINARY)
            motion_detected = bool(float(_np.count_nonzero(thr)) / (h * w) > 0.02)

        brightness = float(_np.mean(gray))
        lighting   = ("very dark"   if brightness < 40  else
                      "dim lighting" if brightness < 80  else
                      "normal lighting" if brightness < 160 else
                      "bright lighting" if brightness < 200 else
                      "very bright - possibly overexposed")

        px_hash = hashlib.md5(_cv2.resize(gray, (16, 16)).tobytes()).hexdigest()[:16]

        # Object detection + dot-matrix
        objects    = detect_objects(frame)
        dot_matrix = render_dot_matrix(frame, objects)

        # Save to memory if objects found
        if objects:
            entry = {
                "timestamp": str(datetime.datetime.now().isoformat()),
                "objects": [{"label": o["label"], "confidence": o["confidence"],
                             "position": o["position"]} for o in objects],
                "lighting": str(lighting),
                "face_count": int(face_count)
            }
            _save_memory_entry(entry)

        summary_parts = [
            "Camera {}x{}. {}.".format(w, h, lighting.capitalize())
        ]
        if face_count == 0:
            summary_parts.append("No faces detected.")
        elif face_count == 1:
            summary_parts.append("1 face detected ({}).".format(face_positions[0] if face_positions else "unknown"))
        else:
            summary_parts.append("{} faces detected ({}).".format(face_count, ", ".join(face_positions)))
        if motion_detected:
            summary_parts.append("Motion detected.")
        if objects:
            obj_names = list({o["label"] for o in objects})
            summary_parts.append("Objects: {}.".format(", ".join(obj_names)))

        return {
            "ok": True, "type": "camera", "ts": float(time.time()),
            "data": {
                "hardware_present": True,
                "width": int(w), "height": int(h),
                "brightness": round(float(brightness), 1),
                "lighting": str(lighting),
                "face_count": int(face_count),
                "face_positions": [str(p) for p in face_positions],
                "motion_detected": bool(motion_detected),
                "pixel_hash": str(px_hash),
                "objects": [{"label": str(o["label"]), "confidence": float(o["confidence"]),
                             "position": str(o["position"])} for o in objects],
                "dot_matrix": str(dot_matrix),
                "summary": str(" ".join(summary_parts))
            }
        }


def do_object_detection():
    """Standalone object detection command — captures frame, runs DNN, returns dot-matrix + memory entry."""
    if not _load_libs():
        return {"ok": False, "error": "libs unavailable"}
    with _cam_lock:
        cap = _get_camera()
        if cap is None or not cap.isOpened():
            return {"ok": False, "error": "No camera hardware"}
        ret, frame = cap.read()
        if not ret:
            return {"ok": False, "error": "Camera read failed"}

    objects    = detect_objects(frame)
    dot_matrix = render_dot_matrix(frame, objects)

    if objects:
        entry = {
            "timestamp": str(datetime.datetime.now().isoformat()),
            "objects": [{"label": o["label"], "confidence": o["confidence"],
                         "position": o["position"]} for o in objects],
        }
        _save_memory_entry(entry)

    obj_names = list({o["label"] for o in objects})
    summary   = ("Detected: " + ", ".join(obj_names) + ".") if obj_names else "No objects detected."

    return {
        "ok": True, "type": "objects", "ts": float(time.time()),
        "data": {
            "objects": [{"label": str(o["label"]), "confidence": float(o["confidence"]),
                         "position": str(o["position"])} for o in objects],
            "dot_matrix": str(dot_matrix),
            "summary": str(summary),
            "model": "MobileNet-SSD COCO"
        }
    }


def recall_objects(limit=20):
    """Return recent object observations from memory."""
    with _memory_lock:
        recent = _object_memory[-limit:]
    return {
        "ok": True, "type": "memory",
        "data": {
            "count": int(len(_object_memory)),
            "recent": recent
        }
    }


# ── Live Camera Preview ────────────────────────────────────────────────────

_preview_running = False
_preview_thread  = None
_preview_lock    = threading.Lock()

def _preview_loop():
    global _preview_running
    _load_libs()
    if _cv2 is None:
        return

    cap = None
    try:
        cap = _cv2.VideoCapture(0, _cv2.CAP_DSHOW)
        if not cap.isOpened():
            _preview_running = False
            return

        cap.set(_cv2.CAP_PROP_FRAME_WIDTH,  640)
        cap.set(_cv2.CAP_PROP_FRAME_HEIGHT, 480)
        cap.set(_cv2.CAP_PROP_FPS, 30)

        cascade = _get_face_cascade()
        _cv2.namedWindow("Yuki Vision", _cv2.WINDOW_NORMAL)
        _cv2.resizeWindow("Yuki Vision", 320, 240)

        frame_count = 0
        last_objects = []

        while _preview_running:
            ret, frame = cap.read()
            if not ret:
                break

            gray  = _cv2.cvtColor(frame, _cv2.COLOR_BGR2GRAY)
            faces = cascade.detectMultiScale(gray, 1.1, 5, minSize=(30, 30))

            # Run object detection every 15 frames (non-blocking quality)
            frame_count += 1
            if frame_count % 15 == 0:
                last_objects = detect_objects(frame)

            # Draw face boxes
            for (fx, fy, fw, fh) in faces:
                _cv2.rectangle(frame, (fx, fy), (fx+fw, fy+fh), (0, 200, 180), 2)
                _cv2.putText(frame, "Face", (fx, fy - 8),
                             _cv2.FONT_HERSHEY_SIMPLEX, 0.45, (0, 200, 180), 1)

            # Draw object boxes
            for obj in last_objects:
                x1, y1, x2, y2 = obj["box"]
                _cv2.rectangle(frame, (x1, y1), (x2, y2), (255, 180, 0), 2)
                label_txt = "{} {:.0f}%".format(obj["label"], obj["confidence"] * 100)
                _cv2.putText(frame, label_txt, (x1, y1 - 6),
                             _cv2.FONT_HERSHEY_SIMPLEX, 0.38, (255, 200, 60), 1)

            # Lighting info overlay
            brightness = float(_np.mean(gray))
            lighting   = ("Very Dark" if brightness < 40 else
                          "Dim"       if brightness < 80 else
                          "Normal"    if brightness < 160 else "Bright")
            label = "  Yuki Vision  |  {} ({:.0f}/255)".format(lighting, brightness)
            _cv2.putText(frame, label, (8, frame.shape[0] - 8),
                         _cv2.FONT_HERSHEY_SIMPLEX, 0.38, (80, 200, 180), 1)

            _cv2.imshow("Yuki Vision", frame)
            key = _cv2.waitKey(1) & 0xFF
            if key == 27 or _cv2.getWindowProperty("Yuki Vision", _cv2.WND_PROP_VISIBLE) < 1:
                break

    except Exception:
        pass
    finally:
        if cap:
            cap.release()
        try:
            _cv2.destroyWindow("Yuki Vision")
        except Exception:
            pass
        _preview_running = False


def start_preview():
    global _preview_running, _preview_thread
    with _preview_lock:
        if _preview_running:
            return
        _preview_running = True
        _preview_thread  = threading.Thread(target=_preview_loop, daemon=True)
        _preview_thread.start()

def stop_preview():
    global _preview_running
    _preview_running = False


# ── Main loop ──────────────────────────────────────────────────────────────

def main():
    _load_memory()
    _emit({"ok": True, "type": "ready", "ts": float(time.time()),
           "msg": "Yuki Vision Server online"})

    # Auto-start preview and download DNN model in background
    def _init_bg():
        if _load_libs():
            cap_probe = _cv2.VideoCapture(0, _cv2.CAP_DSHOW)
            if cap_probe.isOpened():
                cap_probe.release()
                start_preview()
            # Pre-load DNN model so first detect_objects call is fast
            threading.Thread(target=_get_dnn, daemon=True).start()

    threading.Thread(target=_init_bg, daemon=True).start()

    for raw_line in sys.stdin:
        raw_line = raw_line.strip()
        if not raw_line:
            continue

        try:
            cmd = json.loads(raw_line)
        except Exception as e:
            _emit_error("Invalid JSON: " + str(e))
            continue

        action = cmd.get("cmd", "")

        if action == "ping":
            _emit({"ok": True, "type": "pong", "ts": float(time.time()),
                   "preview_active": bool(_preview_running),
                   "memory_entries": int(len(_object_memory))})

        elif action == "screen":
            _emit(capture_screen())

        elif action == "camera":
            _emit(capture_camera())

        elif action == "detect_objects":
            _emit(do_object_detection())

        elif action == "recall_objects":
            limit = int(cmd.get("limit", 20))
            _emit(recall_objects(limit))

        elif action == "show_preview":
            start_preview()
            _emit({"ok": True, "type": "preview_started"})

        elif action == "hide_preview":
            stop_preview()
            _emit({"ok": True, "type": "preview_stopped"})

        elif action == "quit":
            stop_preview()
            _emit({"ok": True, "type": "bye"})
            break

        else:
            _emit_error("Unknown command: " + str(action))


if __name__ == "__main__":
    main()
