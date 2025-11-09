import socket, cv2, numpy as np

UDP_IP = "0.0.0.0"       # listen on all interfaces
UDP_PORT = 2222          # must match dest_port in ESP32 code

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))
sock.settimeout(2)

buf = b''
frame_len = 0

print(f"Listening for ESP32-CAM on UDP port {UDP_PORT}…")

while True:
    try:
        data, _ = sock.recvfrom(2048)
    except socket.timeout:
        continue

    if len(data) == 4:
        # new frame header (frame length)
        frame_len = int.from_bytes(data, 'little')
        buf = b''
        continue

    buf += data
    if frame_len and len(buf) >= frame_len:
        arr = np.frombuffer(buf, dtype=np.uint8)
        img = cv2.imdecode(arr, cv2.IMREAD_COLOR)
        if img is not None:
            cv2.imshow("ESP32-CAM UDP Stream", img)
            if cv2.waitKey(1) == 27:  # Esc key
                break
        buf = b''
        frame_len = 0

sock.close()
cv2.destroyAllWindows()
