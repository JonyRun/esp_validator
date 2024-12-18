#include <Arduino.h>
#include <ArduinoJson.h>
extern "C" {
	#include "freertos/FreeRTOS.h"
 
	#include "freertos/timers.h"
  #include "freertos/task.h"
}
#include <WiFi.h>
#include <MQTT.h>
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <Preferences.h>
#include <SPI.h>
#include <FS.h>
#include "SPIFFS.h" 
HardwareSerial SIM800(2);
const char ssid[] = "Wymega";
const char pass[] = "66006611";
const char* broker = "194.58.123.79";    //"mqtt.su";
IPAddress local_IP(192, 168, 1, 177);

// Задаем IP-адрес сетевого шлюза:
IPAddress subnet(255, 255, 255, 0);
IPAddress gateway(192, 168, 1, 1);
IPAddress primaryDNS(0, 0, 0, 0);   // опционально

IPAddress secondaryDNS(0, 0, 0, 0); // опционально
bool flag_xQueue1=false;
#define moneyPin 5 // Вывод Pulse
#define InhiBit 15
#define FORMAT_SPIFFS_IF_FAILED true
Preferences pref;
String terminalId="";
String cl_phone="";
String randNumber ="";
String Send_RES="true";
bool next_step=false;
 int rc;
 sqlite3 *db1;
 char *zErrMsg = 0;
int reconnect_wifi=0;
char sql_line[350];
char mqttUserName[] = "lera";         
char mqttPass[] = "kris";   
bool enable_topic=false;
bool Bill_validaror=false;
int count_send=0;
WiFiClient net;
MQTTClient client;
SemaphoreHandle_t wifi_mutex;
SemaphoreHandle_t serial_mutex;
SemaphoreHandle_t pref_mutex;
SemaphoreHandle_t sim800_mutex;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;
unsigned long lastMillis = 0;
QueueHandle_t xQueue1;

struct AMessage
{
    String phone="";
    String msg="";
}xMessageEmail;


void vTask_Queue1( void *pvParameters )
 {

 struct AMessage *pxMessage;
    xQueue1 = xQueueCreate( 10, sizeof( struct AMessage * ) );
if( xQueue1 == NULL )
    {
       flag_xQueue1=true; 
    }
vTaskDelete(NULL);
 }
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 char local_clientID[] ="sys_val_1";
 char local_topic_res[]="sys_val_1_res";
void connect() {
  Serial.print("checking wifi...");
	xSemaphoreTake(wifi_mutex, portMAX_DELAY);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
    reconnect_wifi++;
    if(reconnect_wifi>12)
    {ESP.restart();}
  }
 xSemaphoreGive(wifi_mutex);

  Serial.print("\nconnecting...");
    while (!client.connect(local_clientID,mqttUserName,mqttPass,false)) {
 
      Serial.print(".");
 
      delay(1000);  }

      //if(client.connect(clientID,mqttUserName,mqttPass)){xTaskCreate( STopic,"STopic",4128,(void *) &xTP1, 1, NULL );}
     	xSemaphoreTake(wifi_mutex, portMAX_DELAY);
      client.subscribe("mcount1");
     xSemaphoreGive(wifi_mutex);

      //

  Serial.println("\nconnected!");

  // client.unsubscribe("/hello");
}



typedef struct TaskParam_t {
  const char *topic;
  const char *msg;
  const char *incommsgX;
  const char *incomtopicX;
  int Mail_sub;
  const char *Mail_text;
  int regv_amount=0;
    sqlite3 *db;
  const char *sql;
} TaskParam;
TaskParam xTP1,xTP2;


const char* data = "Callback function called";
static int callback(void *data, int argc, char **argv, char **azColName) {
   int i;
  // Serial.printf("%s: ", (const char*)data);
   for (i = 0; i<argc; i++){
     	xSemaphoreTake(serial_mutex, portMAX_DELAY);
       Serial.printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
      // vTaskDelay(5); 
      xSemaphoreGive(serial_mutex);
      
   }
 //  Serial.printf("\n");
   return 0;
}


