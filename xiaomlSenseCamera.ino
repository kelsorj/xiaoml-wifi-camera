/*
  ================================================================
  XIAOML / XIAO ESP32-S3 SENSE CAMERA STREAMING SERVER
  with 0.42" OLED WiFi/IP STATUS DISPLAY
  ================================================================

  Hardware:
    - Seeed Studio XIAO ESP32-S3 Sense
    - Camera module
    - XIAOML Kit 0.42" OLED
        SSD1306
        72 x 40 pixels
        I2C

  Features:
    - Camera initialization
    - WiFi connection
    - OLED status
    - MJPEG live video streaming
    - Single JPEG capture
    - Web interface
    - mDNS hostname
    - WiFi reconnect
    - OLED automatically updates IP

  URLs:
    http://xiaocam.local
    http://xiaocam.local/capture
    http://xiaocam.local:81/stream

  Arduino libraries:
    - ESP32 by Espressif Systems
    - U8g2 by oliver
*/


// ================================================================
// LIBRARIES
// ================================================================

#include <Arduino.h>

#include <WiFi.h>
#include <ESPmDNS.h>

#include "esp_camera.h"
#include "esp_http_server.h"

#include <Wire.h>
#include <U8g2lib.h>


// ================================================================
// USER SETTINGS
// ================================================================

// CHANGE THESE
const char *WIFI_SSID =
  "ssid";

const char *WIFI_PASSWORD =
  "wifi_pwd";


// Network name:
//
// http://xiaocam.local
//
const char *HOSTNAME =
  "xiaocam";


// ================================================================
// OLED
// ================================================================
//
// XIAOML Kit:
// SSD1306
// 72 x 40
// Hardware I2C
//
// U8G2_R2 matches the orientation used by
// the XIAOML example.
//

U8G2_SSD1306_72X40_ER_1_HW_I2C u8g2(
  U8G2_R2,
  U8X8_PIN_NONE
);


// ================================================================
// XIAO ESP32-S3 SENSE CAMERA PINS
// ================================================================

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


// ================================================================
// HTTP SERVERS
// ================================================================

httpd_handle_t web_httpd =
  NULL;

httpd_handle_t stream_httpd =
  NULL;


// ================================================================
// SYSTEM STATE
// ================================================================

bool wifiWasConnected = false;

unsigned long lastWiFiCheck = 0;
unsigned long lastReconnectAttempt = 0;


// ================================================================
// OLED GENERAL DISPLAY FUNCTION
// ================================================================

void oledDisplay(
  const String &line1,
  const String &line2 = "",
  const String &line3 = ""
)
{
  u8g2.firstPage();

  do
  {
    // Tiny font chosen so a full IPv4 address
    // can fit on the 72-pixel-wide display.

    u8g2.setFont(
      u8g2_font_4x6_tr
    );

    // Border
    u8g2.drawFrame(
      0,
      0,
      72,
      40
    );

    // Line 1
    u8g2.setCursor(
      4,
      10
    );

    u8g2.print(
      line1
    );


    // Line 2
    u8g2.setCursor(
      4,
      21
    );

    u8g2.print(
      line2
    );


    // Line 3
    u8g2.setCursor(
      4,
      32
    );

    u8g2.print(
      line3
    );

  } while (
    u8g2.nextPage()
  );
}


// ================================================================
// OLED STATUS SCREENS
// ================================================================

void oledBoot()
{
  oledDisplay(
    "XIAO CAM",
    "BOOTING...",
    ""
  );
}


void oledCameraStarting()
{
  oledDisplay(
    "XIAO CAM",
    "CAMERA...",
    ""
  );
}


void oledCameraOK()
{
  oledDisplay(
    "XIAO CAM",
    "CAMERA OK",
    ""
  );
}


void oledCameraFailed()
{
  oledDisplay(
    "ERROR",
    "CAMERA",
    "FAILED"
  );
}


