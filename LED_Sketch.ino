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

byte lastPeakL = 0;
byte maxPeakL = 0;
byte peakCounterL = 0;
byte lastPeakR = 0;
byte maxPeakR = 0;
byte peakCounterR = 0;

int runningCounter = 0;
int runningCounter2 = 0;
byte stepTime = 2;

int fadeR = 255;
int fadeG = 0;
int fadeB = 0;

char mode = 'C';

CircularBuffer<uint32_t, 50> stripBuffer;
uint32_t runningPattern[] = {RED, GREEN, BLUE, WHITE, 0, 0};

boolean newData = false;

void setup() {
  flushBuffer();

  strip.begin();
  strip.show(); // Initialize the strip to 'off'
  Serial.begin(9600); // opens serial port, sets baud rate to 9600
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
    if (mode == 'F') {
      colorFade();
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
        for (uint16_t i = 0; i < strip.numPixels(); i++) {
          strip.setPixelColor(i, 0);
        }
        strip.show();
        mode = 'V';
      }
      vuMeter();
      break;
    case 'R':
      mode = 'R';
      integerFromPC1 %= 256;
      if (integerFromPC1 <= 1) {
        integerFromPC1 = 2;
      }
      stepTime = integerFromPC1;
      runningCounter = 0;
      runningCounter2 = 0;
      flushBuffer();
      runningLight();
      break;
    case 'L':
      mode = 'L';
      integerFromPC1 %= 256;
      if (integerFromPC1 <= 1) {
        integerFromPC1 = 2;
      }
      stepTime = integerFromPC1;
      runningCounter = 0;
      runningCounter2 = 0;
      flushBuffer();
      runningLight();
      break;
    case 'F':
      mode = 'F';
      integerFromPC1 %= 256;
      if (integerFromPC1 <= 1) {
        integerFromPC1 = 2;
      }
      stepTime = integerFromPC1;
      runningCounter = 0;
      flushBuffer();
      colorFade();
      break;
    default:
      mode = 'C';
      break;
  }
}

void colorWipe() {
  integerFromPC1 %= 256;
  integerFromPC2 %= 256;
  integerFromPC3 %= 256;

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
  if (integerFromPC3 > 255 || integerFromPC3 == 0) {
    integerFromPC3 = 255;
  }

  uint32_t red = strip.Color(1 * integerFromPC3, 0, 0);
  uint32_t green = strip.Color(0, 1 * integerFromPC3, 0);
  uint32_t white = strip.Color(1 * integerFromPC3, 1 * integerFromPC3, 1 * integerFromPC3);

  //left channel

  //last peak
  if (integerFromPC1 > lastPeakL) {
    //louder
    for (uint16_t i = lastPeakL; i < integerFromPC1; i++) {
      strip.setPixelColor(i, red);
    }
    //strip.show();
  } else {
    if (integerFromPC1 < lastPeakL) {
      //quieter
      for (uint16_t i = integerFromPC1; i < lastPeakL; i++) {
        strip.setPixelColor(i, 0);
      }
      //strip.show();
    }
  }
  lastPeakL = integerFromPC1;

  //staying peak
  if (integerFromPC1 >= maxPeakL) {
    maxPeakL = integerFromPC1;
    peakCounterL = 0;
  } else {
    if (peakCounterL > 4) {
      strip.setPixelColor(maxPeakL, 0);
      maxPeakL--;
    } else {
      peakCounterL++;
    }
  }
  strip.setPixelColor(maxPeakL, WHITE);
  //strip.show();

  //right channel

  //last peak
  if (integerFromPC2 > lastPeakR) {
    //louder
    for (uint16_t i = strip.numPixels() - lastPeakR; i > strip.numPixels() - integerFromPC2; i--) {
      strip.setPixelColor(i, green);
    }
    //strip.show();
  } else {
    if (integerFromPC2 < lastPeakR) {
      //quieter
      for (uint16_t i = strip.numPixels() - integerFromPC2; i > strip.numPixels() - lastPeakR; i--) {
        strip.setPixelColor(i, 0);
      }
      //strip.show();
    }
  }
  lastPeakR = integerFromPC2;

  //staying peak
  if (integerFromPC2 >= maxPeakR) {
    maxPeakR = integerFromPC2;
    peakCounterR = 0;
  } else {
    if (peakCounterL > 4) {
      strip.setPixelColor(strip.numPixels() - maxPeakR, 0);
      maxPeakR--;
    } else {
      peakCounterR++;
    }
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
      }
      strip.show();
      delay(10);
      break;
    case 'L':
      for (uint16_t i = strip.numPixels(); i > 0; i--) {
        strip.setPixelColor(i - 1, stripBuffer[strip.numPixels() - 1 - i]);
      }
      strip.show();
      delay(10);
      break;
  }
}

void colorFade() {
  if (runningCounter % stepTime == 0) {
    if (fadeR > 0 && fadeB == 0) {
      fadeR--;
      fadeG++;
    }
    if (fadeG > 0 && fadeR == 0) {
      fadeG--;
      fadeB++;
    }
    if (fadeB > 0 && fadeG == 0) {
      fadeB--;
      fadeR++;
    }
    uint32_t fadeColor = strip.Color(fadeR, fadeG, fadeB);
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, fadeColor);
    }
    strip.show();
  }
  runningCounter++;
}