int db_open(const char *filename, sqlite3 **db) {
   int rc = sqlite3_open(filename, db);
   if (rc) {
       Serial.printf("Can't open database: %s\n", sqlite3_errmsg(*db));
       return rc;
   } else {
       Serial.printf("Opened database successfully\n");
   }
   return rc;
}
int db_exec(sqlite3 *db, const char *sql) {
   Serial.println(sql);
   long start = micros();
   int rc = sqlite3_exec(db, sql, callback, (void*)data, &zErrMsg);
   if (rc != SQLITE_OK) {
       Serial.printf("SQL error: %s\n", zErrMsg);
       sqlite3_free(zErrMsg);
   } else {
       Serial.printf("Operation done successfully\n");
   }
   Serial.print(F("Time taken:"));
   Serial.println(micros()-start);
   return rc;
}


void STask(void* pvParameters )
{
volatile TaskParam *pxTaskParam; 
pxTaskParam = (TaskParam *) pvParameters;
  
   rc = db_exec(pxTaskParam->db, pxTaskParam->sql);
   vTaskDelay(15); 
   if (rc != SQLITE_OK) {
       sqlite3_close(db1);  
     //  return;
   }

  vTaskDelete(NULL);
}



void Smsg(void* pvParameters )
{
volatile TaskParam *pxTaskParam; 
  pxTaskParam = (TaskParam *) pvParameters;
  //	xSemaphoreTake(wifi_mutex, portMAX_DELAY);
    client.publish(pxTaskParam->topic, pxTaskParam->msg);
  //  xSemaphoreGive(wifi_mutex);
  vTaskDelete(NULL);
}


bool hasmsg = false;  
String _response    = "";   


String waitResponse() {                                           // Функция ожидания ответа и возврата полученного результата
  String _resp = "";                                              // Переменная для хранения результата
  long _timeout = millis() + 10000;    
  	xSemaphoreTake(sim800_mutex, portMAX_DELAY);
  while (!SIM800.available() && millis() < _timeout)  {};         // Ждем ответа 10 секунд, если пришел ответ или наступил таймаут, то...
  if (SIM800.available()) {                                       // Если есть, что считывать...
    _resp = SIM800.readString();                                  // ... считываем и запоминаем
  }
  else {                                                          // Если пришел таймаут, то...
    Serial.println("Timeout...");                                 // ... оповещаем об этом и...
  }
   xSemaphoreGive(sim800_mutex); 
  return _resp;                                                   // ... возвращаем результат. Пусто, если проблема
}

String sendATCommand(String cmd, bool waiting) {
  String _resp = "";                                              // Переменная для хранения результата
  Serial.println(cmd); 
  xSemaphoreTake(sim800_mutex, portMAX_DELAY);
  SIM800.println(cmd); 
  xSemaphoreGive(sim800_mutex);                                                // Дублируем команду в монитор порта
                                            // Отправляем команду модулю
  if (waiting) {                                                  // Если необходимо дождаться ответа...
    _resp = waitResponse();                                       // ... ждем, когда будет передан ответ
    // Если Echo Mode выключен (ATE0), то эти 3 строки можно закомментировать
    if (_resp.startsWith(cmd)) {                                  // Убираем из ответа дублирующуюся команду
      _resp = _resp.substring(_resp.indexOf("\r", cmd.length()) + 2);
    }

    Serial.println(_resp); 
                                        // Дублируем ответ в монитор порта
  }
  return _resp;                                                   // Возвращаем результат. Пусто, если проблема
}
void sendSMS(String phone, String message)
{ cl_phone=phone;
  randNumber=message;
 if((sendATCommand("AT+CMGS=\"+7" + phone + "\"", true)).indexOf("ERROR")<0){            // Переходим в режим ввода текстового сообщения
  sendATCommand(message + "\r\n" + (String)((char)26), true);}
  else
  { 
      #if ARDUINOJSON_VERSION_MAJOR==6
       String str2;
              DynamicJsonDocument jsonBuffer(2024);
              JsonObject msg = jsonBuffer.to<JsonObject>();
              #else
              DynamicJsonBuffer jsonBuffer;
              JsonObject& msg = jsonBuffer.createObject();
              #endif
              msg["topic"] = local_topic_res;
              msg["smsST"] ="SIM8_ERR";
              if(Send_RES=="true"){  
              msg["code"]=String(0);
              }
              else
              {
                msg["code"]=String(1);
              }
              
              msg["terminalId"]=terminalId;
              #if ARDUINOJSON_VERSION_MAJOR==6
              serializeJson(msg, str2);
              #endif
              xTP1.topic="mcount1";
              char  buf2[415]="";
              String(str2).toCharArray(buf2,String(str2).length()+1);
              xTP1.msg=buf2;
             client.publish("mcount1", buf2);

              
           
    next_step=true;

   
  }
  
  // После текста отправляем перенос строки и Ctrl+Z
}

