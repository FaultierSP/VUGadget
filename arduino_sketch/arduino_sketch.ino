#include <string.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Fonts/FreeMono9pt7b.h>
#include <ArduinoJson.h>
#include <QList.h> //https://github.com/SloCompTech/QList

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
#define timeWithoutDataToStandby 3000 //in ms
#define graphResolution 10
#define graphRefreshRate 1 //times per second
#define graphMinX 14 //ABSOLUTE VALUES!
#define graphMinY 65
#define graphMaxX 115
#define graphMaxY 142
#define graphWidth 101
#define graphHeight 77

//My screen is buggy...
#define BUGGY_BLUE 0xf800
#define BUGGY_GREEN 0x07e0
#define BUGGY_RED 0x001f
#define BUGGY_AMBER 0x05df

//Global variables
bool standingBy=false;
unsigned int now=0;
unsigned int lastSeenData=0;
QList<unsigned int> networkUploadData;
QList<unsigned int> networkDownloadData;
unsigned int previousUPX=0;
unsigned int previousUPY=0;
unsigned int currentUPX=0;
unsigned int currentUPY=0;
unsigned int previousDOWNX=0;
unsigned int previousDOWNY=0;
unsigned int currentDOWNX=0;
unsigned int currentDOWNY=0;
unsigned int currentUploadRate=0;
unsigned int currentDownloadRate=0;
unsigned long summedUploadRate=0;
unsigned long summedDownloadRate=0;
unsigned int calculatedUploadRate=0;
unsigned int calculatedDownloadRate=0;
unsigned int graphLoopCounter=0;
unsigned int graphLastRender=0;
unsigned long maximalNetworkValue=0;
float columnWidth=graphWidth/(graphResolution-1);

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
  tft.setTextWrap(true);
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

int translateXToScreen(int counter) {
  int result=0;
  
  result=graphMinX+(columnWidth*counter);
  return result;
}

int translateYToScreen(int raw) {
  int result=0;
  float floatGraphHeight=graphHeight; //yeah, it's kinda crutch, but the behaviour otherwise is weird. :/
  float preResult=(floatGraphHeight/maximalNetworkValue)*raw;

  if (preResult<0) {
    result=graphMaxY;
  }
  else {
    result=graphMaxY-preResult;
  }

  return result;
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

  for(int i=0;i<graphResolution;i++) {
    networkDownloadData.push_back(0);
    networkUploadData.push_back(0);
  }

  standBy();
}

void loop() {
  // https://arduinojson.org/v6/how-to/do-serial-communication-between-two-boards/
  // https://learn.adafruit.com/adafruit-gfx-graphics-library/graphics-primitives
  now=millis();

  if(Serial.available()) {
    StaticJsonDocument<500> doc;
    DeserializationError err = deserializeJson(doc, Serial);

    if(err==DeserializationError::Ok) {
      controlVUassembly(0,doc["c"].as<int>());
      controlVUassembly(1,doc["r"].as<int>());

      if(standingBy) {
        tft.fillScreen(ST7735_BLACK);
        tft.drawRoundRect(7,7,115,145,4,ST7735_WHITE);
        tft.drawTriangle(14,30,22,17,30,30,BUGGY_RED);
        tft.drawTriangle(14,37,30,37,22,50,BUGGY_GREEN);
        tft.drawFastHLine(14,60,101,ST7735_WHITE);
      }

      if(doc.containsKey("bt")) {
        currentUploadRate=doc["bt"].as<int>();

        tft.fillRect(32,16,80,18,ST7735_BLACK);
        drawText(35,28,1,ST7735_WHITE,String(currentUploadRate));
      }
      else {
        currentUploadRate=0;
      }

      if(doc.containsKey("br")) {
        currentDownloadRate=doc["br"].as<int>();

        tft.fillRect(32,35,80,18,ST7735_BLACK);
        drawText(35,48,1,ST7735_WHITE,String(currentDownloadRate));
      }
      else {
        currentDownloadRate=0;
      }

      //Graph rendering
      if((now-graphLastRender)<=(1000/graphRefreshRate)) {
        graphLoopCounter++;
        summedUploadRate+=currentUploadRate;
        summedDownloadRate+=currentDownloadRate;
      }
      else {
        tft.fillRect(14,64,graphWidth,graphHeight+2,ST7735_BLACK);

        calculatedUploadRate=summedUploadRate/graphLoopCounter;
        calculatedDownloadRate=summedDownloadRate/graphLoopCounter;

        networkUploadData.pop_front();
        networkUploadData.push_back(calculatedUploadRate);
        networkDownloadData.pop_front();
        networkDownloadData.push_back(calculatedDownloadRate);
        
        for(int i=0;i<networkUploadData.size();i++) {
          if(networkUploadData[i]>maximalNetworkValue) {
            maximalNetworkValue=networkUploadData[i];
          }
        }

        for(int i=0;i<networkDownloadData.size();i++) {
          if(networkDownloadData[i]>maximalNetworkValue) {
            maximalNetworkValue=networkDownloadData[i];
          }
        }

        if(networkDownloadData.size()==graphResolution && networkUploadData.size()==graphResolution) {
          for(int graphCounter=0;graphCounter<graphResolution;graphCounter++) {
            if (graphCounter==0) {
              previousDOWNX=translateXToScreen(0);
              previousDOWNY=translateYToScreen(networkDownloadData[0]);

              previousUPX=translateXToScreen(0);
              previousUPY=translateYToScreen(networkUploadData[0]);
            }
            else {
              currentDOWNX=translateXToScreen(graphCounter);
              currentDOWNY=translateYToScreen(networkDownloadData[graphCounter]);

              currentUPX=translateXToScreen(graphCounter);
              currentUPY=translateYToScreen(networkUploadData[graphCounter]);

              tft.drawLine(previousUPX,previousUPY,currentUPX,currentUPY,BUGGY_RED);
              tft.drawLine(previousDOWNX,previousDOWNY,currentDOWNX,currentDOWNY,BUGGY_GREEN);

              previousDOWNX=currentDOWNX;
              previousDOWNY=currentDOWNY;
              previousUPX=currentUPX;
              previousUPY=currentUPY;
            }
          }
        }
        else {
          //Serial.println("Something nasty happened in the sizes of the graph loop. :(");
        }

        graphLastRender=millis();
        graphLoopCounter=1;
        summedUploadRate=currentUploadRate;
        summedDownloadRate=currentDownloadRate;
        maximalNetworkValue=0;
      }
      //eo graph rendering

      standingBy=false;
      lastSeenData=millis();
    }
  } //if(Serial.available())
  else if ((now-lastSeenData)>=timeWithoutDataToStandby && !standingBy) {
    standBy();
  }
}