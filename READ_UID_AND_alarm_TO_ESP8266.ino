#include <SPI.h>
#include <MFRC522.h>
#include <SoftwareSerial.h>
#include <Servo.h> 
#include "Timer.h"

#define SS_PIN 10
#define RST_PIN 9
#define _rxpin      4
#define _txpin      5
#define Buzzer_PIN   6
#define NormolOpen 3
#define SS_PIN 10
#define RST_PIN 9
#define SERVO_PIN 7 // 伺服馬達的控制訊號接腳 
#define SSID "tenten"
#define PASS "6425dong"
#define IP "184.106.153.149" // ThingSpeak IP Address: 184.106.153.149
String GET = "GET /update?key=HOE94KH5UUX5M2HT"; // GET /update?key=[THINGSPEAK_KEY]&field1=[data 1]&filed2=[data 2]...;

Timer t; //宣告一个定时器
struct RFIDTag {   // 定義結構
  byte uid[4];
  char *name;
};

struct RFIDTag tags[] = {  // 初始化結構資料，請自行修改RFID識別碼。
  {{0,0,0,0}, "illegal"},
  {{181, 128, 143, 56}, "THK_STUDENT_CARD"},
  {{197, 37, 179, 115}, "Mini_Tag"},
  {{85, 154, 184, 55}, "WBW_STUDENT_CARD"}
  
};

byte totalTags = sizeof(tags) / sizeof(RFIDTag);  //計算結構資料筆數
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;

volatile bool alarm_flag = false;
volatile bool open_flag = false;
//volatile bool master_open_flag = false;
volatile bool message_flag = false;
volatile bool timer_flag = false;
volatile bool interval_flag = false; //消息间隔flag
bool lockerSwitch = false; // 伺服馬達（模擬開關）的切換狀態，預設為「關」
byte count =0; //计算进入callback次数
byte message_order = 0xff; //发送第几个message

SoftwareSerial esp8266(_rxpin, _txpin ); // RX, TX
Servo servo;

void setup() {
  Serial.begin(115200);
  esp8266.begin(115200);
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522 
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  pinMode(Buzzer_PIN, OUTPUT);
  pinMode(NormolOpen, INPUT_PULLUP);
  connect_wifi_and_ip();
  t.every(6000, doSomething); //register a callback
  attachInterrupt(digitalPinToInterrupt(NormolOpen), alarm, FALLING);
  locker(lockerSwitch);
  Serial.println("Ready!!!");
  //open_flag = true;
}

void loop() {
 
  if(rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) { //寻找新卡
    byte *id = rfid.uid.uidByte;   // 取得卡片的UID
    byte idSize = rfid.uid.size;   // 取得UID的長度
    bool foundTag = false;    // 是否找到紀錄中的標籤，預設為「否」
    for (byte i = 1; i < totalTags; i++) {
      if (memcmp(tags[i].uid, id, idSize) == 0) {  // 比對陣列資料值
        Serial.println(tags[i].name);  // 顯示標籤的名稱
        foundTag = true;  // 設定成「找到標籤了！」
        open_flag = true;
        alarm_flag = false;
        message_order = i; //消息内容
        message_flag = 1;//启动发送标志位
        if(interval_flag == 0)
        {
           lockerSwitch = !lockerSwitch;  // 切換鎖的狀態
           locker(lockerSwitch);          // 開鎖或關鎖
        }
        break;  
      }
    }
    if (!foundTag) {  // 若掃描到紀錄之外的標籤，則顯示"Wrong card!"。
      Serial.println("Wrong card!");
      if (lockerSwitch && interval_flag == 0) {
          lockerSwitch = false;
          locker(lockerSwitch);
        }
      for (byte i = 0; i < idSize; i++) {
        Serial.print(id[i], DEC);      //顯示id
        Serial.print(" ");
      }
      //todo 监听窗口读取是否注册新卡
   }
     rfid.PICC_HaltA(); //让卡片进入停止状态
  }  
 
  
  if(timer_flag == 1) //启动定时器
  {
     t.update();
  }
  if(message_flag == 1 && message_order!=0xff && interval_flag == 0) //发送消息
  {
     interval_flag = 1;
     timer_flag = 1; //启动定时器flag
     message_flag =0;
     String num;
     if(message_order == 0)
     {
         num = "0";
     }
     else
     {
         num = "1";
     }
     SendOnCloud(String(tags[message_order].name),num);
     message_order = 0xff;
  }
  if (alarm_flag) {
    digitalWrite(Buzzer_PIN, HIGH);
    delay(100);
    digitalWrite(Buzzer_PIN, LOW);
    delay(100);
  }

}

void locker(bool toggle) { // 開鎖或關鎖 
  servo.attach(SERVO_PIN);  //将伺服马达物件附加在数位2脚
  if (toggle) { 
    servo.write(90); // 開鎖 
  } else { 
    servo.write(0); // 關鎖 
  } 
  delay(500); // 等伺服馬達轉到定位 
  servo.detach();
}
void alarm() {
  if(open_flag)
  {
    open_flag = false; //reset flag
    
  }
  else
  {
     alarm_flag = true;
     message_flag = 1; //撬门消息
     message_order = 0;
  }
    
}

void doSomething(){ //定时器callback
  if(count++)
  {
    count=0;
    timer_flag = 0;
    message_flag = 0; 
    interval_flag = 0;
  }
}
void connect_wifi_and_ip() {
    esp8266.print("+++");
    delay(200);
    esp8266.print("\n");
    esp8266.println("AT+CWMODE=1");
    delay(1000);
    esp8266.println("AT+CWDHCP_DEF=1,1");
    delay(1000);
    String cmd="AT+CWJAP_DEF=\"";
    cmd+=SSID;  
    cmd+="\",\"";
    cmd+=PASS;
    cmd+="\"";
    esp8266.println(cmd);
    delay(5000);
    String cmd_ip = "AT+SAVETRANSLINK=1,\""; //connect the ThingSpeak HTTP
    cmd_ip += IP;
    cmd_ip += "\",80,\"TCP\",10";  //AT+SAVETRANSLINK=1,"184.106.153.149",80,"TCP",10
    esp8266.println(cmd_ip);
    delay(6000);
    esp8266.println("AT+RST");
    delay(4000);
}

void SendOnCloud( String T, String H ) {

         String  cmd = GET + "&field1=" + T + "&field2=" + H +"\r\n";         
         esp8266.print(cmd);
         Serial.println(cmd);
          
}
