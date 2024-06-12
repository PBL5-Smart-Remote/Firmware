#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Servo.h>

ESP8266WebServer server(80);
const char *ssid = "ESP8266-Config-34";
const char *password = "12345678";
const char *fingerPrint = "7D:E6:03:03:99:BB:69:6E:07:3E:0E:DE:50:18:A1:DA:6B:57:92:B6";

struct Device
{
  String pin;
  bool isConnected;
  int status;
  String id;
};
Device devices[4];

const String SERVER_BACKEND = "https://api.tugino.com:4501";
const String ESP_ID = "6f67a0a3-2a7f-4e92-b38a-2d828e647850";
const int TIME_OUT = 1000 * 15;
// STORED_WIFI, NOT_STORED_WIFI, CONNECTING_WIFI, NOT_CONNECTED_WIFI, CONNECTED_WIFI
String ESP_STATE = "NOT_STORED_WIFI"; // Khởi tạo trạng thái ban đầu là không lưu
// SETUPED
String DATA_STATE = "NOT_SETUP";

// Servo
Servo myservo;

// DC
int in1 = D3;
int in2 = D4;
int e = D6;

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(512);
  WiFi.mode(WIFI_AP_STA);
  String storedSSID, storedPass;
  if (readWiFiConfig(storedSSID, storedPass))
  {
    ESP_STATE = "STORED_WIFI"; // Đã lưu thông tin WiFi
    WiFi.begin(storedSSID.c_str(), storedPass.c_str());
    Serial.println("Connecting to WiFi...");
    ESP_STATE = "CONNECTING_WIFI"; // Đang thử kết nối WiFi
    int timeBreak = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
      if (timeBreak >= TIME_OUT)
      {
        ESP_STATE = "NOT_CONNECTED_WIFI"; // Không thể kết nối WiFi
        Serial.println("Not found Wifi....");
        break;
      }
      Serial.print(".");
      delay(500);
      timeBreak += 500;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      ESP_STATE = "CONNECTED_WIFI"; // Đã kết nối WiFi
      if (setupID())
      {
        DATA_STATE = "SETUPED"; // Đã cài đặt thông tin thiết bị
      }
      Serial.println("Connected to WiFi");
    }

    pinMode(D1, OUTPUT);
    // pinMode(D2, OUTPUT);
    myservo.attach(D2, 500, 2400);
    pinMode(D5, OUTPUT);

    //DC
    
  }

  if (ESP_STATE == "NOT_STORED_WIFI" || ESP_STATE == "NOT_CONNECTED_WIFI")
  {
    WiFi.softAP(ssid, password); // Phát AP để cấu hình
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    setupWebServer();
  }
}

void loop()
{
  if (ESP_STATE != "CONNECTED_WIFI")
  {
    // Nếu chưa kết nối WiFi, phục vụ các yêu cầu trên web server để cấu hình WiFi
    server.handleClient();
  }

  if (ESP_STATE == "CONNECTED_WIFI" && DATA_STATE == "SETUPED")
  {
    // Khi đã kết nối WiFi và đã cài đặt thông tin thiết bị
    getStatusDevices();
    checkDevices();
    delay(500);
  }
  Serial.println(DATA_STATE);
  if (ESP_STATE == "CONNECTED_WIFI" && DATA_STATE != "SETUPED")
  {
    if (setupID())
    {
      DATA_STATE = "SETUPED";
    }
  }
}

void checkDevices()
{
  for (int i = 0; i < 4; i++)
  {
    Serial.println(devices[i].pin + " " + devices[i].id + " " + devices[i].status);
    if (devices[i].pin == "D1")
    {
      handleRelay(D1, devices[i].status);
    }
    else if (devices[i].pin == "D2")
    {
      handleServo(devices[i].status);
    }
    else if (devices[i].pin == "D5")
    {
      handleRelay(D5, devices[i].status);
    }
    else if (devices[i].pin == "D6")
    {
      handleDC(devices[i].status);
    }
  }
}

int oldAngle = 0;
bool openAlready = false;
void handleServo(int status) {
  Serial.println(status);
  if (status) {
      myservo.write(100);
      openAlready = true;
  } else {
      myservo.write(14);
      openAlready = false;
    
  }
}

void handleRelay(int pinRelay, int status)
{
  digitalWrite(pinRelay, status);
}



void handleDC(int status){
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(e, OUTPUT);

  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  if(status){
    analogWrite(e, 120);
  }else{
    digitalWrite(in2, LOW);
  }            
}

void setupWebServer()
{
  // Trang cấu hình WiFi
  server.on("/", HTTP_GET, []()
            { server.send(200, "text/html", "<h1>Setup WiFi</h1><form action='/setup' method='post'>SSID: <input type='text' name='ssid'><br>Password: <input type='password' name='password'><br><input type='submit' value='Connect'></form>"); });

  server.on("/setup", HTTP_POST, []() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");

    if (ssid.length() == 0 || password.length() == 0) {
        server.send(404, "text/plain", "Missing WiFi credentials");
        return;
    }

    saveWiFiConfig(ssid, password);
    server.send(200, "text/plain", "WiFi credentials saved. Rebooting...");
    delay(1000);
    ESP.restart(); 
});


  server.begin();
}

