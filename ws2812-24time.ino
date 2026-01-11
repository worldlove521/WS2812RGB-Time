#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>

// ===================== 基础配置参数 =====================
const char* ssid = "你的Wi-Fi名称;
const char* password = "你的Wi-Fi密码";

#define LED_PIN     D2
#define LED_COUNT   24
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

ESP8266WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp.aliyun.com", 8 * 3600, 60000);

// ===================== 全局模式与状态配置 =====================
enum RunMode {
  CLOCK_MODE = 1,    
  AMBIENT_MODE = 2,  
  GRAFFITI_MODE = 3  
};

// 氛围灯速度变量（10-200，值越大速度越快）
int ambientSpeed = 50;       

// 全局step变量（仅定义1次，解决重复定义报错）
static int step = 0;

RunMode currentMode = CLOCK_MODE;
int currentClockTheme = 0;
int currentAmbientEffect = 0;
int currentWebTheme = 0;
int ledBrightness = 60;
uint32_t graffitiColors[LED_COUNT] = {0};
String currentStatus = "就绪：时钟模式-主题1 | 亮度：60";

// ===================== 10种时钟主题配置 =====================
struct ClockTheme {
  uint32_t hourColor;
  uint32_t minuteColor;
  uint32_t secondColor;
  uint32_t positionColor;
};

ClockTheme clockThemes[10] = {
  {strip.Color(255, 0, 0), strip.Color(0, 255, 255), strip.Color(0, 0, 255), strip.Color(255, 255, 0)},
  {strip.Color(255, 0, 255), strip.Color(0, 255, 0), strip.Color(255, 255, 0), strip.Color(0, 255, 255)},
  {strip.Color(0, 255, 255), strip.Color(255, 0, 255), strip.Color(255, 0, 0), strip.Color(255, 255, 255)},
  {strip.Color(255, 165, 0), strip.Color(0, 128, 255), strip.Color(128, 0, 128), strip.Color(0, 255, 0)},
  {strip.Color(0, 0, 255), strip.Color(255, 255, 0), strip.Color(255, 0, 0), strip.Color(0, 255, 255)},
  {strip.Color(255, 255, 255), strip.Color(255, 192, 203), strip.Color(135, 206, 235), strip.Color(255, 215, 0)},
  {strip.Color(0, 255, 0), strip.Color(255, 0, 0), strip.Color(0, 0, 255), strip.Color(255, 255, 0)},
  {strip.Color(128, 128, 128), strip.Color(255, 255, 255), strip.Color(0, 0, 0), strip.Color(255, 0, 255)},
  {strip.Color(255, 255, 0), strip.Color(0, 255, 255), strip.Color(255, 0, 255), strip.Color(255, 0, 0)},
  {strip.Color(0, 128, 0), strip.Color(0, 0, 128), strip.Color(128, 0, 0), strip.Color(255, 255, 255)}
};

// ===================== 20种氛围灯配置 =====================
struct AmbientEffect {
  uint32_t colors[8];
  String name;
};

