# **High-Speed ESP32-CAM UDP Video Streamer to OpenCV PC Client**

This project implements a highly optimized, low-latency wireless video transmission (VTX) system. It uses an **ESP32-CAM** to capture JPEG frames and stream them over **UDP** (User Datagram Protocol) to a host PC running a **Python/OpenCV** script for real-time decoding and display.

## **🎥 VTX OUTPUT**

### **100 FPS on 400x296**
https://github.com/user-attachments/assets/bb5fd82e-d823-4ce3-92f3-3338d09a925c
### **70 FPS on 320x240**
https://github.com/user-attachments/assets/f212189a-cc11-4fbc-973c-587f489a4831
### **45 FPS on 640x480**
https://github.com/user-attachments/assets/f614a9de-a232-4b48-8fd3-213d67d799df
### **40 FPS on 800x600**
https://github.com/user-attachments/assets/d8f4c130-ad72-48be-bc0f-98640e194a20
### **12 FPS on 1024x768**
https://github.com/user-attachments/assets/792accd1-42c1-4362-945f-0e0192f51319
## **Tutorial and Installation**

![unnamed](https://github.com/user-attachments/assets/03a5ecef-0c63-4e1d-8167-f18de1f921e3)
https://youtu.be/XgG2CkQOob0

## **🚀 Architecture and Protocol**

Since UDP is packet-based and cannot natively handle large JPEG images, the project uses a custom application layer protocol:

1. **Fragmentation (TX):** The ESP32-CAM captures a JPEG image (up to \~30-50 KB). It first sends a **4-byte header** containing the total length of the image.  
2. **Chunking (TX):** The image data is then broken down into small **1 KB chunks** and sent sequentially over UDP to the PC.  
3. **Reassembly (RX):** The Python receiver listens on a specific port. It first reads the 4-byte header to determine the expected size, then buffers the incoming 1 KB chunks until the full image is reassembled.  
4. **Decoding (RX):** Once complete, the raw JPEG data is decoded using NumPy and OpenCV, and displayed in a window.

## **🛠️ Requirements**

### **Hardware**

* **Transmitter (TX):** ESP32-CAM (with PSRAM for better performance)  
* **Receiver (RX):** Host PC (Windows, Linux, or macOS)  
* **Network:** Both the ESP32-CAM and the PC must be connected to the **same Wi-Fi network**.

### **Software**

* **ESP32-CAM:** Arduino IDE with the ESP32 core and the esp\_camera library.  
* **PC Client:** Python 3.x with the following libraries:  
  * pip install opencv-python numpy

## **⚙️ Configuration (Crucial Step)**

You must configure the Wi-Fi credentials and, most importantly, the target IP address in the TX.ino sketch.

### **TX.ino Configuration Table**

| Parameter | C++ Variable | Value | Description |
| :---- | :---- | :---- | :---- |
| **Wi-Fi SSID** | ssid | "ESP32\_CAM\_RELAY" | The name of your local Wi-Fi network. |
| **Wi-Fi Password** | password | "camera123" | The password for your local Wi-Fi network. |
| **PC IP Address** | dest\_ip | **192,168,137,1** | **MUST BE REPLACED** with the actual IPv4 address of your PC. |
| **UDP Port** | dest\_port | 2222 | The port the Python script is listening on. |
| **Resolution** | FRAME\_SIZE | FRAMESIZE\_VGA (640x480) | Change for lower latency (QVGA) or higher resolution (SVGA). |
| **Quality** | JPEG\_QUALITY | 20 | Lower number means higher quality/larger file/slower stream (Range 0-63). |

### **🚨 Action Required: Setting the PC IP Address**

Before flashing the **TX code**, find your PC's local IP address and update this line in TX.ino:

// 🖥️ Replace with your PC’s IP  
IPAddress dest\_ip(192,168,137,1);

### **4\. TX Code Highlights**

The ESP32-CAM code includes several performance optimizations:

* **Dedicated Core:** The udpStreamerTask runs on **Core 1**, leaving Core 0 free for Wi-Fi and system operations, improving stability and frame rate.  
* **High XCLK Frequency:** config.xclk\_freq\_hz \= 37000000 is set to maximize the clock speed for faster camera operation.  
* **CAMERA\_FB\_IN\_PSRAM:** Uses the external PSRAM for frame buffers, crucial for VGA resolution and above.

## **🐍 PC Receiver Clients**

Two Python receiver scripts are provided for different use cases. Both require the same setup (opencv-python and numpy).

### **Client 1: Basic Viewer (udp\_viewer.py logic)**

This is the simplest reassembly and decoding logic.

1. Listens for the 4-byte frame length header on port **2222**.  
2. Buffers the incoming packets until the total length is reached.  
3. Decodes and displays the image.

### **Client 2: Viewer with FPS Display (udp\_viewerfps.py logic)**

This version adds a simple FPS (Frames Per Second) calculation and displays the rate directly on the video feed.

\# Part of the FPS calculation logic in rx1.py  
frame\_count \+= 1  
now \= time.time()  
if now \- last\_fps\_time \>= 1.0:  
    fps \= frame\_count / (now \- last\_fps\_time)  
    \# ... reset counters ...

## **🎬 Getting Started**

1. **Configure:** Update the Wi-Fi credentials and your **PC's IP address** in the **TX code**.  
2. **Flash:** Upload the modified C++ code to your ESP32-CAM.  
3. **Run Client:** Execute your preferred Python script on your PC (e.g., python rx1.py).  
4. **View:** Once the ESP32-CAM connects to Wi-Fi, the stream will begin automatically, and the video should appear in an OpenCV window on your PC. Press the ESC key to exit the video window.
