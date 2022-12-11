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
#define WIFIWAITCOUNT 100
#define BRIGHTNESS 1

#define RED_OFFSET 1
#define GREEN_OFFSET 1
#define BLUE_OFFSET 2

//Include NPT Client to get current time after startup.
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

//Setup LED "strip"
NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> strip(PIXELCOUNT, OUPUTPIN);
//Create colour shorthands for later use
RgbColor red(BRIGHTNESS * RED_OFFSET, 0, 0);
RgbColor green(0, BRIGHTNESS * GREEN_OFFSET, 0);
RgbColor blue(0, 0, BRIGHTNESS * BLUE_OFFSET);
RgbColor white(BRIGHTNESS);
RgbColor purple(BRIGHTNESS * RED_OFFSET, 0, BRIGHTNESS * BLUE_OFFSET * 2);
RgbColor black(0);

//Fields for GOL
/*
    BIT     USE
    8 (MSB) Alive/Dead
    7       New Cell
    6       Newly Dead Cell
*/
byte field[ROWS][COLUMNS];
//Copy for calculations
byte fieldCopy[ROWS][COLUMNS];

byte priorFields[2][ROWS][COLUMNS];

#define ALIVE_CELL 1 << 7          //1000 0000
#define NEWLY_BORN 1 << 6          //0100 0000
#define NEWLY_DIED 1 << 5          //0010 0000
#define CHANGED 1 << 4             //0001 0000
#define DEAD_CELL ~ALIVE_CELL      //0111 1111

//function stubs
unsigned long getTime();
void writeField();
void evolve();
void printField();
void generateRandomField();

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
    
    strip.Begin();
    generateRandomField();
    writeField();
}

void loop() {
    //only run loop once / second(ish)
    delay(1000);
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
   
   if(!memcmp(priorFields[0], field, sizeof(byte) * ROWS * COLUMNS)) {
        generateRandomField();
        return;
   }
   //Copy field to fieldCopy and prior fields
   for(int row = 0; row < ROWS; row++){
    for(int col = 0; col < COLUMNS; col++) {
        priorFields[0][row][col] = priorFields[1][row][col];
        priorFields[1][row][col] = field[row][col];
        fieldCopy[row][col] = field[row][col];
    }
   }
   

   for(int row = 0; row < ROWS; row++) {
        for(int col = 0; col < COLUMNS; col++) {
            neighbors = 0;
            //row - 1, col
            if(row != 0 && field[row - 1][col] >= ALIVE_CELL)
                    neighbors++;
            //row + 1, col
            if(row +1 != ROWS && field[row + 1][col] >= ALIVE_CELL)
                neighbors++;
            //row, col - 1
            if(col != 0 && field[row][col - 1] >= ALIVE_CELL)
                neighbors++;
            //row, col + 1
            if(col + 1 != COLUMNS && field[row][col + 1] >= ALIVE_CELL)
                neighbors++;
            //row -1, col -1
            if(row != 0 && col != 0 && field[row - 1][col - 1] >= ALIVE_CELL)
              neighbors++;
            //row - 1, col +1
            if(row != 0 && col + 1 != COLUMNS && field[row - 1][col + 1] >= ALIVE_CELL)
              neighbors++;
            //row + 1, col -1
            if(row + 1 != ROWS && col != 0 && field[row + 1][col - 1] >= ALIVE_CELL)
              neighbors++;
            //row + 1, col + 1
            if(row + 1 != ROWS && col + 1 != COLUMNS && field[row + 1][col + 1] >= ALIVE_CELL)
              neighbors++;

            //overcrowded or solitary
            if(field[row][col] >= ALIVE_CELL && (neighbors > 3 || neighbors < 2)){
                fieldCopy[row][col] &= DEAD_CELL;
                fieldCopy[row][col] |= NEWLY_DIED | CHANGED;
            }
            //unerpopulated
            else if (field[row][col] < ALIVE_CELL && neighbors == 3){
                fieldCopy[row][col] |= ALIVE_CELL | NEWLY_BORN | CHANGED;
            }
        }
    }

    //Write the working field back to the main field
    //memcpy(field, fieldCopy, ROWS * COLUMNS);
    for(int row = 0; row < ROWS; row++){
        for(int col = 0; col < COLUMNS; col++) {
            field[row][col] = fieldCopy[row][col];
        }
    }
    //printField();
}

/*
    Print field to serial
*/
void printField() {
    Serial.println();
    for(int row = 0; row < ROWS; row++){
        for(int col = 0; col < COLUMNS; col++) {
            if((field[row][col] & ALIVE_CELL) > 0) {
                if((field[row][col] & NEWLY_BORN) > 0)
                    Serial.print("B");
                else
                    Serial.print("X");
            } else {
                if((field[row][col] & NEWLY_DIED) > 0)
                    Serial.print("D");
                else
                    Serial.print("-");
            }
        }
        Serial.println();
    }
        Serial.println();
        Serial.println();
}

/*
    Generate a random field.
*/
void generateRandomField() {
    for(int row = 0; row < ROWS; row++) {
        for(int col = 0; col < COLUMNS; col++){
            field[row][col] = (random(255) & ALIVE_CELL) | CHANGED;
        }
    }
}

/*
    Write field to LED Matrix
*/
void writeField() {
    /*
        The LED Matrix is just a strip of LEDs
    */
    unsigned int pixelCount = 0;
    unsigned int address =0;
    RgbColor color = purple;
    for(int row = 0; row < ROWS; row++) {
        for(int col = 0; col < COLUMNS; col++) {
            //If the cell has not been changed since last write skip it.
            if((field[row][col] & CHANGED) == 0) {
                pixelCount++;
                continue;
            }

            field[row][col] &= ~CHANGED;
            color = purple;
            //cell is alive
            if((field[row][col] & ALIVE_CELL) > 0) {
                if((field[row][col] & NEWLY_BORN) > 0) {
                    //cell is newly alive
                    color = green;
                    field[row][col] = (field[row][col] & ~NEWLY_BORN) | CHANGED;
                } else {
                    //cell was already alive
                    color = purple;
                }
            //cell is dead
            } else {
                if ((field[row][col] & NEWLY_DIED) > 0) {
                    //cell is newly dead
                    color = red;
                    field[row][col] = (field[row][col] & ~NEWLY_DIED) | CHANGED;
                } else {
                    //cell was already dead
                    color = black;
                }
            }

            if(row % 2 != 0) {
                address = row * COLUMNS + (COLUMNS - col) - 1;
            } else {
                address = pixelCount;
            }

            strip.SetPixelColor(address, color);

            pixelCount++;
            
            //Missing some pixels, slow down the writes a bit
            delay(5);
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
