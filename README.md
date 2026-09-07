# XIAO ESP32-S3 Sense Wi-Fi Camera Streamer

A compact Wi-Fi camera built with the **Seeed Studio XIAO ESP32-S3 Sense / XIAOML Kit**.

The project streams live MJPEG video over Wi-Fi, serves a simple browser-based camera page, supports single-frame JPEG capture, and uses the XIAOML Kit's **0.42" OLED** to show startup status, Wi-Fi connection state, and the assigned IP address.

## Features

- Live **MJPEG video streaming** over Wi-Fi
- Browser-based camera viewer
- Single JPEG snapshot endpoint
- 0.42" OLED status display
- Displays assigned IP address after connecting
- Automatic Wi-Fi reconnection
- OLED updates when Wi-Fi is lost or restored
- mDNS hostname support (`xiaocam.local`)
- Uses PSRAM for improved camera performance
- Adjustable camera resolution and JPEG quality

## Hardware

- Seeed Studio **XIAO ESP32-S3 Sense**
- XIAOML Kit / expansion board
- Camera module
- 0.42" 72 × 40 OLED display
- USB-C cable
- 2.4 GHz Wi-Fi network

## What the OLED Shows

During startup:

```text
XIAO CAM
BOOTING...
```

Then:

```text
XIAO CAM
CAMERA OK
```

While joining Wi-Fi:

```text
XIAO CAM
WIFI...
CONNECTING
```

After a successful connection:

```text
CAM LIVE
IP:
192.168.0.92
```

If Wi-Fi is lost:

```text
WIFI LOST
RECONNECTING
```

The OLED makes the camera easy to deploy without needing to connect a serial terminal just to discover its IP address.

## Software Requirements

### Arduino IDE

Install the current Arduino IDE.

### ESP32 Board Package

In **Boards Manager**, install:

```text
esp32 by Espressif Systems
```

Do **not** use `Arduino ESP32 Boards by Arduino` for this project.

Then select:

```text
Tools → Board → ESP32 Arduino → XIAO_ESP32S3
```

Recommended settings:

```text
Board:            XIAO_ESP32S3
PSRAM:            OPI PSRAM
USB CDC On Boot:  Enabled
```

### U8g2

Open Arduino Library Manager and install:

```text
U8g2 by olikraus
```

The project uses:

```cpp
#include <U8g2lib.h>
```

for the OLED.

## Wi-Fi Configuration

Edit these values near the beginning of the Arduino sketch:

```cpp
const char *WIFI_SSID =
  "YOUR_WIFI_NAME";

const char *WIFI_PASSWORD =
  "YOUR_WIFI_PASSWORD";
```

You can also change the camera's hostname:

```cpp
const char *HOSTNAME =
  "xiaocam";
```

For example:

```cpp
const char *HOSTNAME =
  "bender4-cam1";
```

would make the camera accessible as:

```text
http://bender4-cam1.local
```

on networks where mDNS is available.

## Network Endpoints

Once connected, the OLED will display the camera's IP address.

Assuming the camera receives:

```text
192.168.0.92
```

the available endpoints are:

### Camera Web Page

```text
http://192.168.0.92
```

or:

```text
http://xiaocam.local
```

### Direct MJPEG Stream

```text
http://192.168.0.92:81/stream
```

> Note the `:81`. The live stream is served on port 81.

### Single JPEG Capture

```text
http://192.168.0.92/capture
```

## Camera Resolution

The default configuration can be changed in the sketch:

```cpp
config.frame_size = FRAMESIZE_SXGA;
config.jpeg_quality = 10;
```

Common resolution options include:

| Arduino Setting | Resolution |
|---|---:|
| `FRAMESIZE_QVGA` | 320 × 240 |
| `FRAMESIZE_VGA` | 640 × 480 |
| `FRAMESIZE_SVGA` | 800 × 600 |
| `FRAMESIZE_XGA` | 1024 × 768 |
| `FRAMESIZE_SXGA` | 1280 × 1024 |
| `FRAMESIZE_UXGA` | 1600 × 1200 |
| `FRAMESIZE_QXGA` | 2048 × 1536 |

For live streaming, a good starting point is:

```cpp
config.frame_size = FRAMESIZE_SXGA;
config.jpeg_quality = 10;
```

Higher resolutions produce sharper images but reduce frame rate and increase Wi-Fi bandwidth and memory requirements.

For maximum-resolution still images, higher settings may be more practical than for continuous live streaming.

### JPEG Quality

The ESP32 camera driver uses a somewhat counterintuitive quality setting:

```cpp
config.jpeg_quality = 10;
```

Lower values mean **higher JPEG quality**.

A useful range is approximately:

