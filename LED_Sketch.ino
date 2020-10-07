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
int integerFromPC[numChars] = {0};
byte integerIndex = 0;

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
  setAll(1, 1, 1);
  strip.show(); // Initialize the strip to 'off'
  Serial.begin(9600); // opens serial port, sets baud rate to 9600
}

void loop() {
  recvWithStartEndMarkers();
  if (newData == true) {
    strcpy(tempChars, receivedChars);
    // this temporary copy is necessary to protect the original data
    //  because strtok() used in parseData() replaces the commas with \0
    parseData();
    newData = false;
    showParsedData();
    useData();
  } else {
    switch (mode) {
      case 'R':
        runningLight();
        break;
      case 'L':
        runningLight();
        break;
      case 'F':
        colorFade();
        break;
      case 'S':
        strobe();
        break;
      default:
        break;
    }
  }
}

void recvWithStartEndMarkers() {

  static boolean recvInProgress = false;
  static byte index = 0;
  char startMarker = '<';
  char endMarker = '>';
  char rc;

  while (Serial.available() > 0 && newData == false) {
    rc = Serial.read();

    if (recvInProgress == true) {
      if (rc != endMarker) {
        receivedChars[index] = rc;
        index++;
        if (index >= numChars) {
          index = numChars - 1;
        }
      }
      else {
        receivedChars[index] = '\0'; // terminate the string
        recvInProgress = false;
        index = 0;
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
    memset(integerFromPC, 0, sizeof(integerFromPC));
    integerIndex = 0;
    Serial.print("This just in ... ");
    Serial.println(receivedChars);

    char * strtokIndx; // this is used by strtok() as an index

    strtokIndx = strtok(tempChars, ",");     // get the first part - the string
    strcpy(messageFromPC, strtokIndx); // copy it to messageFromPC
    while (true) {
      //find the next ','
      strtokIndx = strtok(NULL, ",");

      //if not found, we're done
      if ( strtokIndx == NULL ) {
        break;
      }
      else {
        //parseValue
        integerFromPC[integerIndex] = atoi( strtokIndx );
        if (integerIndex < numChars) {
          integerIndex++;
        } else {
          break;
        }
      }
    }
  }
}

void showParsedData() {
  Serial.print("Message: ");
  Serial.println(messageFromPC);
  Serial.println("Integer array");
  for (int i = 0; i < numChars; i++) {
    Serial.println(integerFromPC[i]);
  }
}

void useData() {
  if (integerFromPC[0] < 0) {
    integerFromPC[0] = 0;
  }
  if (integerFromPC[1] < 0) {
    integerFromPC[1] = 0;
  }
  if (integerFromPC[2] < 0) {
    integerFromPC[2] = 0;
  }
  switch (messageFromPC[0]) {
    case 'C':
      mode = 'C';
      colorWipe();
      break;
    case 'V':
      if (mode != 'V') {
        setAll(0, 0, 0);
        mode = 'V';
      }
      vuMeter();
      break;
    case 'R':
      mode = 'R';
      integerFromPC[0] %= 256;
      if (integerFromPC[0] <= 1) {
        integerFromPC[0] = 2;
      }
      initializeCounter();
      runningCounter2 = 0;
      runningLight();
      break;
    case 'L':
      mode = 'L';
      integerFromPC[0] %= 256;
      if (integerFromPC[0] <= 1) {
        integerFromPC[0] = 2;
      }
      initializeCounter();
      runningCounter2 = 0;
      runningLight();
      break;
    case 'F':
      mode = 'F';
      integerFromPC[0] %= 256;
      if (integerFromPC[0] <= 1) {
        integerFromPC[0] = 2;
      }
      initializeCounter();
      colorFade();
      break;
    case 'S':
      mode = 'S';
      strobe();
      break;
    default:
      mode = 'C';
      break;
  }
}

void colorWipe() {
  integerFromPC[0] %= 256;
  integerFromPC[1] %= 256;
  integerFromPC[2] %= 256;

  setAll(integerFromPC[0], integerFromPC[1], integerFromPC[2], 1);
}

void vuMeter() {
  if (integerFromPC[0] > LEDS / 2) {
    integerFromPC[0] = LEDS / 2;
  }
  if (integerFromPC[1] > LEDS / 2) {
    integerFromPC[1] = LEDS / 2;
  }
  if (integerFromPC[2] > 255 || integerFromPC[2] == 0) {
    integerFromPC[2] = 255;
  }

  uint32_t red = strip.Color(1 * integerFromPC[2], 0, 0);
  uint32_t green = strip.Color(0, 1 * integerFromPC[2], 0);
  uint32_t white = strip.Color(1 * integerFromPC[2], 1 * integerFromPC[2], 1 * integerFromPC[2]);

  //left channel

  //last peak
  if (integerFromPC[0] > lastPeakL) {
    //louder
    for (uint16_t i = lastPeakL; i < integerFromPC[0]; i++) {
      strip.setPixelColor(i, red);
    }
    //strip.show();
  } else {
    if (integerFromPC[0] < lastPeakL) {
      //quieter
      for (uint16_t i = integerFromPC[0]; i < lastPeakL; i++) {
        strip.setPixelColor(i, 0);
      }
    }
  }
  lastPeakL = integerFromPC[0];

  //staying peak
  if (integerFromPC[0] >= maxPeakL) {
    maxPeakL = integerFromPC[0];
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
  //right channel

  //last peak
  if (integerFromPC[1] > lastPeakR) {
    //louder
    for (uint16_t i = LEDS - lastPeakR; i > LEDS - integerFromPC[1]; i--) {
      strip.setPixelColor(i, green);
    }
    //strip.show();
  } else {
    if (integerFromPC[1] < lastPeakR) {
      //quieter
      for (uint16_t i = LEDS - integerFromPC[1]; i > LEDS - lastPeakR; i--) {
        strip.setPixelColor(i, 0);
      }
    }
  }
  lastPeakR = integerFromPC[1];

  //staying peak
  if (integerFromPC[1] >= maxPeakR) {
    maxPeakR = integerFromPC[1];
    peakCounterR = 0;
  } else {
    if (peakCounterL > 4) {
      strip.setPixelColor(LEDS - maxPeakR, 0);
      maxPeakR--;
    } else {
      peakCounterR++;
    }
  }
  strip.setPixelColor(LEDS - maxPeakR, WHITE);
  strip.show();
}

void runningLight() {
  if (runningCounter % stepTime == 0) { //don't make a step each loop (speed)
    stripBuffer.pop();
    stripBuffer.unshift(runningPattern[runningCounter2 % (sizeof(runningPattern) / sizeof(long))]);
    runningCounter2++;
  }
  runningCounter++;

  switch (mode) {
    case 'R':
      for (byte i = 0; i < LEDS; i++) {
        strip.setPixelColor(i, stripBuffer[i]);
      }
      strip.show();
      delay(10);
      break;
    case 'L':
      for (byte i = LEDS; i > 0; i--) {
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
    setAll(fadeR, fadeG, fadeB);
  }
  runningCounter++;
}

void strobe() {
  integerFromPC[0] %= 256;  //flashDelay
  integerFromPC[1] %= 10000;  //flashPause

  for (byte i = 0; i < 1; i++) {
    setAll(255, 255, 255);
    delay(integerFromPC[0]);
    setAll(0, 0, 0);
    delay(integerFromPC[1]);
  }
}

//general functions

void setAll(byte colorR, byte colorG, byte colorB) {
  long color = strip.Color(colorR, colorG, colorB);
  for (byte i = 0; i < LEDS; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void setAll(byte colorR, byte colorG, byte colorB, byte speed) {
  long color = strip.Color(colorR, colorG, colorB);
  for (byte i = 0; i < LEDS; i++) {
    strip.setPixelColor(i, color);
    strip.show();
    delay(speed);
  }
}

void flushBuffer() {
  for (byte i = 0; i < stripBuffer.capacity; i++) {
    stripBuffer.push(0);
  }
}

void initializeCounter() {
  stepTime = integerFromPC[0];
  runningCounter = 0;
  flushBuffer();
}
