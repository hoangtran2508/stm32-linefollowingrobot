/* Điền thông tin từ Blynk Console vào 3 dòng này */
#define BLYNK_TEMPLATE_ID "TMPL6NFo_1ZxL"
#define BLYNK_TEMPLATE_NAME "do line"
#define BLYNK_AUTH_TOKEN "chQlCUCkvAUNM1_fQ50WpqF6dvyQvVEM"

/* Khai báo thư viện */
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

/* Thông tin WiFi nhà bạn */
char ssid[] = "IphoneH";
char pass[] = "123456789";

char current_cmd = 'S';
unsigned long last_send_time = 0;

void setup() {
  // Tốc độ baud phải khớp tuyệt đối với STM32 (115200)
  Serial.begin(115200);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}

// BLYNK_WRITE được gọi tự động khi trạng thái nút trên App thay đổi
BLYNK_WRITE(V0) { 
  if(param.asInt() == 1) current_cmd = 'F'; 
  else { current_cmd = 'S'; Serial.print('S'); } // Vừa nhấc tay là gửi 'S' phanh xe luôn
}

BLYNK_WRITE(V1) { 
  if(param.asInt() == 1) current_cmd = 'B'; 
  else { current_cmd = 'S'; Serial.print('S'); }
}

BLYNK_WRITE(V2) { 
  if(param.asInt() == 1) current_cmd = 'L'; 
  else { current_cmd = 'S'; Serial.print('S'); }
}

BLYNK_WRITE(V3) { 
  if(param.asInt() == 1) current_cmd = 'R'; 
  else { current_cmd = 'S'; Serial.print('S'); }
}

BLYNK_WRITE(V4) { 
  if(param.asInt() == 1) Serial.print('M'); // Nút đổi Mode chỉ cần bắn 1 lần
}

void loop() {
  Blynk.run();

  // Bắn lệnh liên tục mỗi 100ms khi đang giữ nút để nuôi Watchdog bên STM32
  if (current_cmd != 'S') {
    if (millis() - last_send_time > 100) {
      Serial.print(current_cmd);
      last_send_time = millis();
    }
  }
}