void oledWiFiConnecting()
{
  oledDisplay(
    "XIAO CAM",
    "WIFI...",
    "CONNECTING"
  );
}


void oledWiFiFailed()
{
  oledDisplay(
    "WIFI",
    "NOT CONNECTED",
    "RETRYING..."
  );
}


void oledWiFiLost()
{
  oledDisplay(
    "WIFI LOST",
    "RECONNECTING",
    ""
  );
}


void oledWiFiConnected()
{
  String ip =
    WiFi.localIP().toString();

  oledDisplay(
    "CAM LIVE",
    "IP:",
    ip
  );
}


// ================================================================
// WEB PAGE
// ================================================================

static const char INDEX_HTML[] PROGMEM =
R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta
  name="viewport"
  content="width=device-width, initial-scale=1"
/>

<title>
XIAO Camera
</title>

<style>

body
{
  margin: 0;
  padding: 0;

  background: #111;
  color: #eee;

  font-family:
    Arial,
    Helvetica,
    sans-serif;

  text-align: center;
}


h1
{
  margin-top: 20px;

  font-size: 24px;
}


.status
{
  margin-bottom: 15px;

  font-size: 14px;
}


#camera
{
  width: 95%;

  max-width: 1000px;

  height: auto;

  border-radius: 8px;
}


.buttons
{
  margin: 20px;
}


a
{
  display: inline-block;

  margin: 5px;

  padding: 12px 20px;

  background: white;
  color: black;

  text-decoration: none;

  border-radius: 6px;
}

</style>

</head>


<body>


<h1>
XIAO ESP32-S3 Camera
</h1>


<div class="status">
LIVE
</div>


<img
  id="camera"
/>


<div class="buttons">

<a
  href="/capture"
  target="_blank"
>
Capture JPEG
</a>

</div>


<script>

const hostname =
  window.location.hostname;

const streamURL =
  "http://" +
  hostname +
  ":81/stream";

document.getElementById(
  "camera"
).src =
  streamURL;

</script>


</body>

</html>

)rawliteral";


// ================================================================
// WEB PAGE HANDLER
// ================================================================

static esp_err_t index_handler(
  httpd_req_t *req
)
{
  httpd_resp_set_type(
    req,
    "text/html"
  );

  httpd_resp_set_hdr(
    req,
    "Cache-Control",
    "no-store"
  );

  return httpd_resp_send(
    req,
    INDEX_HTML,
    HTTPD_RESP_USE_STRLEN
  );
}


// ================================================================
// SINGLE JPEG CAPTURE
// ================================================================

static esp_err_t capture_handler(
  httpd_req_t *req
)
{
  camera_fb_t *fb =
    esp_camera_fb_get();


  if (!fb)
  {
    Serial.println(
      "Camera capture failed"
    );

    httpd_resp_send_500(
      req
    );

    return ESP_FAIL;
  }


  httpd_resp_set_type(
    req,
    "image/jpeg"
  );


  httpd_resp_set_hdr(
    req,
    "Content-Disposition",
    "inline; filename=capture.jpg"
  );


  httpd_resp_set_hdr(
    req,
    "Access-Control-Allow-Origin",
    "*"
  );


  httpd_resp_set_hdr(
    req,
    "Cache-Control",
    "no-store"
  );


  esp_err_t result =
    httpd_resp_send(
      req,
      (const char *)fb->buf,
      fb->len
    );


  esp_camera_fb_return(
    fb
  );


  return result;
}


// ================================================================
// MJPEG STREAM CONSTANTS
// ================================================================

#define PART_BOUNDARY \
  "123456789000000000000987654321"


static const char *STREAM_CONTENT_TYPE =
  "multipart/x-mixed-replace;boundary="
  PART_BOUNDARY;


static const char *STREAM_BOUNDARY =
  "\r\n--"
  PART_BOUNDARY
  "\r\n";