void parseSMS(String msg_r) {                                   // Парсим SMS
  String msgheader  = "";
  String msgbody    = "";
  String msgphone   = "";

  msg_r = msg_r.substring(msg_r.indexOf("+CMGR: "));
  msgheader = msg_r.substring(0, msg_r.indexOf("\r"));            // Выдергиваем телефон

  msgbody = msg_r.substring(msgheader.length() + 2);
  msgbody = msgbody.substring(0, msgbody.lastIndexOf("OK"));  // Выдергиваем текст SMS
  msgbody.trim();

  int firstIndex = msgheader.indexOf("\",\"") + 3;
  int secondIndex = msgheader.indexOf("\",\"", firstIndex);
  msgphone = msgheader.substring(firstIndex, secondIndex);
              char  bufph[115]="";
              String(msgphone).toCharArray(bufph,String(msgphone).length()+1);
              char  bufmsg[415]="";
              String(msgbody).toCharArray(bufmsg,String(msgbody).length()+1);
              Serial.println("Phone: " + msgphone);                       // Выводим номер телефона
              Serial.println("Message: " + msgbody);                      // Выводим текст SMS
   String str7;
              #if ARDUINOJSON_VERSION_MAJOR==6
              DynamicJsonDocument jsonBuffer(2024);
              JsonObject msg = jsonBuffer.to<JsonObject>();
              #else
              DynamicJsonBuffer jsonBuffer;
              JsonObject& msg = jsonBuffer.createObject();
              #endif
              msg["topic"] = local_clientID;
           //   msg["LOCK"] =local_number;
              msg["phone"]=bufph;
              msg["msg"]=bufmsg;
              #if ARDUINOJSON_VERSION_MAJOR==6
              serializeJson(msg, str7);
              #endif
              xTP1.topic="mcount1";
              char  buf7[415]="";
              String(str7).toCharArray(buf7,String(str7).length()+1);
              xTP1.msg=buf7;
              client.publish("mcount1", buf7);
  
}


void HOLD_TASK(void* pvParameters )
{long lastUpdate = millis();                                   // Время последнего обновления
long updatePeriod   = 60000; 

         
for(;;)
{



 if (SIM800.available())   {                   // Если модем, что-то отправил...
    _response = waitResponse();                 // Получаем ответ от модема для анализа
    _response.trim();              // Если нужно выводим в монитор порта
    //....
    if (_response.indexOf("+CMGS")>-1) {       // Пришло сообщение об отправке SMS

      int index = _response.lastIndexOf("\r\n");// Находим последний перенос строки, перед статусом

      String result =_response;

      result.trim();  
                             // Убираем пробельные символы в начале/конце
 Serial.print(result);

      if (result.indexOf("OK")>-1) { 
        if(Send_RES=="true"){   
          randNumber=xMessageEmail.msg;
                        // Если результат ОК - все нормально
       #if ARDUINOJSON_VERSION_MAJOR==6
       String str1;
              DynamicJsonDocument jsonBuffer(2024);
              JsonObject msg = jsonBuffer.to<JsonObject>();
              #else
              DynamicJsonBuffer jsonBuffer;
              JsonObject& msg = jsonBuffer.createObject();
              #endif
              msg["topic"] = local_topic_res;
              msg["smsST"] ="SEND_OK";
              msg["code"]=String(randNumber);
                 msg["terminalId"]=terminalId;
              #if ARDUINOJSON_VERSION_MAJOR==6
              serializeJson(msg, str1);
              #endif
              xTP1.topic="mcount1";
              char  buf1[415]="";
              String(str1).toCharArray(buf1,String(str1).length()+1);
              xTP1.msg=buf1;
              xTaskCreate( Smsg,"Smsg",4128,(void *) &xTP1,1, NULL );
         // client.publish("mcount1", buf1);
          next_step=true;
        }
        else
        {
          Send_RES="true";
          next_step=true;
        }
        _response="";
        Serial.println ("Message was sent. OK");

      }

      if (result.indexOf("ERROR")>-1) {   

          if(Send_RES=="true"){ 
                                  // Если нет, нужно повторить отправку
       #if ARDUINOJSON_VERSION_MAJOR==6
       String str2;
              DynamicJsonDocument jsonBuffer(2024);
              JsonObject msg = jsonBuffer.to<JsonObject>();
              #else
              DynamicJsonBuffer jsonBuffer;
              JsonObject& msg = jsonBuffer.createObject();
              #endif
              msg["topic"] = local_topic_res;
              msg["smsST"] ="SEND_ERR";
              msg["code"]=String(0);
              msg["terminalId"]=terminalId;
              #if ARDUINOJSON_VERSION_MAJOR==6
              serializeJson(msg, str2);
              #endif
              xTP1.topic="mcount1";
              char  buf2[415]="";
              String(str2).toCharArray(buf2,String(str2).length()+1);
              xTP1.msg=buf2;
             client.publish("mcount1", buf2);

              
           String str3;
              msg["topic"] = "sys_data_base";
              msg["smsST"] ="SEND_ERR";
              msg["code"]=cl_phone;
              msg["terminalId"]=terminalId;
              #if ARDUINOJSON_VERSION_MAJOR==6
              serializeJson(msg, str3);
              #endif
              xTP1.topic="mcount1";
              char  buf3[415]="";
              String(str3).toCharArray(buf3,String(str3).length()+1);
              xTP1.msg=buf3;
             client.publish("mcount1", buf3);


             
              next_step=true;
              }
        else
        {
          Send_RES="true";
           next_step=true;
        }
        Serial.println ("Message was not sent. Error");
_response="";
      }

    
    }

  }

 vTaskDelay(10); 
}

  vTaskDelete(NULL);
}






