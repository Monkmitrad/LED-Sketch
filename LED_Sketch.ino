#include <Aldi_NeoPixel.h>
#include <CircularBuffer.h>

#define PIN 8
#define LEDS 50
#define WHITE 0x646464
#define RED   0xFF0000
#define GREEN 0x00FF00
#define BLUE  0x0000FF

// Parameter 1 = number of strip in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_RGB     strip are wired for RGB bitstream
//   NEO_GRB     strip are wired for GRB bitstream
//   NEO_KHZ400  400 KHz bitstream (e.g. FLORA strip)
//   NEO_KHZ800  800 KHz bitstream (e.g. High Density LED strip)
Aldi_NeoPixel strip = Aldi_NeoPixel(LEDS, PIN, NEO_BRG + NEO_KHZ800);

int incomingInt = 0; // for incoming serial data
const byte numChars = 32;
char receivedChars[numChars];
char tempChars[numChars];        // temporary array for use when parsing

// variables to hold the parsed data
char messageFromPC[numChars] = {0};
int integerFromPC1 = 0;
int integerFromPC2 = 0;
int integerFromPC3 = 0;

uint32_t color = WHITE;

byte maxPeakL = 0;
byte peakCounterL = 0;
byte maxPeakR = 0;
byte peakCounterR = 0;

int runningCounter = 0;
int runningCounter2 = 0;
byte stepTime = 2;

char mode = 'C';

CircularBuffer<uint32_t, 50> stripBuffer;
uint32_t runningPattern[] = {RED, GREEN, BLUE, WHITE, 0, 0};

boolean newData = false;

void setup() {

  flushBuffer();

  strip.begin();
  strip.show(); // Initialize all strip to 'off'
  Serial.begin(9600); // opens serial port, sets data rate to 9600 bps
}

void loop() {
  recvWithStartEndMarkers();
  if (newData == true) {
    strcpy(tempChars, receivedChars);
    // this temporary copy is necessary to protect the original data
    //   because strtok() used in parseData() replaces the commas with \0
    parseData();
    // showParsedData();
    useData();
    newData = false;
  } else {
    if (mode == 'R' || mode == 'L') {
      runningLight();
    }
  }
}

void recvWithStartEndMarkers() {

  static boolean recvInProgress = false;
  static byte ndx = 0;
  char startMarker = '<';
  char endMarker = '>';
  char rc;

  while (Serial.available() > 0 && newData == false) {
    rc = Serial.read();

    if (recvInProgress == true) {
      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      }
      else {
        receivedChars[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    }

    else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
}

void parseData() {
  if (newData == true) {
    Serial.print("This just in ... ");
    Serial.println(receivedChars);
    //newData = false;

    char * strtokIndx; // this is used by strtok() as an index

    strtokIndx = strtok(tempChars, ",");     // get the first part - the string
    strcpy(messageFromPC, strtokIndx); // copy it to messageFromPC

    strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
    integerFromPC1 = atoi(strtokIndx);     // convert this part to an integer

    strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
    integerFromPC2 = atoi(strtokIndx);     // convert this part to an integer

    strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
    integerFromPC3 = atoi(strtokIndx);     // convert this part to an integer
  }
}

void showParsedData() {
  Serial.print("Message: ");
  Serial.println(messageFromPC);
  Serial.print("Integer 1: ");
  Serial.println(integerFromPC1);
  Serial.print("Integer 2: ");
  Serial.println(integerFromPC2);
  Serial.print("Integer 3: ");
  Serial.println(integerFromPC3);
}

void useData() {
  if (integerFromPC1 < 0) {
    integerFromPC1 = 0;
  }
  if (integerFromPC2 < 0) {
    integerFromPC2 = 0;
  }
  if (integerFromPC3 < 0) {
    integerFromPC3 = 0;
  }
  switch (messageFromPC[0]) {
    case 'C':
      mode = 'C';
      colorWipe();
      break;
    case 'V':
      if (mode != 'V') {
        mode = 'V';
      }
      vuMeter();
      break;
    case 'R':
      mode = 'R';
      if (integerFromPC1 > 10) {
        integerFromPC1 = 10;
      }
      if (integerFromPC1 <= 0) {
        integerFromPC1 = 1;
      }
      stepTime = integerFromPC1;
      runningCounter = 0;
      runningCounter2 = 0;
      flushBuffer();
      runningLight();
      break;
    case 'L':
      if (integerFromPC1 > 10) {
        integerFromPC1 = 10;
      }
      if (integerFromPC1 <= 0) {
        integerFromPC1 = 1;
      }
      stepTime = integerFromPC1;
      mode = 'L';
      runningCounter = 0;
      runningCounter2 = 0;
      flushBuffer();
      runningLight();
      break;
    default:
      mode = 'C';
      break;
  }
}

void colorWipe() {
  if (integerFromPC1 > 255) {
    integerFromPC1 = 255;
  }
  if (integerFromPC2 > 255) {
    integerFromPC2 = 255;
  }
  if (integerFromPC3 > 255) {
    integerFromPC3 = 255;
  }
  color = strip.Color(integerFromPC1, integerFromPC2, integerFromPC3);
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
    strip.show();
    delay(1);
  }
}

void vuMeter() {
  if (integerFromPC1 > LEDS / 2) {
    integerFromPC1 = LEDS / 2;
  }
  if (integerFromPC2 > LEDS / 2) {
    integerFromPC2 = LEDS / 2;
  }
  //left channel

  //staying peak
  if (integerFromPC1 >= maxPeakL) {
    maxPeakL = integerFromPC1;
    peakCounterL = 0;
  } else {
    if (peakCounterL > 4) {
      maxPeakL--;
    } else {
      peakCounterL++;
    }
  }
  for (uint16_t i = 0; i < strip.numPixels() / 2; i++) {
    if (i < integerFromPC1) {
      strip.setPixelColor(i, RED);
    } else {
      strip.setPixelColor(i, 0);
    }
    strip.show();
    //delay(1);
  }
  strip.setPixelColor(maxPeakL, WHITE);
  strip.show();
  //right channel

  //staying peak
  if (integerFromPC2 >= maxPeakR) {
    maxPeakR = integerFromPC2;
    peakCounterR = 0;
  } else {
    if (peakCounterL > 4) {
      maxPeakR--;
    } else {
      peakCounterR++;
    }
  }

  for (uint16_t i = strip.numPixels(); i >= strip.numPixels() / 2; i--) {
    if (strip.numPixels() - i < integerFromPC2) {
      strip.setPixelColor(i, GREEN);
    } else {
      strip.setPixelColor(i, 0);
    }
    strip.show();
    //delay(1);
  }
  strip.setPixelColor(strip.numPixels() - maxPeakR, WHITE);
  strip.show();
}

void flushBuffer() {
  for (uint16_t i = 0; i < stripBuffer.capacity; i++) {
    stripBuffer.push(0);
  }
}

void runningLight() {
  if (runningCounter % stepTime == 0) { //don't make a step each loop (speed)
    stripBuffer.pop();
    stripBuffer.unshift(runningPattern[runningCounter2 % (sizeof(runningPattern) / sizeof(uint32_t))]);
    runningCounter2++;
  }
  runningCounter++;

  switch (mode) {
    case 'R':
      for (uint16_t i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, stripBuffer[i]);
        strip.show();
        delay(1);
      }
      break;
    case 'L':
      for (uint16_t i = strip.numPixels(); i > 0; i--) {
        strip.setPixelColor(i-1, stripBuffer[strip.numPixels() - 1 - i]);
        strip.show();
        delay(1);
      }
      break;
  }

}