AmbientEffect ambientEffects[20] = {
  {{strip.Color(255,0,0), strip.Color(255,127,0), strip.Color(255,255,0), strip.Color(0,255,0), strip.Color(0,255,255), strip.Color(0,0,255), strip.Color(127,0,255), strip.Color(255,0,255)}, "彩虹流水"},
  {{strip.Color(255,0,0), strip.Color(255,63,0), strip.Color(255,127,0), strip.Color(255,191,0), strip.Color(255,255,0), strip.Color(191,255,0), strip.Color(127,255,0), strip.Color(63,255,0)}, "暖橙渐变"},
  {{strip.Color(0,0,255), strip.Color(0,63,255), strip.Color(0,127,255), strip.Color(0,191,255), strip.Color(0,255,255), strip.Color(0,255,191), strip.Color(0,255,127), strip.Color(0,255,63)}, "冷蓝渐变"},
  {{strip.Color(255,0,255), strip.Color(191,0,255), strip.Color(127,0,255), strip.Color(63,0,255), strip.Color(0,0,255), strip.Color(0,63,127), strip.Color(0,127,191), strip.Color(0,191,255)}, "紫蓝渐变"},
  {{strip.Color(0,255,0), strip.Color(63,255,0), strip.Color(127,255,0), strip.Color(191,255,0), strip.Color(255,255,0), strip.Color(255,191,0), strip.Color(255,127,0), strip.Color(255,63,0)}, "绿黄渐变"},
  {{strip.Color(255,0,150), strip.Color(0,255,255), strip.Color(255,0,200), strip.Color(0,200,255), strip.Color(255,0,255), strip.Color(0,150,255), strip.Color(200,0,255), strip.Color(0,255,200)}, "赛博粉蓝"},
  {{strip.Color(128,0,255), strip.Color(0,255,128), strip.Color(255,0,128), strip.Color(0,128,255), strip.Color(128,255,0), strip.Color(255,128,0), strip.Color(0,128,128), strip.Color(128,0,128)}, "赛博紫绿"},
  {{strip.Color(255,40,0), strip.Color(0,255,200), strip.Color(255,80,0), strip.Color(0,200,255), strip.Color(255,120,0), strip.Color(0,160,255), strip.Color(255,160,0), strip.Color(0,120,255)}, "赛博红青"},
  {{strip.Color(0,255,255), strip.Color(255,255,0), strip.Color(0,200,255), strip.Color(255,200,0), strip.Color(0,150,255), strip.Color(255,150,0), strip.Color(0,100,255), strip.Color(255,100,0)}, "赛博青黄"},
  {{strip.Color(255,0,255), strip.Color(255,255,0), strip.Color(200,0,255), strip.Color(200,200,0), strip.Color(150,0,255), strip.Color(150,150,0), strip.Color(100,0,255), strip.Color(100,100,0)}, "赛博紫黄"},
  {{strip.Color(255,0,0), strip.Color(200,0,0), strip.Color(150,0,0), strip.Color(100,0,0), strip.Color(50,0,0), strip.Color(100,0,0), strip.Color(150,0,0), strip.Color(200,0,0)}, "红色呼吸"},
  {{strip.Color(0,255,0), strip.Color(0,200,0), strip.Color(0,150,0), strip.Color(0,100,0), strip.Color(0,50,0), strip.Color(0,100,0), strip.Color(0,150,0), strip.Color(0,200,0)}, "绿色呼吸"},
  {{strip.Color(0,0,255), strip.Color(0,0,200), strip.Color(0,0,150), strip.Color(0,0,100), strip.Color(0,0,50), strip.Color(0,0,100), strip.Color(0,0,150), strip.Color(0,0,200)}, "蓝色呼吸"},
  {{strip.Color(255,0,255), strip.Color(200,0,200), strip.Color(150,0,150), strip.Color(100,0,100), strip.Color(50,0,50), strip.Color(100,0,100), strip.Color(150,0,150), strip.Color(200,0,200)}, "紫色呼吸"},
  {{strip.Color(255,255,0), strip.Color(200,200,0), strip.Color(150,150,0), strip.Color(100,100,0), strip.Color(50,50,0), strip.Color(100,100,0), strip.Color(150,150,0), strip.Color(200,200,0)}, "黄色呼吸"},
  {{strip.Color(255,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0)}, "红色跑马"},
  {{strip.Color(0,255,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0)}, "绿色跑马"},
  {{strip.Color(0,0,255), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0)}, "蓝色跑马"},
  {{strip.Color(255,0,255), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0)}, "紫色跑马"},
  {{strip.Color(255,255,255), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0), strip.Color(0,0,0)}, "白色扫描"}
};

// ===================== 时钟映射函数 =====================
const int positionLeds[] = {0, 6, 12, 18};
#define POSITION_COUNT 4
const int hourToLedMap[] = {12, 14, 16, 18, 20, 22, 0, 2, 4, 6, 8, 10};

int mapTimeToLed(int timeTick, bool isHour) {
  int ledIndex;
  if (isHour) {
    ledIndex = hourToLedMap[constrain(timeTick, 0, 11)];
  } else {
    ledIndex = (timeTick * 24) / 60;
    ledIndex = (ledIndex + 12) % 24;
    ledIndex = constrain(ledIndex, 0, 23);
  }
  return ledIndex;
}

void lightPositionLeds(uint32_t color) {
  for (int i = 0; i < POSITION_COUNT; i++) {
    strip.setPixelColor(positionLeds[i], color);
  }
}