void sGetMonetCount(void* pvParameters )
{
Bill_validaror=true;

volatile TaskParam *pxTaskParam; 
  pxTaskParam = (TaskParam *) pvParameters;
pinMode(moneyPin, INPUT_PULLUP); 
digitalWrite(InhiBit, HIGH);

int valuePulse = 10;					// Стоимость одного импульса
int minWidthPulse = 40;				// Минимальная ширина одного импульса
int maxWidthPulse = 80;				// Максимальная ширина одного импульса
int debounce = 4;							// Защита от помех
int pulseCount = 1;						// Сколько импульсов получено
int receivedRUB = 0;					// Сумма
unsigned long pulseDuration;	// Как давно был последний импульс
unsigned long pulseBegin = 0; // Начало импульса
unsigned long pulseEnd = 0;		// Конец импульса
unsigned long curtime;				// Время
int postPulsePause = 350;			// Время ожидания, для завершения подсчета импульсов
int pulseState;								// Состояние входа "0" или "1"
int lastState = 1;
int reqvalue = 0;
unsigned long timer=millis()+220000 ;

reqvalue=pxTaskParam->regv_amount; 
if(pref.getInt("amount")>0)
{
  receivedRUB =pref.getInt("amount")+receivedRUB;
}
while( reqvalue > 1)
	{	pulseState = digitalRead(moneyPin);
if(timer<millis()){
Bill_validaror=false;

digitalWrite(InhiBit,LOW);
 String str2;
              #if ARDUINOJSON_VERSION_MAJOR==6
              DynamicJsonDocument jsonBuffer(4024);
              JsonObject msg = jsonBuffer.to<JsonObject>();
              #else
              DynamicJsonBuffer jsonBuffer;
              JsonObject& msg = jsonBuffer.createObject();
              #endif
              msg["topic"] = local_topic_res;
           
              msg["avalibale"]="LIMIT_TIMER";
              msg["amount"]=String(0);
              msg["end"]=String(0);
              msg["terminalId"]=terminalId;
              #if ARDUINOJSON_VERSION_MAJOR==6
              serializeJson(msg, str2);
              #endif
              xTP1.topic="mcount1";
                char  buf2[615]="";
              String(str2).toCharArray(buf2,String(str2).length()+1);
              xTP1.msg=buf2;
             client.publish("mcount1", buf2);
}
if(!Bill_validaror)
{
 reqvalue = 0; 
}
		// Считываем значение с входа moneyPin
		curtime = millis(); // Записываем значение миллисекунд с момента начала выполнения программы

		if ((pulseState == 1) && (lastState == 1)) // Ждем начало импульса, логический "0"
		{
			pulseBegin = curtime; // Записываем значение милисикунд
			lastState = 0;				// Записываем значение "0" в переменную lastState
		}
		else if ((pulseState == 0) && (lastState == 0)) // Ждем окончания импульса, логический "1"
		{
			pulseDuration = curtime - pulseBegin; // Расчет длительности импульса в миллисекунд
			if (pulseDuration > debounce)					// Защита от помех, если импульс был небольшой
			{
				lastState = 1; // Записываем значение "1" в переменную lastState
			}
			if ((pulseDuration > minWidthPulse) && (pulseDuration < maxWidthPulse)) // Проверяем ширину импульса
			{
				pulseEnd = curtime; // Сохранить значение милисикунд, окончания импульса
				pulseCount++;				// Инкремент счетчика импульсов

			}
		}
		if ((pulseEnd > 0) && (curtime - pulseEnd > postPulsePause)) // Проверяем, поступают ли еще импульсы
		{
			receivedRUB += pulseCount * valuePulse; // Расчет суммы
     	xSemaphoreTake(serial_mutex, portMAX_DELAY);
			Serial.print("Credit: ");	
			Serial.print(receivedRUB); 
			Serial.println(" RUB");		 
      xSemaphoreGive(serial_mutex);   
     	pulseEnd = 0;
			pulseCount = 1;
      			if (receivedRUB >= reqvalue)
			{	digitalWrite(InhiBit, LOW);	 	
			 
              String str;
              #if ARDUINOJSON_VERSION_MAJOR==6
              DynamicJsonDocument jsonBuffer(2024);
              JsonObject msg = jsonBuffer.to<JsonObject>();
              #else
              DynamicJsonBuffer jsonBuffer;
              JsonObject& msg = jsonBuffer.createObject();
              #endif
              msg["topic"] =  local_topic_res;
             
              msg["avalibale"]="AVAL";
              msg["amount"]=String(receivedRUB);
              msg["end"]=String(1);
              msg["terminalId"]=terminalId;
              #if ARDUINOJSON_VERSION_MAJOR==6
              serializeJson(msg, str);
              #endif
              xTP1.topic="mcount1";
              char  buf[615]="";
              String(str).toCharArray(buf,String(str).length()+1);
              xTP1.msg=buf;
           client.publish("mcount1", buf);
          pref.putInt("amount",0); 
			    reqvalue = 0;
			    receivedRUB = 0;            
    
			}
      else{
        // save to memory 
              pref.putInt("amount",receivedRUB);
              String str1;
              #if ARDUINOJSON_VERSION_MAJOR==6
              DynamicJsonDocument jsonBuffer(2024);
              JsonObject msg = jsonBuffer.to<JsonObject>();
              #else
              DynamicJsonBuffer jsonBuffer;
              JsonObject& msg = jsonBuffer.createObject();
              #endif
              msg["topic"] =  local_topic_res;
             
              msg["avalibale"]="AVAL";
              msg["amount"]=String(receivedRUB);
              msg["end"]=String(0);
              msg["terminalId"]=terminalId;
              #if ARDUINOJSON_VERSION_MAJOR==6
              serializeJson(msg, str1);
              #endif
              xTP1.topic="mcount1";
              char  buf1[615]="";
              String(str1).toCharArray(buf1,String(str1).length()+1);
              xTP1.msg=buf1;
     client.publish("mcount1", buf1);
      }
		}
   vTaskDelay(10);
	}
Bill_validaror=false;
  vTaskDelete(NULL);
}


