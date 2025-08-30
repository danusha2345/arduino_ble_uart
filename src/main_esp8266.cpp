#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPS++.h>

// OLED дисплей настройки
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi настройки Access Point
const char* ap_ssid = "ESP8266-UART-Bridge";
const char* ap_password = "123456789";

// Веб-сервер и WebSocket
ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

// UART настройки и GPS парсер
#define RX_PIN D1  // GPIO5
#define TX_PIN D2  // GPIO4
#include <SoftwareSerial.h>
SoftwareSerial serialPort(RX_PIN, TX_PIN);
TinyGPSPlus gps;

// Статус подключений и GPS данные
bool clientConnected = false;
unsigned long lastDisplayUpdate = 0;
unsigned long lastDataReceived = 0;
unsigned long lastGpsParseTime = 0;
String lastMessage = "";
String uartBuffer = "";

// GPS данные
struct GPSData {
    double latitude = 0.0;
    double longitude = 0.0;
    double hdop = 999.99; // Horizontal Dilution of Precision
    double vdop = 999.99; // Vertical Dilution of Precision
    double horizontalAccuracy = 999.9; // Горизонтальная точность в метрах
    double verticalAccuracy = 999.9;   // Вертикальная точность в метрах
    int satellites = 0;
    int fixQuality = 0; // Тип фиксации: 0=Invalid, 1=GPS, 2=DGPS, 3=PPS, 4=RTK, 5=Float RTK, 6=Estimated, 7=Manual, 8=Simulation
    String fixType = "NO FIX"; // Текстовое описание типа фиксации
    bool valid = false;
    unsigned long lastUpdate = 0;
} gpsData;