// ===================== 氛围灯函数（适配速度变量） =====================
void runAmbientEffect(int effectIndex) {
  AmbientEffect effect = ambientEffects[constrain(effectIndex, 0, 19)];
  uint32_t* colors = effect.colors;
  static unsigned long lastUpdate = 0;

  // 用ambientSpeed控制灯效速度
  if (millis() - lastUpdate < ambientSpeed) return;
  lastUpdate = millis();

  switch (effectIndex) {
    case 0:
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(i, colors[(i + step) % 8]);
      }
      break;
    case 1 ... 4:
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(i, colors[(i * 8) / LED_COUNT]);
      }
      break;
    case 5 ... 8:
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(i, colors[(i + step) % 8]);
      }
      break;
    case 9 ... 13:
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(i, colors[step % 8]);
      }
      break;
    case 14 ... 18:
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(i, (i == step % LED_COUNT) ? colors[0] : strip.Color(0,0,0));
      }
      break;
    case 19:
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(i, (i >= step % LED_COUNT && i <= (step + 2) % LED_COUNT) ? colors[0] : strip.Color(0,0,0));
      }
      break;
  }

  strip.show();
  step++;
  if (step > 100) step = 0;
}

// ===================== 涂鸦模式函数 =====================
void initGraffiti() {
  for (int i = 0; i < LED_COUNT; i++) {
    graffitiColors[i] = strip.Color(0, 0, 0);
  }
  strip.show();
}

void updateGraffiti(int ledIndex, uint32_t color) {
  if (ledIndex >= 0 && ledIndex < LED_COUNT) {
    graffitiColors[ledIndex] = color;
    strip.setPixelColor(ledIndex, color);
    strip.show();
  }
}

void showGraffiti() {
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, graffitiColors[i]);
  }
  strip.show();
}

// ===================== Web服务器函数（含速度控制API） =====================
String readLittleFSFile(String path) {
  File file = LittleFS.open(path, "r");
  if (!file) return "404 Not Found";
  String content = file.readString();
  file.close();
  return content;
}

void handleRoot() {
  String html = readLittleFSFile("/index.html");
  server.send(200, "text/html", html);
}

void handleApi() {
  String action = server.arg("action");
  String response = "OK";

  if (action == "setMode") {
    int mode = server.arg("value").toInt();
    if (mode >= 1 && mode <= 3) {
      currentMode = (RunMode)mode;
      if (currentMode == CLOCK_MODE) timeClient.update();
      if (currentMode == AMBIENT_MODE) step = 0;
      if (currentMode == GRAFFITI_MODE) initGraffiti();
    }
  } else if (action == "setClockTheme") {
    currentClockTheme = constrain(server.arg("value").toInt(), 0, 9);
  } else if (action == "setAmbientEffect") {
    currentAmbientEffect = constrain(server.arg("value").toInt(), 0, 19);
  } else if (action == "setWebTheme") {
    currentWebTheme = constrain(server.arg("value").toInt(), 0, 4);
  } else if (action == "setBrightness") {
    ledBrightness = constrain(server.arg("value").toInt(), 0, 255);
    strip.setBrightness(ledBrightness);
    strip.show();
  } else if (action == "setAmbientSpeed") {
    ambientSpeed = constrain(server.arg("value").toInt(), 10, 200);
  } else if (action == "setGraffiti") {
    int ledIndex = server.arg("led").toInt();
    int r = server.arg("r").toInt();
    int g = server.arg("g").toInt();
    int b = server.arg("b").toInt();
    updateGraffiti(ledIndex, strip.Color(r, g, b));
  } else if (action == "getStatus") {
    String modeName = (currentMode == CLOCK_MODE) ? "时钟模式" : (currentMode == AMBIENT_MODE) ? "氛围灯模式" : "涂鸦模式";
    String detail = (currentMode == CLOCK_MODE) ? "主题" + String(currentClockTheme + 1) : (currentMode == AMBIENT_MODE) ? "效果" + String(currentAmbientEffect + 1) + " | 速度：" + String(ambientSpeed) : "自定义涂鸦";
    currentStatus = "就绪：" + modeName + "-" + detail + " | 亮度：" + String(ledBrightness);
    response = currentStatus;
  } else {
    response = "Invalid Action";
  }

  server.send(200, "text/plain", response);
}

void initWebServer() {
  server.on("/", handleRoot);
  server.on("/api", handleApi);
  server.begin();
  Serial.println("[Web服务器] 启动成功，支持浏览器访问IP控制");
}