void SGetVal(void* pvParameters )
{
volatile TaskParam *pxTaskParam; 
  pxTaskParam = (TaskParam *) pvParameters;

  
if(String(pxTaskParam->incomtopicX).indexOf("mcount1")>-1)
{
  	xSemaphoreTake(serial_mutex, portMAX_DELAY);
       // Serial.println(pxTaskParam->incomtopicX);
      xSemaphoreGive(serial_mutex);
 

    xSemaphoreTake(wifi_mutex, portMAX_DELAY);
    client.publish("mcount1", "lera");
    xSemaphoreGive(wifi_mutex);
}

  vTaskDelete(NULL);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



  void messageReceived(String &topic, String &payload) {
if(topic.indexOf('mcount1')>-1 && payload!=""){
    	xSemaphoreTake(serial_mutex, portMAX_DELAY);
      Serial.println("incoming: " + topic + " - " + payload); 
     
      xSemaphoreGive(serial_mutex);

#if ARDUINOJSON_VERSION_MAJOR==6
  DynamicJsonDocument jsonBuffer(2024 + payload.length());
  DeserializationError error = deserializeJson(jsonBuffer, payload);
  if (error) {
    Serial.printf("DeserializationError\n");
    return;
  }
  JsonObject root = jsonBuffer.as<JsonObject>();

#else

  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(payload);

#endif


  if (root.containsKey("topic")) {
if (String("system_msg").equals(root["topic"].as<String>()) &&  terminalId==root["device"].as<String>())
{  
if( String("disconnected").equals(root["msg"].as<String>()))
{
              Bill_validaror=false;
              digitalWrite(InhiBit, LOW);  
}
}



      if (String(local_clientID).equals(root["topic"].as<String>())) {
if(root.containsKey("new_client"))
{		  if( xQueue1 != 0 )
    { 
Send_RES=root["new_client"].as<String>(); 
terminalId=root["terminalId"].as<String>();
 
//randNumber = root["code"].as<String>();
//cl_phone=root["phone"].as<String>();
	 
         xMessageEmail.phone=root["phone"].as<String>();
         xMessageEmail.msg=root["code"].as<String>();  
          if(xMessageEmail.msg!="washer close") 
          {
        struct AMessage *pxMessage;
        pxMessage = & xMessageEmail;
        vTaskDelay(30);
       
        xQueueSendToFront( xQueue1, ( void * ) &pxMessage, ( TickType_t ) 0 );}
        else{
        
        struct AMessage *pxMessage;
        pxMessage = & xMessageEmail;
        vTaskDelay(30);
       
        xQueueSend( xQueue1, ( void * ) &pxMessage, ( TickType_t ) 0 );
    }
    }
    else
    {
      Serial.println("Q_died");
    }
    
//sendSMS(cl_phone,randNumber);
}
else
{





             if(Bill_validaror==false && String("cansel").indexOf((root["get_amount"].as<String>()))==-1)
            {    terminalId=root["terminalId"].as<String>();
              if(pref.getInt("amount")>0)

{
    String str4;
              #if ARDUINOJSON_VERSION_MAJOR==6
              DynamicJsonDocument jsonBuffer(2024);
              JsonObject msg = jsonBuffer.to<JsonObject>();
              #else
              DynamicJsonBuffer jsonBuffer;
              JsonObject& msg = jsonBuffer.createObject();
              #endif
              msg["topic"] = local_topic_res;
            
              msg["avalibale"]="POST_AVAL";
              msg["amount"]=String(pref.getInt("amount"));
              msg["end"]=String(0);
              msg["terminalId"]=root["terminalId"].as<String>();
              #if ARDUINOJSON_VERSION_MAJOR==6
              serializeJson(msg, str4);
              #endif
              xTP1.topic="mcount1";
                char  buf4[315]="";
              String(str4).toCharArray(buf4,String(str4).length()+1);
              xTP1.msg=buf4;
              client.publish("mcount1", buf4);
              xTP1.regv_amount= (root["get_amount"].as<String>()).toInt()-pref.getInt("amount");
              xTaskCreate( sGetMonetCount,"sGetMonetCount",12128,(void *) &xTP1, 1, NULL ); 
}

else
{
            xTP1.regv_amount= (root["get_amount"].as<String>()).toInt();
             terminalId=root["terminalId"].as<String>();
             Serial.printf("logServer detected!!!\n");
             String str4;
             #if ARDUINOJSON_VERSION_MAJOR==6
              DynamicJsonDocument jsonBuffer(2024);
              JsonObject msg = jsonBuffer.to<JsonObject>();
              #else
              DynamicJsonBuffer jsonBuffer;
              JsonObject& msg = jsonBuffer.createObject();
              #endif
              msg["topic"] = local_topic_res;

              msg["avalibale"]="GET_AVAL";
              msg["amount"]=String(pref.getInt("amount"));
              msg["end"]=String(0);
              msg["terminalId"]=root["terminalId"].as<String>();
              #if ARDUINOJSON_VERSION_MAJOR==6
              serializeJson(msg, str4);
              #endif
              xTP1.topic="mcount1";
              char  buf4[615]="";
              String(str4).toCharArray(buf4,String(str4).length()+1);
              xTP1.msg=buf4; 
              client.publish("mcount1", buf4);
              xTP1.regv_amount= (root["get_amount"].as<String>()).toInt();
              xTaskCreate( sGetMonetCount,"sGetMonetCount",12128,(void *) &xTP1, 1, NULL );
       
}
            }
            else if(Bill_validaror==true && terminalId==root["terminalId"].as<String>())
        { Serial.println(root["get_amount"].as<String>());
              if(String("cansel").equals(root["get_amount"].as<String>()))
              {
              Bill_validaror=false;
              digitalWrite(InhiBit, LOW);

              }
              else
              {
                String str3;
              #if ARDUINOJSON_VERSION_MAJOR==6
              DynamicJsonDocument jsonBuffer(2024);
              JsonObject msg = jsonBuffer.to<JsonObject>();
              #else
              DynamicJsonBuffer jsonBuffer;
              JsonObject& msg = jsonBuffer.createObject();
              #endif
              msg["topic"] = local_topic_res;
             
              msg["avalibale"]="SET_AVAL";
              msg["amount"]=String(0);
              msg["end"]=String(0);
              msg["terminalId"]=root["terminalId"].as<String>();
              #if ARDUINOJSON_VERSION_MAJOR==6
              serializeJson(msg, str3);
              #endif
              xTP1.topic="mcount1";
                char  buf3[615]="";
              String(str3).toCharArray(buf3,String(str3).length()+1);
              xTP1.msg=buf3;
              client.publish("mcount1", buf3);
              }  
        }
           else
            {
              String str;
              #if ARDUINOJSON_VERSION_MAJOR==6
              DynamicJsonDocument jsonBuffer(2024);
              JsonObject msg = jsonBuffer.to<JsonObject>();
              #else
              DynamicJsonBuffer jsonBuffer;
              JsonObject& msg = jsonBuffer.createObject();
              #endif
              msg["topic"] = local_topic_res;
            
              msg["avalibale"]="NOT_AVAL";
              msg["amount"]=String(0);
              msg["end"]=String(0);
              msg["terminalId"]=root["terminalId"].as<String>();
              #if ARDUINOJSON_VERSION_MAJOR==6
              serializeJson(msg, str);
              #endif
              xTP1.topic="mcount1";
                char  buf[615]="";
              String(str).toCharArray(buf,String(str).length()+1);
              xTP1.msg=buf;
             client.publish("mcount1", buf);
            }
            
}}
  }
topic="";
payload="";
  }

}
 void vADifferentTask( void *pvParameters )
 {
 struct AMessage *pxRxedMessage;


  for( ;; )
  {
    if( xQueue1 != 0 )
    {
        // Receive a message on the created queue.  Block for 10 ticks if a
        // message is not immediately available.
        if( xQueueReceive( xQueue1, &( pxRxedMessage ), ( TickType_t ) 10 ) )
        { 
	xSemaphoreTake(serial_mutex, portMAX_DELAY);
 
      Serial.println("queue");
      xSemaphoreGive(serial_mutex);
                next_step=false;
                sendSMS(pxRxedMessage->phone,pxRxedMessage->msg);
                while(next_step!=true){
                vTaskDelay(50);
                }
    Serial.println("queue_end");
           
        }
 vTaskDelay(50);  
    }
  
    

    vTaskDelay(20);   
  }
  vTaskDelete(NULL);
 }
void STopic(void* pvParameters )
{
unsigned long lastReconnectAttempt=millis();
  for(;;){
  
 if (!client.connected()) {
      connect();
  } 
   xSemaphoreTake(wifi_mutex, portMAX_DELAY);
    client.loop();
   xSemaphoreGive(wifi_mutex);
    vTaskDelay(10);
  }
  vTaskDelete(NULL);
}


void setup() {
	pinMode(InhiBit, OUTPUT);
  Serial.begin(9600);
    SIM800.begin(9600);   
  pref.begin("mcount", false);
  // xQueue1 =xQueueCreate( 30,sizeof( struct AMessage * ));

 
          	wifi_mutex = xSemaphoreCreateMutex();
          	pref_mutex = xSemaphoreCreateMutex();
            serial_mutex = xSemaphoreCreateMutex();
            sim800_mutex= xSemaphoreCreateMutex();
            WiFi.begin(ssid, pass);
            client.begin(broker,1883, net);
            client.onMessage(messageReceived);
            connect();
            xTaskCreate( STopic,"STopic",8128,NULL, 1, NULL );
            xTaskCreate( HOLD_TASK,"HOLD_TASK",8128,NULL, 1, NULL ); 
            xTaskCreate( vTask_Queue1,"vTask_Queue1",8128,NULL, 1, NULL );
            xTaskCreate( vADifferentTask,"vADifferentTask",8128,NULL, 1, NULL ); 
           
            sendATCommand("AT",true);    

}

void loop() {	

}