static const char *STREAM_PART =
  "Content-Type: image/jpeg\r\n"
  "Content-Length: %u\r\n\r\n";


// ================================================================
// MJPEG STREAM HANDLER
// ================================================================

static esp_err_t stream_handler(
  httpd_req_t *req
)
{
  camera_fb_t *fb =
    NULL;

  esp_err_t result =
    ESP_OK;

  char header[64];


  result =
    httpd_resp_set_type(
      req,
      STREAM_CONTENT_TYPE
    );


  if (
    result != ESP_OK
  )
  {
    return result;
  }


  httpd_resp_set_hdr(
    req,
    "Access-Control-Allow-Origin",
    "*"
  );


  httpd_resp_set_hdr(
    req,
    "Cache-Control",
    "no-store"
  );


  Serial.println(
    "Video stream client connected"
  );


  while (true)
  {
    // ----------------------------------------------------------
    // Capture frame
    // ----------------------------------------------------------

    fb =
      esp_camera_fb_get();


    if (!fb)
    {
      Serial.println(
        "Camera frame capture failed"
      );

      result =
        ESP_FAIL;

      break;
    }


    // ----------------------------------------------------------
    // Send MJPEG boundary
    // ----------------------------------------------------------

    result =
      httpd_resp_send_chunk(
        req,
        STREAM_BOUNDARY,
        strlen(
          STREAM_BOUNDARY
        )
      );


    // ----------------------------------------------------------
    // Send JPEG header
    // ----------------------------------------------------------

    if (
      result == ESP_OK
    )
    {
      size_t headerLength =
        snprintf(
          header,
          sizeof(header),
          STREAM_PART,
          fb->len
        );


      result =
        httpd_resp_send_chunk(
          req,
          header,
          headerLength
        );
    }


    // ----------------------------------------------------------
    // Send JPEG image
    // ----------------------------------------------------------

    if (
      result == ESP_OK
    )
    {
      result =
        httpd_resp_send_chunk(
          req,
          (const char *)fb->buf,
          fb->len
        );
    }


    // ----------------------------------------------------------
    // Return framebuffer
    // ----------------------------------------------------------

    esp_camera_fb_return(
      fb
    );

    fb =
      NULL;


    // ----------------------------------------------------------
    // Browser disconnected
    // ----------------------------------------------------------

    if (
      result != ESP_OK
    )
    {
      Serial.println(
        "Video stream client disconnected"
      );

      break;
    }


    // Give the network stack a chance
    // to service other tasks.

    delay(1);
  }


  if (fb)
  {
    esp_camera_fb_return(
      fb
    );
  }


  return result;
}


// ================================================================
// START WEB SERVERS
// ================================================================

void startCameraServers()
{
  // ============================================================
  // NORMAL WEB SERVER
  // PORT 80
  // ============================================================

  httpd_config_t webConfig =
    HTTPD_DEFAULT_CONFIG();


  webConfig.server_port =
    80;


  httpd_uri_t indexUri =
    {};

  indexUri.uri =
    "/";

  indexUri.method =
    HTTP_GET;

  indexUri.handler =
    index_handler;

  indexUri.user_ctx =
    NULL;


  httpd_uri_t captureUri =
    {};

  captureUri.uri =
    "/capture";

  captureUri.method =
    HTTP_GET;

  captureUri.handler =
    capture_handler;

  captureUri.user_ctx =
    NULL;


  if (
    httpd_start(
      &web_httpd,
      &webConfig
    ) == ESP_OK
  )
  {
    httpd_register_uri_handler(
      web_httpd,
      &indexUri
    );


    httpd_register_uri_handler(
      web_httpd,
      &captureUri
    );


    Serial.println(
      "Web server started"
    );
  }
  else
  {
    Serial.println(
      "Failed to start web server"
    );
  }


  // ============================================================
  // STREAM SERVER
  // PORT 81
  // ============================================================

  httpd_config_t streamConfig =
    HTTPD_DEFAULT_CONFIG();


  streamConfig.server_port =
    81;


  // Each server needs its own
  // control port.

  streamConfig.ctrl_port =
    webConfig.ctrl_port + 1;


  httpd_uri_t streamUri =
    {};

  streamUri.uri =
    "/stream";

  streamUri.method =
    HTTP_GET;

  streamUri.handler =
    stream_handler;

  streamUri.user_ctx =
    NULL;


  if (
    httpd_start(
      &stream_httpd,
      &streamConfig
    ) == ESP_OK
  )
  {
    httpd_register_uri_handler(
      stream_httpd,
      &streamUri
    );


    Serial.println(
      "Stream server started"
    );
  }
  else
  {
    Serial.println(
      "Failed to start stream server"
    );
  }
}