// HTML страница для подключения
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP8266 UART Bridge</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }
        .container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }
        .status { padding: 10px; margin: 10px 0; border-radius: 5px; }
        .connected { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
        .disconnected { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
        textarea { width: 100%; height: 200px; margin: 10px 0; padding: 10px; }
        button { padding: 10px 20px; margin: 5px; font-size: 16px; cursor: pointer; }
        .send { background: #007bff; color: white; border: none; border-radius: 5px; }
        .clear { background: #6c757d; color: white; border: none; border-radius: 5px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP8266 UART Bridge</h1>
        <div id="status" class="status disconnected">Отключено</div>
        
        <h3>Полученные данные:</h3>
        <textarea id="received" readonly></textarea>
        
        <h3>Отправить данные:</h3>
        <textarea id="send" placeholder="Введите данные для отправки..."></textarea>
        <br>
        <button class="send" onclick="sendData()">Отправить</button>
        <button class="clear" onclick="clearReceived()">Очистить</button>
    </div>

    <script>
        let ws = null;
        
        function connect() {
            ws = new WebSocket('ws://' + window.location.hostname + ':81');
            
            ws.onopen = function(event) {
                document.getElementById('status').className = 'status connected';
                document.getElementById('status').innerHTML = 'Подключено';
            };
            
            ws.onmessage = function(event) {
                const received = document.getElementById('received');
                received.value += event.data + '\n';
                received.scrollTop = received.scrollHeight;
            };
            
            ws.onclose = function(event) {
                document.getElementById('status').className = 'status disconnected';
                document.getElementById('status').innerHTML = 'Отключено';
                setTimeout(connect, 3000);
            };
        }
        
        function sendData() {
            const sendBox = document.getElementById('send');
            if (ws && ws.readyState === WebSocket.OPEN && sendBox.value.trim()) {
                ws.send(sendBox.value);
                sendBox.value = '';
            }
        }
        
        function clearReceived() {
            document.getElementById('received').value = '';
        }
        
        connect();
        
        document.getElementById('send').addEventListener('keypress', function(e) {
            if (e.key === 'Enter' && e.ctrlKey) {
                sendData();
            }
        });
    </script>
</body>
</html>
)rawliteral";

void setupDisplay() {
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("SSD1306 allocation failed");
        return;
    }
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("ESP8266 UART Bridge");
    display.println("Starting...");
    display.display();
}

// Функция получения текстового описания типа фиксации
String getFixTypeString(int quality) {
    switch(quality) {
        case 0: return "NO FIX";
        case 1: return "GPS";
        case 2: return "DGPS";
        case 3: return "PPS";
        case 4: return "RTK";
        case 5: return "FLOAT";
        case 6: return "EST";
        case 7: return "MANUAL";
        case 8: return "SIM";
        default: return "UNKNOWN";
    }
}

// Ручной парсинг NMEA для извлечения точности и типа фиксации
void parseNmeaAccuracy(String nmea) {
    // Парсинг GST сообщений для точности
    if (nmea.startsWith("$GNGST") || nmea.startsWith("$GPGST")) {
        int commaIndex[8];
        int commaCount = 0;
        
        // Находим позиции запятых
        for (int i = 0; i < nmea.length() && commaCount < 8; i++) {
            if (nmea[i] == ',') {
                commaIndex[commaCount++] = i;
            }
        }
        
        if (commaCount >= 7) {
            // Извлекаем точность из GST: поля 6 и 7 - горизонтальная и вертикальная точность в метрах
            String hAccStr = nmea.substring(commaIndex[5] + 1, commaIndex[6]);
            String vAccStr = nmea.substring(commaIndex[6] + 1, commaIndex[7]);
            
            if (hAccStr.length() > 0 && hAccStr != "") {
                gpsData.horizontalAccuracy = hAccStr.toFloat();
            }
            if (vAccStr.length() > 0 && vAccStr != "") {
                gpsData.verticalAccuracy = vAccStr.toFloat();
            }
        }
    }
    
    // Парсинг GGA для дополнительной информации о точности и типе фиксации
    if (nmea.startsWith("$GNGGA") || nmea.startsWith("$GPGGA")) {
        int commaIndex[15];
        int commaCount = 0;
        
        for (int i = 0; i < nmea.length() && commaCount < 15; i++) {
            if (nmea[i] == ',') {
                commaIndex[commaCount++] = i;
            }
        }
        
        if (commaCount >= 8) {
            // Поле 6 в GGA - Quality indicator (тип фиксации)
            String qualityStr = nmea.substring(commaIndex[5] + 1, commaIndex[6]);
            if (qualityStr.length() > 0 && qualityStr != "") {
                gpsData.fixQuality = qualityStr.toInt();
                gpsData.fixType = getFixTypeString(gpsData.fixQuality);
            }
            
            // Поле 7 в GGA - количество спутников
            String satStr = nmea.substring(commaIndex[6] + 1, commaIndex[7]);
            if (satStr.length() > 0 && satStr != "") {
                gpsData.satellites = satStr.toInt();
            }
            
            // Поле 8 в GGA - HDOP
            String hdopStr = nmea.substring(commaIndex[7] + 1, commaIndex[8]);
            if (hdopStr.length() > 0 && hdopStr != "") {
                gpsData.hdop = hdopStr.toFloat();
            }
        }
    }
}

void parseGpsData() {
    if (millis() - lastGpsParseTime < 500) return; // Парсим 2 раза в секунду
    
    // Разбиваем буфер на отдельные NMEA строки
    int startPos = 0;
    for (int i = 0; i <= uartBuffer.length(); i++) {
        if (i == uartBuffer.length() || uartBuffer[i] == '\n') {
            if (i > startPos) {
                String nmeaLine = uartBuffer.substring(startPos, i);
                nmeaLine.trim();
                
                // Парсим точность из NMEA строк
                parseNmeaAccuracy(nmeaLine);
                
                // Также используем TinyGPS++ для основных данных
                for (int j = 0; j < nmeaLine.length(); j++) {
                    if (gps.encode(nmeaLine[j])) {
                        if (gps.location.isValid()) {
                            gpsData.latitude = gps.location.lat();
                            gpsData.longitude = gps.location.lng();
                            gpsData.valid = true;
                            gpsData.lastUpdate = millis();
                        }
                        
                        if (gps.satellites.isValid()) {
                            gpsData.satellites = gps.satellites.value();
                        }
                    }
                }
            }
            startPos = i + 1;
        }
    }
    
    uartBuffer = ""; // Очищаем буфер после парсинга
    lastGpsParseTime = millis();
}

void updateDisplay() {
    if (millis() - lastDisplayUpdate < 1000) return;
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(1);
    
    // Заголовок
    display.println("GPS UART Bridge");
    display.println("---------------");
    
    // GPS статус и координаты
    if (gpsData.fixQuality > 0 && (millis() - gpsData.lastUpdate < 10000)) {
        // Тип фиксации и количество спутников
        display.print("Fix: ");
        display.print(gpsData.fixType);
        display.print(" SAT: ");
        display.println(gpsData.satellites);
        
        // Широта
        display.print("Lat: ");
        display.println(gpsData.latitude, 6);
        
        // Долгота  
        display.print("Lon: ");
        display.println(gpsData.longitude, 6);
        
        // Горизонтальная и вертикальная точность
        display.print("H: ");
        display.print(gpsData.horizontalAccuracy, 1);
        display.print(" V: ");
        display.print(gpsData.verticalAccuracy, 1);
        display.println("m");
        
    } else {
        // Нет GPS сигнала - показываем тип фиксации и базовую информацию
        display.print("Fix: ");
        display.print(gpsData.fixType);
        display.print(" SAT: ");
        display.println(gpsData.satellites);
        display.println("");
        
        // WiFi статус
        display.print("AP: ");
        display.println(ap_ssid);
        display.print("IP: ");
        display.println(WiFi.softAPIP());
        display.print("WS: ");
        display.println(clientConnected ? "ON" : "OFF");
    }
    
    display.display();
    lastDisplayUpdate = millis();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            clientConnected = false;
            Serial.printf("WebSocket[%u] Disconnected!\n", num);
            break;
            
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            clientConnected = true;
            Serial.printf("WebSocket[%u] Connected from %d.%d.%d.%d\n", 
                         num, ip[0], ip[1], ip[2], ip[3]);
            break;
        }
        
        case WStype_TEXT:
            Serial.printf("WebSocket[%u] received text: %s\n", num, payload);
            lastMessage = String((char*)payload);
            lastDataReceived = millis();
            
            // Отправляем данные в UART
            serialPort.write(payload, length);
            break;
            
        case WStype_BIN:
            Serial.printf("WebSocket[%u] received binary length: %u\n", num, length);
            serialPort.write(payload, length);
            break;
            
        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\nStarting ESP8266 UART to WiFi Bridge...");
    
    // Инициализация дисплея  
    Wire.begin(D6, D5); // SDA=D6(GPIO14), SCL=D5(GPIO12)
    setupDisplay();
    
    // Запуск UART
    serialPort.begin(460800);
    Serial.println("UART initialized at 460800 baud");
    
    // Настройка WiFi Access Point
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    
    // Настройка веб-сервера
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", webpage);
    });
    
    server.begin();
    Serial.println("HTTP server started");
    
    // Запуск WebSocket сервера
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.println("WebSocket server started");
    
    // Обновление дисплея
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Ready!");
    display.print("Connect to: ");
    display.println(ap_ssid);
    display.print("IP: ");
    display.println(myIP);
    display.display();
}

void loop() {
    server.handleClient();
    webSocket.loop();
    
    // Читаем данные из UART в буфер
    if (serialPort.available()) {
        while (serialPort.available()) {
            char c = serialPort.read();
            
            // Добавляем в буфер для парсинга GPS
            uartBuffer += c;
            
            // Отправляем все данные в WebSocket если есть подключения
            if (clientConnected) {
                String charStr = String(c);
                webSocket.broadcastTXT(charStr);
            }
            
            // Сохраняем для отображения (только печатные символы)
            if (c != '\r' && c != '\n' && c >= 32 && c <= 126) {
                lastMessage += c;
                if (lastMessage.length() > 20) {
                    lastMessage = lastMessage.substring(lastMessage.length() - 20);
                }
            }
        }
        lastDataReceived = millis();
        
        // Ограничиваем размер буфера для предотвращения переполнения памяти
        if (uartBuffer.length() > 2048) {
            uartBuffer = uartBuffer.substring(uartBuffer.length() - 1024);
        }
    }
    
    // Периодический парсинг GPS данных (2 раза в секунду)
    parseGpsData();
    
    // Обновляем дисплей
    updateDisplay();
    
    yield(); // Для стабильности WiFi
}