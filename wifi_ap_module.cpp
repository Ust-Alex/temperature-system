#include "wifi_ap_module.h"
#include "system_config.h"
#include <WiFi.h>
#include <WebSocketsServer.h>

// ТОЧНО КАК В ТЕСТЕ
#ifndef WIFI_AP_SSID
#define WIFI_AP_SSID      "Temperature_System"
#endif

#ifndef WIFI_AP_PASSWORD  
#define WIFI_AP_PASSWORD  "termo1234"
#endif

// ТОЧНО КАК В ТЕСТЕ
WiFiServer server(80);
WebSocketsServer webSocket(81);
bool wifiStarted = false;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    // Пусто
}

// ТОЧНАЯ КОПИЯ setup() ИЗ ТЕСТА
void wifi_ap_setup() {
    Serial.println("\n=== WI-FI AP ===");
    
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
    
    Serial.print("SSID: ");
    Serial.println(WIFI_AP_SSID);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
    
    server.begin();
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    
    wifiStarted = true;
    Serial.println("=====================\n");
}

// ТОЧНАЯ КОПИЯ loop() ИЗ ТЕСТА
void wifi_ap_loop() {
    if(!wifiStarted) return;
    
    // ТОЧНО КАК В ТЕСТЕ
    WiFiClient client = server.available();
    
    if(client) {
        Serial.println("New client connected!");
        
        // Ждём запрос ТОЧНО КАК В ТЕСТЕ
        while(client.connected() && !client.available()) {
            delay(1);
        }
        
        // Читаем запрос ТОЧНО КАК В ТЕСТЕ
        String request = client.readStringUntil('\r');
        Serial.print("Request: ");
        Serial.println(request);
        
        // ОТВЕТ ТОЧНО КАК В ТЕСТЕ (с большим шрифтом)
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html; charset=UTF-8");
        client.println("Connection: close");
        client.println();
        
        // HTML ТОЧНО КАК В ТЕСТЕ (большой шрифт)
        client.println("<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'></head>");
        client.println("<body style='margin:0; padding:20px; background:black; color:white;'>");
        client.println("<div style='font-size: 60px; font-weight: bold; text-align: center; margin: 20px;'>");
        client.println("ESP32 РАБОТАЕТ!");
        client.println("</div>");
        client.println("<div style='font-size: 40px; text-align: center;'>");
        client.println("Если видите этот текст");
        client.println("<br>Wi-Fi и HTTP работают");
        client.println("</div>");
        client.println("<div style='font-size: 30px; text-align: center; margin-top: 40px; color: #4CAF50;'>");
        client.println("УСПЕХ! ✅");
        client.println("</div>");
        client.println("</body></html>");
        
        client.stop();
        Serial.println("Page sent (BIG FONT)\n");
    }
    
    // WebSocket (пока пусто)
    webSocket.loop();
    
    delay(100);
}

// Заглушка
void wifi_ap_send_temperatures() {
    // Пока не используется
}