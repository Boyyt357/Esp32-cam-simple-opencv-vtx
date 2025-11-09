import socket, cv2, numpy as np, time

UDP_IP = "0.0.0.0"
UDP_PORT = 2222

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))
sock.settimeout(2)

buf = b''
frame_len = 0
last_fps_time = time.time()
frame_count = 0
fps = 0

print(f"Listening for ESP32-CAM on UDP port {UDP_PORT}…")

while True:
    try:
        data, _ = sock.recvfrom(2048)
    except socket.timeout:
        continue

    if len(data) == 4:
        # New frame header
        frame_len = int.from_bytes(data, 'little')
        buf = b''
        continue

    buf += data

    if frame_len and len(buf) >= frame_len:
        arr = np.frombuffer(buf, dtype=np.uint8)
        img = cv2.imdecode(arr, cv2.IMREAD_COLOR)

        if img is not None:
            # FPS calculation (no added lag)
            frame_count += 1
            now = time.time()
            if now - last_fps_time >= 1.0:
                fps = frame_count / (now - last_fps_time)
                frame_count = 0
                last_fps_time = now

            # Overlay text (very light)
            cv2.putText(img, f"FPS: {fps:.1f}", (10, 25),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

            cv2.imshow("ESP32-CAM UDP Stream", img)
            if cv2.waitKey(1) == 27:  # ESC to exit
                break

        buf = b''
        frame_len = 0

sock.close()
cv2.destroyAllWindows()