// ================================================================
// CAMERA INITIALIZATION
// ================================================================

bool initCamera()
{
  camera_config_t config =
    {};


  // --------------------------------------------------------------
  // Camera clock
  // --------------------------------------------------------------

  config.ledc_channel =
    LEDC_CHANNEL_0;

  config.ledc_timer =
    LEDC_TIMER_0;


  // --------------------------------------------------------------
  // Camera data pins
  // --------------------------------------------------------------

  config.pin_d0 =
    Y2_GPIO_NUM;

  config.pin_d1 =
    Y3_GPIO_NUM;

  config.pin_d2 =
    Y4_GPIO_NUM;

  config.pin_d3 =
    Y5_GPIO_NUM;

  config.pin_d4 =
    Y6_GPIO_NUM;

  config.pin_d5 =
    Y7_GPIO_NUM;

  config.pin_d6 =
    Y8_GPIO_NUM;

  config.pin_d7 =
    Y9_GPIO_NUM;


  // --------------------------------------------------------------
  // Camera control pins
  // --------------------------------------------------------------

  config.pin_xclk =
    XCLK_GPIO_NUM;

  config.pin_pclk =
    PCLK_GPIO_NUM;

  config.pin_vsync =
    VSYNC_GPIO_NUM;

  config.pin_href =
    HREF_GPIO_NUM;


  config.pin_sccb_sda =
    SIOD_GPIO_NUM;

  config.pin_sccb_scl =
    SIOC_GPIO_NUM;


  config.pin_pwdn =
    PWDN_GPIO_NUM;

  config.pin_reset =
    RESET_GPIO_NUM;


  // --------------------------------------------------------------
  // Camera clock frequency
  // --------------------------------------------------------------

  config.xclk_freq_hz =
    20000000;


  // --------------------------------------------------------------
  // We want JPEG directly from
  // the camera for streaming.
  // --------------------------------------------------------------

  config.pixel_format =
    PIXFORMAT_JPEG;


  // --------------------------------------------------------------
  // Default streaming resolution
  //
  // VGA = 640 x 480
  //
  // Other options:
  //
  // FRAMESIZE_QVGA   320 x 240
  // FRAMESIZE_VGA    640 x 480
  // FRAMESIZE_SVGA   800 x 600
  // FRAMESIZE_XGA   1024 x 768
  // FRAMESIZE_SXGA  1280 x 1024
  // FRAMESIZE_UXGA  1600 x 1200
  // --------------------------------------------------------------

  config.frame_size =
    FRAMESIZE_SXGA;


  // --------------------------------------------------------------
  // JPEG quality
  //
  // Lower = better image quality
  //
  // 10-15 is a good range.
  // --------------------------------------------------------------

  config.jpeg_quality =
    10;


  // --------------------------------------------------------------
  // PSRAM configuration
  // --------------------------------------------------------------

  if (
    psramFound()
  )
  {
    Serial.println(
      "PSRAM detected"
    );


    config.fb_location =
      CAMERA_FB_IN_PSRAM;


    // Two framebuffers significantly
    // improve video streaming.

    config.fb_count =
      2;


    // Always favor latest frame.

    config.grab_mode =
      CAMERA_GRAB_LATEST;
  }
  else
  {
    Serial.println(
      "WARNING: PSRAM not detected"
    );


    // Reduce resolution if we
    // don't have PSRAM.

    config.frame_size =
      FRAMESIZE_QVGA;


    config.fb_location =
      CAMERA_FB_IN_DRAM;


    config.fb_count =
      1;


    config.grab_mode =
      CAMERA_GRAB_WHEN_EMPTY;
  }


  // --------------------------------------------------------------
  // Initialize camera
  // --------------------------------------------------------------

  esp_err_t err =
    esp_camera_init(
      &config
    );


  if (
    err != ESP_OK
  )
  {
    Serial.printf(
      "Camera initialization failed: 0x%x\n",
      err
    );


    return false;
  }


  // --------------------------------------------------------------
  // Optional image tuning
  // --------------------------------------------------------------

  sensor_t *sensor =
    esp_camera_sensor_get();


  if (sensor)
  {
    sensor->set_brightness(
      sensor,
      0
    );


    sensor->set_contrast(
      sensor,
      0
    );


    sensor->set_saturation(
      sensor,
      0
    );
  }


  return true;
}


