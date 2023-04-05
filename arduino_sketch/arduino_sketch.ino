#include <string.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Fonts/FreeMono9pt7b.h>
#include <ArduinoJson.h>

//Pinouts
#define TFT_CS 10
#define TFT_DC 8
#define TFT_RST 9
#define cpuledok 2
#define cpuledhigh 3
#define ramledok 4
#define ramledhigh 7
#define cpuvum 5
#define ramvum 6

Adafruit_ST7735 tft=Adafruit_ST7735(TFT_CS,TFT_DC,TFT_RST);

//Settings
#define redthreshold 90
#define timeWithoutDataToStandby 3000

//My screen is buggy...
#define BUGGY_WHITE 0xffff
#define BUGGY_BLUE 0xf800
#define BUGGY_GREEN 0x07e0
#define BUGGY_RED 0x001f
#define BUGGY_AMBER 0x05df

//Global variables
bool standingBy=false;
int now=0;
int lastSeenData=0;

//Functions
void controlVUassembly(unsigned short int vuID,unsigned short int value)
{
  float valueF=value*1.9;

  //0 für CPU, 1 für RAM
  switch (vuID)
  {
  case 0:
    analogWrite(cpuvum,valueF);
    if (value<redthreshold)
    {
      digitalWrite(cpuledok,HIGH);
      digitalWrite(cpuledhigh,LOW);
    }
    else
    {
      digitalWrite(cpuledok,LOW);
      digitalWrite(cpuledhigh,HIGH);
    }

    break;
  
  case 1:
    analogWrite(ramvum,valueF);
    if (value<redthreshold)
    {
      digitalWrite(ramledok,HIGH);
      digitalWrite(ramledhigh,LOW);
    }
    else
    {
      digitalWrite(ramledok,LOW);
      digitalWrite(ramledhigh,HIGH);
    }
    break;

  default:
    //standBy();
    break;
  }
}

void switchLEDsOff()
{
  digitalWrite(LED_BUILTIN,LOW);
  digitalWrite(cpuledok,LOW);
  digitalWrite(cpuledhigh,LOW);
  digitalWrite(ramledok,LOW);
  digitalWrite(ramledhigh,LOW);
}

void drawText(int cursorX,int cursorY,int textSize,int color,const String text)
{
  tft.setCursor(cursorX,cursorY);
  tft.setTextColor(color,ST7735_BLACK);
  tft.setTextSize(textSize);
  tft.println(text);
}

void standBy() {
  standingBy=true;

  tft.fillScreen(ST7735_BLACK);
  tft.drawRoundRect(7,7,115,145,4,BUGGY_AMBER);
  drawText(36,60,1,BUGGY_AMBER,"Warte");
  drawText(46,80,1,BUGGY_AMBER,"auf");
  drawText(35,100,1,BUGGY_AMBER,"Daten.");

  controlVUassembly(0,0);
  controlVUassembly(1,0);
  switchLEDsOff();
}

void setup() {
  pinMode(LED_BUILTIN,OUTPUT);
  pinMode(cpuledok,OUTPUT);
  pinMode(cpuledhigh,OUTPUT);
  pinMode(ramledok,OUTPUT);
  pinMode(ramledhigh,OUTPUT);
  pinMode(cpuvum,OUTPUT);
  pinMode(ramvum,OUTPUT);

  tft.initR(INITR_BLACKTAB);
  tft.setFont(&FreeMono9pt7b);

  Serial.begin(9600);

  standBy();
}

void loop() {
  // https://arduinojson.org/v6/how-to/do-serial-communication-between-two-boards/
  if(Serial.available()) {
    StaticJsonDocument<300> doc;
    DeserializationError err = deserializeJson(doc, Serial);

    if(err==DeserializationError::Ok) {
      controlVUassembly(0,doc["c"].as<int>());
      controlVUassembly(1,doc["r"].as<int>());

      //standingBy=false;
      lastSeenData=millis();
    }
    else if (!standingBy) {
      standBy();
    }
  }
  else if (!standingBy) {
    standBy();
  }

  now=millis();
  //Serial.println(String(now));
  /*if((now-lastSeenData)>=timeWithoutDataToStandby && !standingBy) {
    standBy();
  }*/
}