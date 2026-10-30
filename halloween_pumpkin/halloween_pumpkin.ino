/*
  ไฟฟักทองฮาโลวีน ไหวเหมือนเปลวเทียน — ESP32 + LED WS2812B
  K iTech Review (TikTok) · Kham iTech Lab (YouTube / Facebook)

  ต่อสาย
    WS2812B 5V   -> 5V ของพาวเวอร์แบงก์ (หรือขา 5V/VIN ของบอร์ด)
    WS2812B GND  -> GND (ต่อกราวด์ร่วมกับ ESP32)
    WS2812B DIN  -> GPIO 5 ผ่านตัวต้านทาน 330 โอห์ม

  เปลี่ยนสีจากมือถือ
    ต่อ Wi-Fi บ้านไม่ได้ บอร์ดจะเปิด Wi-Fi ของตัวเองชื่อ "Pumpkin-ESP32" (รหัส halloween)
    แล้วเปิด http://192.168.4.1 ในมือถือ
    ถ้าต่อ Wi-Fi บ้านได้ ดู IP ใน Serial Monitor หรือเปิด http://pumpkin.local

  ไลบรารี: Adafruit NeoPixel (ติดตั้งจาก Library Manager)
  บอร์ด: ESP32 รุ่นไหนก็ได้ (ESP32, S3, C3) ใช้ได้ทั้ง ESP32 core 2.x และ 3.x
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Adafruit_NeoPixel.h>

// ---------- ตั้งค่า ----------
const char *WIFI_SSID = "";          // ใส่ชื่อ Wi-Fi บ้าน (เว้นว่างไว้ = ใช้ Wi-Fi ของบอร์ดเอง)
const char *WIFI_PASS = "";
const char *AP_SSID   = "Pumpkin-ESP32";
const char *AP_PASS   = "halloween";   // อย่างน้อย 8 ตัวอักษร

const int  LED_PIN    = 5;
const int  NUM_LEDS   = 12;            // วง 12 ดวง เปลี่ยนตามของที่มี
const uint8_t MAX_BRIGHT = 160;        // จำกัดความสว่าง ไม่ให้กินไฟเกินพาวเวอร์แบงก์

// ---------- สี ----------
enum Mode { FLAME, PURPLE, GREEN, OFF };
Mode mode = FLAME;

struct Palette { uint8_t r1, g1, b1, r2, g2, b2; };   // สีตอนมืดสุด -> สีตอนสว่างสุด
const Palette PALETTES[] = {
  {255, 40, 0,   255, 150, 20},   // FLAME: ส้มแดง -> เหลืองอุ่น
  {90, 0, 160,   200, 60, 255},   // PURPLE
  {0, 120, 10,   80, 255, 40},    // GREEN (เขียวผี)
};

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);

float level[NUM_LEDS];    // ความสว่างตอนนี้ 0..1
float target[NUM_LEDS];   // ความสว่างที่กำลังไหลไปหา
unsigned long lastFrame = 0;

// ---------- เปลวไฟ ----------
void flickerStep() {
  if (mode == OFF) {
    strip.clear();
    strip.show();
    return;
  }
  const Palette &p = PALETTES[mode];
  for (int i = 0; i < NUM_LEDS; i++) {
    // สุ่มเป้าหมายใหม่เป็นระยะ ทำให้แต่ละดวงไหวไม่พร้อมกัน
    if (random(100) < 12) target[i] = random(35, 101) / 100.0;
    // บางครั้งวูบลงแรง ๆ เหมือนลมพัดเทียน
    if (random(1000) < 4) target[i] = 0.15;
    level[i] += (target[i] - level[i]) * 0.25;

    float t = level[i];
    uint8_t r = p.r1 + (p.r2 - p.r1) * t;
    uint8_t g = p.g1 + (p.g2 - p.g1) * t;
    uint8_t b = p.b1 + (p.b2 - p.b1) * t;
    float k = 0.25 + 0.75 * t;   // มืดลงตอนวูบ
    strip.setPixelColor(i, strip.Color(r * k, g * k, b * k));
  }
  strip.show();
}

// ---------- หน้าเว็บ ----------
const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="th"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ไฟฟักทอง</title>
<style>
 body{margin:0;font-family:sans-serif;background:#120a1f;color:#fff;text-align:center;padding:24px}
 h1{font-size:26px;margin:8px 0 20px}
 button{display:block;width:100%;max-width:320px;margin:12px auto;padding:18px;font-size:20px;
        border:0;border-radius:14px;color:#fff;cursor:pointer}
 .f{background:#e8690b}.p{background:#7b2cbf}.g{background:#1f9d3a}.o{background:#444}
 #s{margin-top:16px;color:#bbb}
</style></head><body>
<h1>🎃 ไฟฟักทองฮาโลวีน</h1>
<button class="f" onclick="go('flame')">เปลวเทียน</button>
<button class="p" onclick="go('purple')">ม่วง</button>
<button class="g" onclick="go('green')">เขียวผี</button>
<button class="o" onclick="go('off')">ปิดไฟ</button>
<div id="s"></div>
<script>
function go(m){fetch('/mode?m='+m).then(r=>r.text()).then(t=>document.getElementById('s').textContent=t)}
</script></body></html>
)HTML";

void handleRoot() { server.send_P(200, "text/html; charset=utf-8", PAGE); }

void handleMode() {
  String m = server.arg("m");
  if (m == "flame")       mode = FLAME;
  else if (m == "purple") mode = PURPLE;
  else if (m == "green")  mode = GREEN;
  else if (m == "off")    mode = OFF;
  else { server.send(400, "text/plain; charset=utf-8", "ไม่รู้จักโหมดนี้"); return; }
  server.send(200, "text/plain; charset=utf-8", String("เปลี่ยนเป็น ") + m + " แล้ว");
}

// ---------- Wi-Fi ----------
void startWiFi() {
  if (strlen(WIFI_SSID) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("ต่อ Wi-Fi บ้าน");
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
      delay(250);
      Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      if (MDNS.begin("pumpkin")) Serial.println("หรือเปิด http://pumpkin.local");
      return;
    }
    Serial.println("ต่อไม่ได้ เปิด Wi-Fi ของบอร์ดแทน");
  }
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("ต่อ Wi-Fi ชื่อ ");
  Serial.print(AP_SSID);
  Serial.print(" แล้วเปิด http://");
  Serial.println(WiFi.softAPIP());
}

void setup() {
  Serial.begin(115200);
  delay(200);
  randomSeed(esp_random());

  strip.begin();
  strip.setBrightness(MAX_BRIGHT);
  strip.clear();
  strip.show();
  for (int i = 0; i < NUM_LEDS; i++) level[i] = target[i] = 0.6;

  startWiFi();
  server.on("/", handleRoot);
  server.on("/mode", handleMode);
  server.begin();
}

void loop() {
  server.handleClient();
  if (millis() - lastFrame >= 30) {   // ราว 33 เฟรมต่อวินาที
    lastFrame = millis();
    flickerStep();
  }
}