// ===================== 串口控制函数（核心新增） =====================
// 串口指令提示
void printSerialHelp() {
  Serial.println("\n======================================");
  Serial.println("          串口控制指令列表");
  Serial.println("======================================");
  Serial.println("1. 显示帮助：help");
  Serial.println("2. 切换模式：mode [1/2/3] （1=时钟 2=氛围灯 3=涂鸦）");
  Serial.println("3. 时钟主题：clock [0-9] （对应10种主题）");
  Serial.println("4. 氛围灯效果：ambient [0-19] （对应20种效果）");
  Serial.println("5. 调节亮度：brightness [0-255] （0=最暗 255=最亮）");
  Serial.println("6. 调节速度：speed [10-200] （值越大速度越快）");
  Serial.println("7. 清空涂鸦：clear");
  Serial.println("8. 显示状态：status");
  Serial.println("======================================\n");
}

// 解析串口指令
void parseSerialCommand() {
  if (Serial.available() <= 0) return; // 无串口数据则返回

  String command = Serial.readStringUntil('\n'); // 读取一行指令
  command.trim(); // 去除首尾空格、换行符
  command.toLowerCase(); // 转为小写，忽略大小写

  // 分割指令与参数（以空格分隔）
  int spaceIndex = command.indexOf(' ');
  String cmd = (spaceIndex > 0) ? command.substring(0, spaceIndex) : command;
  String param = (spaceIndex > 0) ? command.substring(spaceIndex + 1) : "";

  // 处理各类指令
  if (cmd == "help") {
    printSerialHelp();
  } else if (cmd == "mode") {
    int mode = param.toInt();
    if (mode >= 1 && mode <= 3) {
      currentMode = (RunMode)mode;
      if (currentMode == CLOCK_MODE) {
        timeClient.update();
        Serial.print("[模式切换] 已切换到时钟模式，当前主题：");
        Serial.println(currentClockTheme + 1);
      } else if (currentMode == AMBIENT_MODE) {
        step = 0;
        Serial.print("[模式切换] 已切换到氛围灯模式，当前效果：");
        Serial.println(ambientEffects[currentAmbientEffect].name);
      } else if (currentMode == GRAFFITI_MODE) {
        initGraffiti();
        Serial.println("[模式切换] 已切换到涂鸦模式，已清空画布");
      }
    } else {
      Serial.println("[错误] 模式参数无效，仅支持1/2/3");
    }
  } else if (cmd == "clock") {
    int theme = param.toInt();
    if (theme >= 0 && theme <= 9) {
      currentClockTheme = theme;
      Serial.print("[时钟主题] 已切换到主题");
      Serial.println(theme + 1);
    } else {
      Serial.println("[错误] 主题参数无效，仅支持0-9");
    }
  } else if (cmd == "ambient") {
    int effect = param.toInt();
    if (effect >= 0 && effect <= 19) {
      currentAmbientEffect = effect;
      Serial.print("[氛围灯效果] 已切换到：");
      Serial.println(ambientEffects[effect].name);
    } else {
      Serial.println("[错误] 效果参数无效，仅支持0-19");
    }
  } else if (cmd == "brightness") {
    int brightness = param.toInt();
    if (brightness >= 0 && brightness <= 255) {
      ledBrightness = brightness;
      strip.setBrightness(ledBrightness);
      strip.show();
      Serial.print("[亮度调节] 已设置亮度为：");
      Serial.println(brightness);
    } else {
      Serial.println("[错误] 亮度参数无效，仅支持0-255");
    }
  } else if (cmd == "speed") {
    int speed = param.toInt();
    if (speed >= 10 && speed <= 200) {
      ambientSpeed = speed;
      Serial.print("[速度调节] 已设置灯效速度为：");
      Serial.println(speed);
    } else {
      Serial.println("[错误] 速度参数无效，仅支持10-200");
    }
  } else if (cmd == "clear") {
    initGraffiti();
    Serial.println("[涂鸦操作] 已清空涂鸦画布");
  } else if (cmd == "status") {
    String modeName = (currentMode == CLOCK_MODE) ? "时钟模式" : (currentMode == AMBIENT_MODE) ? "氛围灯模式" : "涂鸦模式";
    String detail = (currentMode == CLOCK_MODE) ? "主题" + String(currentClockTheme + 1) : (currentMode == AMBIENT_MODE) ? "效果" + String(currentAmbientEffect + 1) + " | 速度：" + String(ambientSpeed) : "自定义涂鸦";
    currentStatus = "就绪：" + modeName + "-" + detail + " | 亮度：" + String(ledBrightness);
    Serial.println("[当前状态] " + currentStatus);
  } else if (cmd != "") {
    Serial.println("[错误] 未知指令，输入help查看所有指令");
  }
}

