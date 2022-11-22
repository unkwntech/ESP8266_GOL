#include <Arduino.h>
#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <algorithm>
#include <iterator>

//Options
#define ROWS 16
#define COLUMNS 16
#define PIXELCOUNT COLUMNS*ROWS
#define OUPUTPIN 2
#define WIFIWAITCOUNT 25
#define BRIGHTNESS 1


//Include NPT Client to get current time after startup.
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

//Setup LED "strip"
NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> strip(PIXELCOUNT, OUPUTPIN);
//Create colour shorthands for later use
RgbColor red(BRIGHTNESS, 0, 0);
RgbColor green(0, BRIGHTNESS, 0);
RgbColor blue(0, 0, BRIGHTNESS);
RgbColor white(BRIGHTNESS);
RgbColor black(0);

//Fields for GOL
//Each row is an int with each bit representing a single cell
int field[ROWS] = {};
//Copy for calculations
int fieldCopy[ROWS] = {};


//function stubs
unsigned long getTime();
void writeField();
void evolve();
void printField(int f[]);

void setup() {

    //setup serial connection
    Serial.begin(115200);

    //Setup and connect to wifi
    WiFi.mode(WIFI_STA);
    WiFi.begin("Muninn", "8586197600");

    //Wait until wifi is connected or wait count is reached
    for(int i = 0; i < WIFIWAITCOUNT; i++) {
        if(WiFi.status() != WL_CONNECTED)
            delay(100);
        else
            break;
    }

    //if wifi is connected, setup NTP client and PRNG
    if(WiFi.status() == WL_CONNECTED) {
        Serial.println("Wifi Connected");

        //start NTP client
        timeClient.begin();

        //Use current time to seed PRNG
        randomSeed(getTime());
    }
    
    //Setup and blank the LEDS
    strip.Begin();

    //init field
    for(int i = 0; i < ROWS; i++)
        field[i] = 0;
    
    writeField();
    
}

void loop() {
    //only run loop once / second(ish)
    delay(10000);
    evolve();
    writeField();
}

void evolve() {
    /*
        Any live cell with two or three live neighbours survives.
        Any dead cell with three live neighbours becomes a live cell.
        All other live cells die in the next generation. Similarly, all other dead cells stay dead.
    */

   int neighbors = 0;
   //Copy field to fieldCopy
   std::copy(std::begin(field), std::end(field), std::begin(fieldCopy));

   for(int row = 0; row < ROWS; row++) {
        for(int col = 0; col < COLUMNS; col++) {
            neighbors = 0;
            //row - 1, col
            if(row != 0 && (field[row - 1] & (1 << (col))) > 0)
                    neighbors++;
            //row + 1, col
            if(row +1 != ROWS && (field[row + 1] & (1 << col)) > 0)
                neighbors++;
            //row, col - 1
            if(col != 0 && (field[row] & (1 << ((col) - 1))) > 0)
                neighbors++;
            //row, col + 1
            if(col + 1 != COLUMNS && (field[row] & (1 << ((col) + 1))) > 0)
                neighbors++;
            //row -1, col -1
            if(row != 0 && col != 0 && (field[row - 1] & (1 << (col - 1))) > 0)
              neighbors++;
            //row - 1, col +1
            if(row != 0 && col != COLUMNS && (field[row - 1] & (1 << (col + 1))) > 0)
              neighbors++;
            //row + 1, col -1
            if(row != ROWS && col != 0 && (field[row + 1] & (1 << (col - 1))) > 0)
              neighbors++;
            //row + 1, col + 1
            if(row != ROWS && col != COLUMNS && (field[row + 1] & (1 << (col + 1))) > 0)
              neighbors++;

            //overcrowded or solitary
            if(neighbors > 3 || neighbors < 2){
                fieldCopy[row] = fieldCopy[row] & ~(1 << col);
            }
            //unerpopulated
            else if (neighbors == 3){
                fieldCopy[row] = fieldCopy[row] | (1 << col);
            }
        }
    }

    //Write the working field back to the main field
    std::copy(std::begin(fieldCopy), std::end(fieldCopy), std::begin(field));
}

/*
    Print field to serial
*/
void printField(int f[]) {
    for(int row = 0; row < ROWS; row++){
        for(int col = 0; col < COLUMNS; col++) {
            if((f[row] & (1 << col)) > 0) {
                Serial.print("X");
            } else {
                Serial.print("-");
            }
        }
        Serial.println();
    }
}

/*
    Write field to LED Matrix
*/
void writeField() {
    /*
        The LED Matrix is just a strip of LEDs
    */
    int pixelCount = 0;
    for(int row = 0; row < ROWS; row++) {
        for(int col = 0; col < COLUMNS; col++) {
            if(field[row] & 1 << col)
                strip.SetPixelColor(pixelCount, green);
            else
                strip.SetPixelColor(pixelCount, black);
            pixelCount++;
        }
    }
    strip.Show();
}

/*
    Get current time from NTP server
*/
unsigned long getTime() {
  timeClient.update();
  return timeClient.getEpochTime();
}