// ================================================================
// START MDNS
// ================================================================

void startMDNS()
{
  MDNS.end();


  if (
    MDNS.begin(
      HOSTNAME
    )
  )
  {
    MDNS.addService(
      "http",
      "tcp",
      80
    );


    Serial.print(
      "mDNS started: http://"
    );

    Serial.print(
      HOSTNAME
    );

    Serial.println(
      ".local"
    );
  }
  else
  {
    Serial.println(
      "mDNS failed"
    );
  }
}


// ================================================================
// PRINT NETWORK INFORMATION
// ================================================================

void printNetworkInfo()
{
  IPAddress ip =
    WiFi.localIP();


  Serial.println();
  Serial.println(
    "================================"
  );

  Serial.println(
    "CAMERA ONLINE"
  );

  Serial.println(
    "================================"
  );


  Serial.print(
    "SSID: "
  );

  Serial.println(
    WiFi.SSID()
  );


  Serial.print(
    "IP: "
  );

  Serial.println(
    ip
  );


  Serial.print(
    "Signal: "
  );

  Serial.print(
    WiFi.RSSI()
  );

  Serial.println(
    " dBm"
  );


  Serial.println();


  Serial.print(
    "Web page:      http://"
  );

  Serial.println(
    ip
  );


  Serial.print(
    "JPEG capture:  http://"
  );

  Serial.print(
    ip
  );

  Serial.println(
    "/capture"
  );


  Serial.print(
    "Video stream:  http://"
  );

  Serial.print(
    ip
  );

  Serial.println(
    ":81/stream"
  );


  Serial.println();


  Serial.print(
    "Friendly name: http://"
  );

  Serial.print(
    HOSTNAME
  );

  Serial.println(
    ".local"
  );


  Serial.println(
    "================================"
  );

  Serial.println();
}


// ================================================================
// CONNECT TO WIFI
// ================================================================

bool connectWiFi(
  unsigned long timeoutMs
)
{
  Serial.print(
    "Connecting to WiFi: "
  );

  Serial.println(
    WIFI_SSID
  );


  oledWiFiConnecting();


  WiFi.mode(
    WIFI_STA
  );


  // Disable WiFi power saving.
  // This usually gives smoother
  // camera streaming.

  WiFi.setSleep(
    false
  );


  WiFi.setHostname(
    HOSTNAME
  );


  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );


  unsigned long start =
    millis();


  while (
    WiFi.status() != WL_CONNECTED
  )
  {
    delay(
      500
    );


    Serial.print(
      "."
    );


    if (
      millis() - start >
      timeoutMs
    )
    {
      Serial.println();

      Serial.println(
        "WiFi connection timed out"
      );


      oledWiFiFailed();


      return false;
    }
  }


  Serial.println();

  Serial.println(
    "WiFi connected"
  );


  wifiWasConnected =
    true;


  oledWiFiConnected();


  startMDNS();


  printNetworkInfo();


  return true;
}


