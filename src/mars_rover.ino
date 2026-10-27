/*
  ============================================================
  MARS ROVER - ESP8266 NodeMCU
  ============================================================
  Movement controls + live sensor dashboard, all served from
  the ESP8266 itself in AP (Access Point) mode.

  SENSORS:
    - MPU6050  (I2C) -> accel (ax,ay,az), gyro (gx,gy,gz), tilt angle
    - DHT11         -> temperature (C), humidity (%)
    - BMP280   (I2C) -> pressure (hPa), altitude (m), temperature (C)

  MOTORS:
    - L293D driver, 2 DC geared motors (differential drive: LEFT / RIGHT)

  NEW IN THIS VERSION:
    - I2C bus scanner (web button "Scan I2C Bus") - lists every address found
    - Address picker: choose which detected address is the MPU6050 and
      which is the BMP280, then Save - the choice is written to EEPROM
      (flash) so it survives power-off/reboot, and the sensors are
      re-initialized immediately with the new address.

  HOW IT WORKS:
    - ESP8266 creates its own WiFi network (AP mode) called "MARS_ROVER"
    - Connect your phone/laptop to that WiFi (password below)
    - Open a browser to 192.168.4.1
    - Tap the D-pad buttons to drive; sensor readings update live via AJAX
    - Use the "I2C Scanner" panel if a sensor isn't detected, to find and
      assign its actual address (useful because BMP280 modules ship as
      either 0x76 or 0x77, and some MPU6050 boards use 0x69 instead of 0x68)

  LIBRARIES NEEDED (Arduino IDE -> Library Manager):
    - ESP8266WiFi, ESP8266WebServer   (come with ESP8266 board package)
    - Wire, EEPROM                    (built-in)
    - Adafruit MPU6050
    - Adafruit Unified Sensor
    - Adafruit BMP280 Library
    - DHT sensor library (by Adafruit)
  ============================================================
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>

// ---------------- WIFI AP SETTINGS ----------------
const char* AP_SSID = "MARS_ROVER";
const char* AP_PASS = "rover1234";   // must be 8+ characters

// ---------------- PIN MAP ----------------
// I2C bus (MPU6050 + BMP280 share this)
#define I2C_SDA D2   // GPIO4
#define I2C_SCL D1   // GPIO5

// L293D motor driver - LEFT motor
#define IN1 D5       // GPIO14
#define IN2 D6       // GPIO12
// L293D motor driver - RIGHT motor
#define IN3 D7       // GPIO13
#define IN4 D8       // GPIO15

// DHT11 data pin
#define DHTPIN D0    // GPIO16
#define DHTTYPE DHT11

// ---------------- EEPROM LAYOUT ----------------
// byte 0: MPU6050 I2C address
// byte 1: BMP280 I2C address
// byte 2: magic marker (0xA5) so we know EEPROM was ever written
#define EEPROM_SIZE       8
#define EEPROM_ADDR_MPU   0
#define EEPROM_ADDR_BMP   1
#define EEPROM_MAGIC_LOC  2
#define EEPROM_MAGIC_VAL  0xA5

#define DEFAULT_MPU_ADDR  0x68
#define DEFAULT_BMP_ADDR  0x76

uint8_t mpuAddr = DEFAULT_MPU_ADDR;
uint8_t bmpAddr = DEFAULT_BMP_ADDR;

// ---------------- OBJECTS ----------------
ESP8266WebServer server(80);
Adafruit_MPU6050 mpu;
Adafruit_BMP280 bmp;   // I2C
DHT dht(DHTPIN, DHTTYPE);

bool mpuOK = false;
bool bmpOK = false;

// live sensor values (updated in loop, read by /data handler)
float g_ax, g_ay, g_az, g_gx, g_gy, g_gz, g_tiltX, g_tiltY;
float g_temp_dht, g_hum_dht;
float g_temp_bmp, g_pressure, g_altitude;
String g_lastMove = "STOP";

unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 500; // ms

// ================= EEPROM HELPERS =================
void loadAddressesFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  uint8_t magic = EEPROM.read(EEPROM_MAGIC_LOC);
  if (magic == EEPROM_MAGIC_VAL) {
    mpuAddr = EEPROM.read(EEPROM_ADDR_MPU);
    bmpAddr = EEPROM.read(EEPROM_ADDR_BMP);
  } else {
    mpuAddr = DEFAULT_MPU_ADDR;
    bmpAddr = DEFAULT_BMP_ADDR;
  }
}

void saveAddressToEEPROM(const String &sensor, uint8_t addr) {
  if (sensor == "mpu") {
    mpuAddr = addr;
    EEPROM.write(EEPROM_ADDR_MPU, addr);
  } else if (sensor == "bmp") {
    bmpAddr = addr;
    EEPROM.write(EEPROM_ADDR_BMP, addr);
  }
  EEPROM.write(EEPROM_MAGIC_LOC, EEPROM_MAGIC_VAL);
  EEPROM.commit();
}

// ================= SENSOR (RE)INIT =================
void initMPU() {
  mpuOK = mpu.begin(mpuAddr);
  if (mpuOK) {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.print("MPU6050 OK at 0x"); Serial.println(mpuAddr, HEX);
  } else {
    Serial.print("MPU6050 NOT FOUND at 0x"); Serial.println(mpuAddr, HEX);
  }
}

void initBMP() {
  bmpOK = bmp.begin(bmpAddr);
  if (bmpOK) {
    Serial.print("BMP280 OK at 0x"); Serial.println(bmpAddr, HEX);
  } else {
    Serial.print("BMP280 NOT FOUND at 0x"); Serial.println(bmpAddr, HEX);
  }
}

// ================= MOTOR CONTROL =================
void motorsStop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void moveForward() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);   // left forward
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);   // right forward
}

void moveBackward() {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);  // left backward
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);  // right backward
}

void turnLeft() {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);  // left backward
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);   // right forward
}

void turnRight() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);   // left forward
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);  // right backward
}

// ================= SENSOR READ =================
void readSensors() {
  if (mpuOK) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    g_ax = a.acceleration.x;
    g_ay = a.acceleration.y;
    g_az = a.acceleration.z;
    g_gx = g.gyro.x;
    g_gy = g.gyro.y;
    g_gz = g.gyro.z;
    // approximate tilt angles (degrees) from accelerometer
    g_tiltX = atan2(g_ay, sqrt(g_ax * g_ax + g_az * g_az)) * 180.0 / PI;
    g_tiltY = atan2(-g_ax, sqrt(g_ay * g_ay + g_az * g_az)) * 180.0 / PI;
  }

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h)) g_hum_dht = h;
  if (!isnan(t)) g_temp_dht = t;

  if (bmpOK) {
    g_temp_bmp = bmp.readTemperature();
    g_pressure = bmp.readPressure() / 100.0F; // Pa -> hPa
    g_altitude = bmp.readAltitude(1013.25);   // adjust to local sea-level hPa if known
  }
}

// ================= WEB PAGE (HTML+CSS+JS) =================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Mars Rover Control</title>
<style>
  body { background:#1a1512; color:#f2c9a0; font-family:Arial, sans-serif; text-align:center; margin:0; padding:16px; }
  h1 { color:#e8734a; margin-bottom:4px; }
  .sub { color:#a88; font-size:13px; margin-bottom:16px; }
  .dpad { display:grid; grid-template-columns:70px 70px 70px; grid-template-rows:70px 70px 70px; gap:6px; justify-content:center; margin:20px auto; }
  .dpad button { font-size:22px; border:none; border-radius:10px; background:#e8734a; color:#1a1512; font-weight:bold; }
  .dpad button:active { background:#ff9166; }
  #up { grid-column:2; grid-row:1; }
  #left { grid-column:1; grid-row:2; }
  #stop { grid-column:2; grid-row:2; background:#c0392b; color:#fff; }
  #right { grid-column:3; grid-row:2; }
  #down { grid-column:2; grid-row:3; }
  .panel { background:#241d19; border-radius:12px; padding:14px; margin:10px auto; max-width:380px; text-align:left; }
  .panel h3 { margin-top:0; color:#e8734a; border-bottom:1px solid #3a2f28; padding-bottom:6px; }
  .row { display:flex; justify-content:space-between; padding:3px 0; font-size:15px; }
  .row span:last-child { color:#fff; font-weight:bold; }
  #status { margin-top:6px; font-size:13px; color:#8f8; }
  .scanBtn, .saveBtn { background:#4a90e2; color:#fff; border:none; border-radius:8px; padding:8px 14px; font-size:14px; margin:4px 2px; }
  .saveBtn { background:#2ecc71; }
  select { background:#1a1512; color:#f2c9a0; border:1px solid #4a3a30; border-radius:6px; padding:4px; font-size:14px; }
  #scanResult { font-size:13px; color:#ccc; margin-top:8px; }
  .addrRow { display:flex; align-items:center; justify-content:space-between; margin:8px 0; }
  #scanStatus { font-size:12px; color:#8f8; margin-top:4px; }
</style>
</head>
<body>
  <h1>🚀 MARS ROVER</h1>
  <div class="sub">AP Mode Control Dashboard</div>

  <div class="dpad">
    <button id="up" ontouchstart="cmd('forward')" ontouchend="cmd('stop')" onmousedown="cmd('forward')" onmouseup="cmd('stop')">▲</button>
    <button id="left" ontouchstart="cmd('left')" ontouchend="cmd('stop')" onmousedown="cmd('left')" onmouseup="cmd('stop')">◀️</button>
    <button id="stop" onclick="cmd('stop')">■</button>
    <button id="right" ontouchstart="cmd('right')" ontouchend="cmd('stop')" onmousedown="cmd('right')" onmouseup="cmd('stop')">▶️</button>
    <button id="down" ontouchstart="cmd('backward')" ontouchend="cmd('stop')" onmousedown="cmd('backward')" onmouseup="cmd('stop')">▼</button>
  </div>
  <div id="status">Move: STOP</div>

  <div class="panel">
    <h3>MPU6050 - Motion</h3>
    <div class="row"><span>Accel X / Y / Z (m/s²)</span><span id="acc">-</span></div>
    <div class="row"><span>Gyro X / Y / Z (°/s)</span><span id="gyro">-</span></div>
    <div class="row"><span>Tilt X / Y (°)</span><span id="tilt">-</span></div>
    <div class="row"><span>Status</span><span id="mpuStatus">-</span></div>
  </div>

  <div class="panel">
    <h3>DHT11 - Environment</h3>
    <div class="row"><span>Temperature</span><span id="dhtTemp">-</span></div>
    <div class="row"><span>Humidity</span><span id="dhtHum">-</span></div>
  </div>

  <div class="panel">
    <h3>BMP280 - Atmosphere</h3>
    <div class="row"><span>Temperature</span><span id="bmpTemp">-</span></div>
    <div class="row"><span>Pressure</span><span id="pressure">-</span></div>
    <div class="row"><span>Altitude</span><span id="altitude">-</span></div>
    <div class="row"><span>Status</span><span id="bmpStatus">-</span></div>
  </div>

  <div class="panel">
    <h3>I2C Scanner &amp; Address Config</h3>
    <button class="scanBtn" onclick="scanI2C()">Scan I2C Bus</button>
    <div id="scanResult">No scan yet.</div>

    <div class="addrRow">
      <span>MPU6050 address:</span>
      <select id="mpuSelect"><option value="">-- scan first --</option></select>
    </div>
    <div class="addrRow">
      <span>BMP280 address:</span>
      <select id="bmpSelect"><option value="">-- scan first --</option></select>
    </div>
    <button class="saveBtn" onclick="saveAddr('mpu')">Save MPU Address</button>
    <button class="saveBtn" onclick="saveAddr('bmp')">Save BMP Address</button>
    <div id="scanStatus"></div>
  </div>

<script>
function cmd(dir){
  fetch('/move?dir=' + dir)
    .then(r => r.text())
    .then(t => { document.getElementById('status').innerText = 'Move: ' + t; });
}

function poll(){
  fetch('/data').then(r => r.json()).then(d => {
    document.getElementById('acc').innerText = d.ax.toFixed(2)+' / '+d.ay.toFixed(2)+' / '+d.az.toFixed(2);
    document.getElementById('gyro').innerText = d.gx.toFixed(2)+' / '+d.gy.toFixed(2)+' / '+d.gz.toFixed(2);
    document.getElementById('tilt').innerText = d.tiltX.toFixed(1)+' / '+d.tiltY.toFixed(1);
    document.getElementById('dhtTemp').innerText = d.dhtTemp.toFixed(1)+' °C';
    document.getElementById('dhtHum').innerText = d.dhtHum.toFixed(1)+' %';
    document.getElementById('bmpTemp').innerText = d.bmpTemp.toFixed(1)+' °C';
    document.getElementById('pressure').innerText = d.pressure.toFixed(1)+' hPa';
    document.getElementById('altitude').innerText = d.altitude.toFixed(1)+' m';
    document.getElementById('mpuStatus').innerText = d.mpuOK ? ('OK (0x'+d.mpuAddr+')') : ('NOT FOUND (0x'+d.mpuAddr+')');
    document.getElementById('bmpStatus').innerText = d.bmpOK ? ('OK (0x'+d.bmpAddr+')') : ('NOT FOUND (0x'+d.bmpAddr+')');
  });
}
setInterval(poll, 800);
poll();

function scanI2C(){
  document.getElementById('scanResult').innerText = 'Scanning...';
  fetch('/scan').then(r => r.json()).then(d => {
    if (d.found.length === 0) {
      document.getElementById('scanResult').innerText = 'No I2C devices found. Check wiring.';
      return;
    }
    document.getElementById('scanResult').innerText = 'Found: ' + d.found.map(a => '0x'+a).join(', ');
    const mpuSel = document.getElementById('mpuSelect');
    const bmpSel = document.getElementById('bmpSelect');
    mpuSel.innerHTML = '';
    bmpSel.innerHTML = '';
    d.found.forEach(a => {
      mpuSel.innerHTML += '<option value="'+a+'">0x'+a+'</option>';
      bmpSel.innerHTML += '<option value="'+a+'">0x'+a+'</option>';
    });
  });
}

function saveAddr(sensor){
  const sel = document.getElementById(sensor + 'Select');
  const addr = sel.value;
  if (!addr) { alert('Scan first and pick an address.'); return; }
  fetch('/setaddr?sensor=' + sensor + '&addr=0x' + addr)
    .then(r => r.text())
    .then(t => { document.getElementById('scanStatus').innerText = t; });
}
</script>
</body>
</html>
)rawliteral";

// ================= WEB HANDLERS =================
void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleMove() {
  String dir = server.arg("dir");
  if (dir == "forward")  { moveForward();  g_lastMove = "FORWARD"; }
  else if (dir == "backward") { moveBackward(); g_lastMove = "BACKWARD"; }
  else if (dir == "left")     { turnLeft();     g_lastMove = "LEFT"; }
  else if (dir == "right")    { turnRight();    g_lastMove = "RIGHT"; }
  else                        { motorsStop();   g_lastMove = "STOP"; }
  server.send(200, "text/plain", g_lastMove);
}

void handleData() {
  char mpuHex[3], bmpHex[3];
  sprintf(mpuHex, "%02X", mpuAddr);
  sprintf(bmpHex, "%02X", bmpAddr);

  String json = "{";
  json += "\"ax\":" + String(g_ax, 3) + ",";
  json += "\"ay\":" + String(g_ay, 3) + ",";
  json += "\"az\":" + String(g_az, 3) + ",";
  json += "\"gx\":" + String(g_gx, 3) + ",";
  json += "\"gy\":" + String(g_gy, 3) + ",";
  json += "\"gz\":" + String(g_gz, 3) + ",";
  json += "\"tiltX\":" + String(g_tiltX, 2) + ",";
  json += "\"tiltY\":" + String(g_tiltY, 2) + ",";
  json += "\"dhtTemp\":" + String(g_temp_dht, 1) + ",";
  json += "\"dhtHum\":" + String(g_hum_dht, 1) + ",";
  json += "\"bmpTemp\":" + String(g_temp_bmp, 1) + ",";
  json += "\"pressure\":" + String(g_pressure, 1) + ",";
  json += "\"altitude\":" + String(g_altitude, 1) + ",";
  json += "\"move\":\"" + g_lastMove + "\",";
  json += "\"mpuOK\":" + String(mpuOK ? "true" : "false") + ",";
  json += "\"bmpOK\":" + String(bmpOK ? "true" : "false") + ",";
  json += "\"mpuAddr\":\"" + String(mpuHex) + "\",";
  json += "\"bmpAddr\":\"" + String(bmpHex) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// scans the whole I2C bus (addresses 1-126) and returns found devices as JSON
void handleScan() {
  String json = "{\"found\":[";
  bool first = true;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      char hexStr[3];
      sprintf(hexStr, "%02X", addr);
      if (!first) json += ",";
      json += "\"" + String(hexStr) + "\"";
      first = false;
    }
  }
  json += "]}";
  server.send(200, "application/json", json);
}

// sets which I2C address to use for a given sensor, saves to EEPROM, reinits it
void handleSetAddr() {
  String sensor = server.arg("sensor");   // "mpu" or "bmp"
  String addrStr = server.arg("addr");    // e.g. "0x68"
  if (sensor == "" || addrStr == "") {
    server.send(400, "text/plain", "Missing sensor or addr parameter");
    return;
  }
  uint8_t addr = (uint8_t) strtol(addrStr.c_str(), NULL, 16);

  saveAddressToEEPROM(sensor, addr);

  if (sensor == "mpu") {
    initMPU();
    server.send(200, "text/plain",
      mpuOK ? ("Saved. MPU6050 now using 0x" + addrStr + " (connected)")
            : ("Saved 0x" + addrStr + ", but sensor did not respond - check wiring"));
  } else if (sensor == "bmp") {
    initBMP();
    server.send(200, "text/plain",
      bmpOK ? ("Saved. BMP280 now using 0x" + addrStr + " (connected)")
            : ("Saved 0x" + addrStr + ", but sensor did not respond - check wiring"));
  } else {
    server.send(400, "text/plain", "Unknown sensor - use 'mpu' or 'bmp'");
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  // motor pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  motorsStop();

  // load previously-saved I2C addresses (or defaults if none saved yet)
  loadAddressesFromEEPROM();

  // I2C + sensors
  Wire.begin(I2C_SDA, I2C_SCL);

  initMPU();
  initBMP();

  dht.begin();

  // WiFi AP mode
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress ip = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(ip);

  // routes
  server.on("/", handleRoot);
  server.on("/move", handleMove);
  server.on("/data", handleData);
  server.on("/scan", handleScan);
  server.on("/setaddr", handleSetAddr);
  server.begin();
  Serial.println("Web server started");
}

// ================= LOOP =================
void loop() {
  server.handleClient();

  if (millis() - lastSensorRead > SENSOR_INTERVAL) {
    readSensors();
    lastSensorRead = millis();
  }
}