// ===================== 初始化与主循环 =====================
void setup() {
  // 1. 初始化串口（波特率115200，解决乱码）
  Serial.begin(115200);
  delay(500); // 等待串口稳定
  Serial.println("\n======================================");
  Serial.println("          ESP8266 圆环控制启动");
  Serial.println("======================================");

  // 2. 初始化灯带
  strip.begin();
  strip.setBrightness(ledBrightness);
  strip.show(); // 初始化为全黑
  Serial.println("[灯带] 初始化成功，灯珠数量：" + String(LED_COUNT));

  // 3. 初始化LittleFS
  if (!LittleFS.begin()) {
    Serial.println("[LittleFS] 挂载失败，尝试格式化...");
    LittleFS.format();
    if (!LittleFS.begin()) {
      Serial.println("[LittleFS] 格式化失败，Web功能不可用");
    } else {
      Serial.println("[LittleFS] 格式化成功，挂载完成");
    }
  } else {
    Serial.println("[LittleFS] 挂载成功");
  }

  // 4. 连接WiFi并显示IP
  Serial.print("[WiFi] 正在连接 " + String(ssid) + " ...");
  WiFi.begin(ssid, password);
  unsigned long wifiTimeout = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    // WiFi超时保护（10秒）
    if (millis() - wifiTimeout > 10000) {
      Serial.println("\n[WiFi] 连接超时，请检查账号密码");
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] 连接成功");
    Serial.println("[WiFi] IP地址：" + WiFi.localIP().toString()); // 显示核心IP地址
  }

  // 5. 初始化NTP时间客户端
  timeClient.begin();
  timeClient.update();
  Serial.println("[NTP] 时间客户端初始化成功");

  // 6. 初始化Web服务器
  initWebServer();

  // 7. 初始化涂鸦模式
  initGraffiti();

  // 8. 打印串口控制提示
  printSerialHelp();
  currentStatus = "就绪：时钟模式-主题1 | 亮度：" + String(ledBrightness);
  Serial.println("[系统] 初始化完成，" + currentStatus);
}

void loop() {
  // 1. 处理Web服务器请求
  server.handleClient();

  // 2. 处理串口指令（核心新增，解决串口控制）
  parseSerialCommand();

  // 3. 处理不同运行模式
  switch (currentMode) {
    case CLOCK_MODE: {
      timeClient.update();
      int currentHour24 = timeClient.getHours();
      int currentMinute = timeClient.getMinutes();
      int currentSecond = timeClient.getSeconds();
      int currentHour12 = currentHour24 % 12;

      static int lastHour12 = -1, lastMinute = -1, lastSecond = -1;
      if (currentHour12 != lastHour12 || currentMinute != lastMinute || currentSecond != lastSecond) {
        strip.clear();
        ClockTheme theme = clockThemes[currentClockTheme];
        lightPositionLeds(theme.positionColor);

        int hourLed = hourToLedMap[constrain((int)round(currentHour12 + (currentMinute / 60.0)), 0, 11)];
        int minuteLed = mapTimeToLed(currentMinute, false);
        int secondLed = mapTimeToLed(currentSecond, false);

        strip.setPixelColor(hourLed, theme.hourColor);
        strip.setPixelColor(minuteLed, theme.minuteColor);
        strip.setPixelColor((minuteLed + 1) % 24, theme.minuteColor);
        strip.setPixelColor(secondLed, theme.secondColor);

        strip.show();
        lastHour12 = currentHour12;
        lastMinute = currentMinute;
        lastSecond = currentSecond;
      }
      break;
    }
    case AMBIENT_MODE: {
      runAmbientEffect(currentAmbientEffect);
      break;
    }
    case GRAFFITI_MODE: {
      showGraffiti();
      delay(100);
      break;
    }
  }
}