// ================================================================
// HANDLE WIFI CONNECTION
// ================================================================

void handleWiFi()
{
  // Only check every 2 seconds.

  if (
    millis() - lastWiFiCheck <
    2000
  )
  {
    return;
  }


  lastWiFiCheck =
    millis();


  bool currentlyConnected =
    WiFi.status() ==
    WL_CONNECTED;


  // --------------------------------------------------------------
  // Connection was lost
  // --------------------------------------------------------------

  if (
    !currentlyConnected &&
    wifiWasConnected
  )
  {
    Serial.println(
      "WiFi connection lost"
    );


    oledWiFiLost();


    wifiWasConnected =
      false;


    lastReconnectAttempt =
      0;
  }


  // --------------------------------------------------------------
  // WiFi disconnected
  // --------------------------------------------------------------

  if (
    !currentlyConnected
  )
  {
    // Try reconnecting every
    // five seconds.

    if (
      millis() -
      lastReconnectAttempt >=
      5000
    )
    {
      lastReconnectAttempt =
        millis();


      Serial.println(
        "Attempting WiFi reconnect..."
      );


      WiFi.reconnect();
    }


    return;
  }


  // --------------------------------------------------------------
  // Connection restored
  // --------------------------------------------------------------

  if (
    currentlyConnected &&
    !wifiWasConnected
  )
  {
    wifiWasConnected =
      true;


    Serial.println(
      "WiFi reconnected"
    );


    oledWiFiConnected();


    startMDNS();


    printNetworkInfo();
  }
}


// ================================================================
// SETUP
// ================================================================

void setup()
{
  // --------------------------------------------------------------
  // SERIAL
  // --------------------------------------------------------------

  Serial.begin(
    115200
  );


  delay(
    1000
  );


  Serial.println();
  Serial.println();
  Serial.println(
    "================================"
  );

  Serial.println(
    "XIAOML CAMERA"
  );

  Serial.println(
    "================================"
  );


  // --------------------------------------------------------------
  // OLED
  // --------------------------------------------------------------

  u8g2.begin();


  oledBoot();


  Serial.println(
    "OLED initialized"
  );


  delay(
    1000
  );


  // --------------------------------------------------------------
  // CAMERA
  // --------------------------------------------------------------

  oledCameraStarting();


  Serial.println(
    "Initializing camera..."
  );


  if (
    !initCamera()
  )
  {
    oledCameraFailed();


    Serial.println(
      "FATAL: Camera initialization failed"
    );


    // Stop here if camera
    // initialization fails.

    while (true)
    {
      delay(
        1000
      );
    }
  }


  Serial.println(
    "Camera initialized"
  );


  oledCameraOK();


  delay(
    1000
  );


  // --------------------------------------------------------------
  // WIFI
  // --------------------------------------------------------------

  connectWiFi(
    30000
  );


  // --------------------------------------------------------------
  // CAMERA WEB SERVERS
  //
  // Start these even if the first
  // WiFi connection fails.
  //
  // If WiFi later connects,
  // they'll become available.
  // --------------------------------------------------------------

  startCameraServers();


  Serial.println(
    "Camera servers running"
  );


  // If already connected,
  // make sure OLED contains IP.

  if (
    WiFi.status() ==
    WL_CONNECTED
  )
  {
    oledWiFiConnected();
  }


  Serial.println(
    "Setup complete"
  );
}


// ================================================================
// MAIN LOOP
// ================================================================

void loop()
{
  // Everything else runs as
  // FreeRTOS / HTTP tasks.

  // We only need to monitor WiFi.

  handleWiFi();


  delay(
    10
  );
}