void saveWiFiConfig(String ssid, String password)
{
  // Lưu SSID
  for (int i = 0; i < ssid.length(); ++i)
  {
    EEPROM.write(i, ssid[i]);
  }
  EEPROM.write(ssid.length(), '\0'); // Kết thúc chuỗi

  // Lưu Password
  for (int i = 0; i < password.length(); ++i)
  {
    EEPROM.write(32 + i, password[i]); // Bắt đầu từ vị trí 32
  }
  EEPROM.write(32 + password.length(), '\0'); // Kết thúc chuỗi

  EEPROM.commit();
}

bool readWiFiConfig(String &ssid, String &password)
{
  char ch;
  bool validData = false; // Biến kiểm tra liệu có dữ liệu hợp lệ

  // Đọc SSID
  for (int i = 0; i < 32; ++i)
  {
    ch = EEPROM.read(i);
    if (ch == '\0')
      break; // Kết thúc chuỗi khi gặp ký tự null
    if (ch != 0xFF)
      validData = true; // Kiểm tra liệu có ký tự hợp lệ
    ssid += ch;
  }

  // Chỉ tiếp tục đọc Password nếu đã tìm thấy SSID hợp lệ
  if (validData)
  {
    validData = false; // Reset lại cho việc kiểm tra mật khẩu
    // Đọc Password
    for (int i = 32; i < 64; ++i)
    {
      ch = EEPROM.read(i);
      if (ch == '\0')
        break; // Kết thúc chuỗi khi gặp ký tự null
      if (ch != 0xFF)
        validData = true; // Kiểm tra liệu có ký tự hợp lệ
      password += ch;
    }
  }

  // Chỉ trả về true nếu cả SSID và Password đều hợp lệ
  return validData && ssid.length() > 0 && password.length() > 0;
}

void getStatusDevices()
{
  if (WiFi.status() == WL_CONNECTED)
  { // Kiểm tra kết nối WiFi
    WiFiClientSecure client;
    client.setFingerprint(fingerPrint);
    HTTPClient http;

    // Tạo URL cho yêu cầu GET
    String url = SERVER_BACKEND + "/esps/" + ESP_ID;
    Serial.println(url);

    // Khởi tạo yêu cầu HTTP GET
    http.begin(client, url.c_str()); // Sử dụng URL đã tạo

    // Gửi yêu cầu GET
    int httpCode = http.GET();

    // Kiểm tra phản hồi
    if (httpCode > 0)
    {
      String payload = http.getString(); // Lấy nội dung phản hồi
      Serial.println("Received response:");
      Serial.println(payload);
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);

      JsonArray devicesArray = doc["devices"];
      for (int i = 0; i < devicesArray.size(); i++)
      {
        JsonObject obj = devicesArray[i];
        devices[i].isConnected = obj["isConnected"].as<bool>();
        devices[i].status = obj["status"].as<int>();
      }
    }
    else
    {
      Serial.print("Error on sending GET: ");
      Serial.println(http.errorToString(httpCode).c_str());
    }

    http.end(); // Đóng kết nối
  }
  else
  {
    Serial.println("Not connected to WiFi");
  }
}

bool setupID()
{
  if (WiFi.status() == WL_CONNECTED)
  { // Kiểm tra kết nối WiFi
    WiFiClientSecure client;
    client.setFingerprint(fingerPrint);
    HTTPClient http;

    // Tạo URL cho yêu cầu GET
    String url = SERVER_BACKEND + "/esps/connect/" + ESP_ID + "?numDevices=3";
    Serial.println(url);

    // Khởi tạo yêu cầu HTTP GET
    http.begin(client, url.c_str()); // Sử dụng URL đã tạo

    // Gửi yêu cầu GET
    int httpCode = http.GET();

    Serial.println(httpCode);

    // Kiểm tra phản hồi
    if (httpCode > 0)
    {
      String payload = http.getString(); // Lấy nội dung phản hồi
      Serial.println("Received response:");
      Serial.println(payload);
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);

      JsonArray devicesArray = doc["devices"];
      for (int i = 0; i < devicesArray.size(); i++)
      {
        JsonObject obj = devicesArray[i];
        devices[i].pin = obj["pin"].as<String>();
        devices[i].isConnected = obj["isConnected"].as<bool>();
        devices[i].status = obj["status"].as<int>();
        devices[i].id = obj["_id"].as<String>();

        Serial.println(devices[i].pin + " " + devices[i].status);
      }
    }
    else
    {
      Serial.print("Error on sending GET: ");
      Serial.println(http.errorToString(httpCode).c_str());
    }

    http.end();  // Đóng kết nối
    return true; // Trả về true nếu yêu cầu thành công
  }
  else
  {
    Serial.println("Not connected to WiFi");
    return false; // Trả về false nếu không kết nối được WiFi
  }
}