```text
10–15
```

## Camera Pin Configuration

The sketch uses the XIAO ESP32-S3 Sense camera pin mapping:

```cpp
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1

#define XCLK_GPIO_NUM     10

#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15

#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13
```

## OLED Configuration

The XIAOML Kit OLED is configured using U8g2:

```cpp
U8G2_SSD1306_72X40_ER_1_HW_I2C u8g2(
  U8G2_R2,
  U8X8_PIN_NONE
);
```

The display is only 72 × 40 pixels, so the UI intentionally uses a small font that allows a full IPv4 address to fit on screen.

## Using the Stream in Python

The MJPEG stream can also be consumed directly with OpenCV:

```python
import cv2

url = "http://192.168.0.92:81/stream"

cap = cv2.VideoCapture(url)

while True:
    ok, frame = cap.read()

    if not ok:
        print("Lost camera")
        break

    cv2.imshow("XIAO Camera", frame)

    if cv2.waitKey(1) == 27:
        break

cap.release()
cv2.destroyAllWindows()
```

This makes the camera useful as a low-cost network vision endpoint for:

- laboratory equipment monitoring
- robot deck observation
- plate-position detection
- status-light monitoring
- instrument display monitoring
- machine-vision experiments
- remote equipment observation

The ESP32 can handle image acquisition and networking while a more capable computer performs image analysis or machine learning.

## Troubleshooting

### `WiFi.h: No such file or directory`

Example:

```text
fatal error: WiFi.h: No such file or directory
```

This usually means the ESP32 Arduino core is not installed or the wrong board is selected.

Install:

```text
esp32 by Espressif Systems
```

Then select:

```text
XIAO_ESP32S3
```

Do not install `WiFi.h` separately.

---

### `U8g2lib.h: No such file or directory`

Example:

```text
fatal error: U8g2lib.h: No such file or directory
```

Install:

```text
U8g2 by olikraus
```

from Arduino Library Manager.

---

### OLED shows an IP but `/stream` does not work

The live stream runs on **port 81**.

Correct:

```text
http://192.168.0.92:81/stream
```

Incorrect:

```text
http://192.168.0.92/stream
```

The normal browser interface remains on port 80:

```text
http://192.168.0.92
```

---

### Camera connects to Wi-Fi but browser cannot reach it

Make sure the computer and XIAO are on the same network.

Open the Arduino Serial Monitor at:

```text
115200 baud
```

and verify that startup includes messages similar to:

```text
Camera initialized
WiFi connected
Web server started
Stream server started
Camera servers running
```

---

### Streaming is slow at high resolution

Reduce the frame size:

```cpp
config.frame_size = FRAMESIZE_XGA;
```

or:

```cpp
config.frame_size = FRAMESIZE_VGA;
```

You can also increase JPEG compression:

```cpp
config.jpeg_quality = 12;
```

Remember: a larger JPEG-quality number means more compression and lower image quality.

## Architecture

```text
                  XIAO ESP32-S3 Sense
                 ┌─────────────────────┐
 Camera ────────►│ Camera interface    │
                 │                     │
 OLED ◄─────────►│ I2C                 │
                 │                     │
                 │ ESP32-S3            │
                 │                     │
                 │ Wi-Fi               │
                 └──────────┬──────────┘
                            │
                            │ 2.4 GHz Wi-Fi
                            ▼
                    Local network
                            │
              ┌─────────────┼─────────────┐
              ▼             ▼             ▼
           Browser        OpenCV        Server
              │             │             │
              ▼             ▼             ▼
           MJPEG        Computer       Monitoring /
           viewer        vision         automation
```

## Potential Next Steps

Possible improvements include:

- Browser-based resolution selector
- Browser-based JPEG quality control
- Camera name shown on OLED
- Alternating hostname/IP OLED display
- FPS counter
- Wi-Fi signal-strength display
- OTA firmware updates
- RTSP streaming
- MQTT status publishing
- static IP configuration
- automatic camera discovery
- authentication
- timestamp overlay
- motion detection
- periodic still-image capture
- integration with laboratory automation systems

## Security

This project is intended for use on a trusted local network.

The sample HTTP video server does **not** provide authentication or TLS encryption. Avoid exposing the ESP32 directly to the public Internet.

For remote access, use a secured gateway, VPN, reverse proxy, or other authenticated infrastructure rather than forwarding the ESP32's HTTP ports directly to the Internet.

## Acknowledgements

Built using:

- Seeed Studio XIAO ESP32-S3 Sense
- Espressif ESP32 Arduino core
- ESP32 camera driver
- U8g2 display library

---

If you build something interesting with this camera, contributions and improvements are welcome.
