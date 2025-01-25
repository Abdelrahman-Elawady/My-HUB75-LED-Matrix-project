// HUB75E pinouts (64*64 pixels led has D & E both; test using continuity in multimeter)
// R1 | G1
// B1 | GND
// R2 | G2
// B2 | E
//  A | B
//  C | D
// CLK| LAT
// OE | GND

#define R1 25
#define G1 26
#define BL1 27
#define R2 14
#define G2 12
#define BL2 13
#define CH_A 23
#define CH_B 19
#define CH_C 5
#define CH_D 17
#define CH_E 22
#define CLK 16
#define LAT 4
#define OE 15

//libraries
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h> //has adafruit GFX as dependency
#include <FastLED.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <Fonts/FreeSerifBold9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
//note: when using fonts, thebaseline becomes at bottom left 
//instead of top left of defautl font which returns -ve value 
//for the height so invert that in code 
//additionally, too many fonts imported will take too much memory as it comes in bitmaps not vectors

#ifndef PSTR
#define PSTR // Macro to store strings in flash memory (PROGMEM) instead of SRAM
#endif

#include "emoji_arrays.h" //emoji arrays

// Configuration for panels
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 64  	// Panel height of 64 will required PIN_E to be defined.
#define PANELS_NUMBER 2 	// Number of chained panels
#define PIN_E 22

#define PANE_WIDTH PANEL_WIDTH * PANELS_NUMBER //total width
#define PANE_HEIGHT PANEL_HEIGHT //total height


// placeholder for the matrix object
MatrixPanel_I2S_DMA *matrix = nullptr;

const uint32_t passkey = 121213; // password for BLE

// variables
String msg = ""; //stored current message text to display
String received = ""; //recieved user input in bluetooth


// default user control variables
String defaultMessage = "CPD Center Welcomes You"; //default message to change in flash memory
int defaultBrightness = 122; //0-255 128=50% approx. (will be restricted to 150 in app)
int defaultTextSize = 3; //5 should be maximum (more than 60% screen height)
int duration = 20; //this controls the speed of the scrolling text (smaller = faster)
bool multiColor = false; //to control multicolor mode
bool displayOn = true; //to control screen on/off mode
bool plasmaMode = false; // to activate plasma mode
bool textWrapStatic = false; // Text wrapping control for static text
bool showOneEmoji = false; //if the user wants to display an emoji
bool showTwoEmojis = false; //if the user wants to display two emojis
String requestedEmojiOne = ""; //default emoji displayed
String requestedEmojiTwo = ""; // ***
const uint16_t *currentEmojiOne = nullptr; //where emoji array will be stored to display
const uint16_t *currentEmojiTwo = nullptr; //***
int emojiWidthOne = 32;
int emojiWidthTwo = 32;
const GFXfont *currentFont = nullptr;
String requestedFont = "Default";

uint16_t time_counter = 0, cycles = 0, fps = 0; // for plasma effect
unsigned long fps_timer; // ***

CRGB currentColor; //for plasma mode colors
CRGBPalette16 palettes[] = {HeatColors_p, LavaColors_p, RainbowColors_p, RainbowStripeColors_p, CloudColors_p};
CRGBPalette16 currentPalette = palettes[0];
CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND) {
  return ColorFromPalette(currentPalette, index, brightness, blendType);
}

uint16_t myBLACK = matrix->color565(0, 0, 0);
uint16_t myWHITE = matrix->color565(255, 255, 255);
uint16_t myRED = matrix->color565(255, 0, 0);
uint16_t myGREEN = matrix->color565(0, 255, 0);
uint16_t myBLUE = matrix->color565(0, 0, 255);
uint16_t myYELLOW = matrix->color565(255, 255, 0);
uint16_t myCYAN = matrix->color565(0, 255, 255);
uint16_t myMAGENTA = matrix->color565(255, 0, 255);
uint16_t CPDThemeBlue = matrix->color565(0, 106, 110);
uint16_t CPDThemeYellow = matrix->color565(222, 185, 78);
uint16_t CPDThemeGreen = matrix->color565(114, 152, 151);
uint16_t myCOLORS[7] = {myRED, myGREEN, myBLUE, myWHITE, myYELLOW, myCYAN, myMAGENTA};

uint16_t defaultColor = CPDThemeBlue; //default color for the user input
uint8_t wheelval = 0; //for the multicolor mode colors iterations

// declaring pointer variable to BLE objects
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristicTX = NULL;
BLECharacteristic* pCharacteristicRX = NULL;
// control connectivety of the client
bool deviceConnected = false;
bool oldDeviceConnected = false;
//Universally Unique Identifier is a 128-bit value used to uniquely identify services, characteristics, and descriptors within the BLE protocol
#define SERVICE_UUID "d8a07eb5-215a-42c4-8ec4-514e1cdc6b78"
#define CHARACTERISTIC_UUID_RX "21687baa-dfe1-4f13-bbcc-0a787e9d85a2"
#define CHARACTERISTIC_UUID_TX "95b0807e-bdeb-4a9e-b3bc-3e3e757937e9"

//declaring a class named MyServerCallbacks that inherits from the BLEServerCallbacks class
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

//function to display the current status of the screen and send it to user
void sendDisplayStatus() {
  // Extract RGB values from textColor by bit shifting and bit-wise masking starting at LSB >> RGB
  uint8_t red = (defaultColor >> 11) & 0x1F;
  uint8_t green = (defaultColor >> 5) & 0x3F;
  uint8_t blue = defaultColor & 0x1F;

  // Scale the values to 8-bit (0-255) range
  red = map(red, 0, 31, 0, 255);
  green = map(green, 0, 63, 0, 255);
  blue = map(blue, 0, 31, 0, 255);
  String status = "Message: " + msg 
  + "\n" + "Color: " + String(red) + "," + String(green) + "," + String(blue) 
  + "\n" + "Font: " + requestedFont
  + "\n" + "text Size: " + String(defaultTextSize)  
  + "\n" + "Speed: " + String(duration) 
  + "\n" + "Brightness: " + String(defaultBrightness) 
  + "\n" + "Display On: " + String(displayOn ? "Yes" : "No") 
  + "\n" + "Multicolor: " + String(multiColor ? "On" : "Off") 
  + "\n" + "Emoji_1: " + requestedEmojiOne 
  + "\n" + "Emoji_2: " + requestedEmojiTwo 
  + "\n" + "Wrap text mode: " + String(textWrapStatic ? "Yes" : "No")
  + "\n" + "Plasma mode: " + String(plasmaMode ? "Yes" : "No")
;

  // Debug print to verify the status string
  // Serial.println("Sending status:");
  // Serial.println(status);

  // Convert the status string to a C-style string (an array of each letter) which is required by the setvalue method to use
  const char* statusCharArray = status.c_str();

  // Set the value and notify
  pCharacteristicTX->setValue((uint8_t*)statusCharArray, strlen(statusCharArray));
  pCharacteristicTX->notify();
}

//declaring MyCallbacks class to handle use characteritic written input
class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0) {
      received = String(rxValue.c_str()); 
    } 
    if (received.startsWith("speed:")) {
      int newDuration = received.substring(6).toInt();
      if (newDuration > 0 && newDuration <= 200) { 
        duration = newDuration;
      }
    } else if (received.startsWith("size:")) {
      int newSize = received.substring(5).toInt();
      if (newSize < 1 || newSize > 10) { 
        newSize = 1; 
      }
      defaultTextSize = newSize;
    } else if (received.startsWith("plasma:on")) {
      plasmaMode = true;
      displayOn = false;
    } else if (received.startsWith("plasma:off")) {
      plasmaMode = false;
      displayOn = true;
    } else if (received.startsWith("wrap:on")) {
        textWrapStatic = true;
    } else if (received.startsWith("wrap:off")) {
        textWrapStatic = false;
    } else if (received.startsWith("emoji1:")) {
      showOneEmoji = true;
      requestedEmojiOne = received.substring(7);
    } else if (received.startsWith("emoji2:")) {
      showTwoEmojis = true;
      requestedEmojiTwo = received.substring(7);
    } else if (received.startsWith("emoji1:off")) {
      showOneEmoji = false;
      requestedEmojiOne = "";
      currentEmojiOne = nullptr; 
    } else if (received.startsWith("emoji2:off")) {
      showTwoEmojis = false;
      requestedEmojiTwo = "";
      currentEmojiTwo = nullptr; 
    } else if (received.startsWith("font:serif")) {
      currentFont = &FreeSerifBold9pt7b;
      requestedFont = "Serif";
    } else if (received.startsWith("font:sans")) {
      currentFont = &FreeSansBold9pt7b;
      requestedFont = "Sans";
    } else if (received.startsWith("font:mono")) {
      currentFont = &FreeMonoBold9pt7b;
      requestedFont = "Mono";
    } else if (received.startsWith("font:default")) {
      currentFont = nullptr;
      requestedFont = "Default";
    } else if (received.startsWith("brightness:")) {
      int brightness = received.substring(11).toInt();
      if (brightness >= 0 && brightness <= 200) { 
        matrix->setBrightness8(brightness);
        defaultBrightness = brightness;
      }
    } else if (received.startsWith("color:")) {
      int r = received.substring(6, 9).toInt();
      int g = received.substring(10, 13).toInt();
      int b = received.substring(14, 17).toInt();
      defaultColor = matrix->color565(r, g, b);
    } else if (received == "multicolor:on") {
      multiColor = true;
    } else if (received == "multicolor:off") {
      multiColor = false;
    } else if (received == "off") {
      displayOn = false;
      plasmaMode = false;
      matrix->clearScreen();
      matrix->flipDMABuffer();
    } else if (received == "on") {
      displayOn = true;
    } else if (received == "reset") {
      ESP.restart();
    } else {
      msg = String(rxValue.c_str()); 
    }
    sendDisplayStatus();
  }
};

void setup() {
  Serial.begin(115200);
  
  BLEDevice::init("CPD_INDOORS");   // Create the BLE Device
  //BLEDevice::setMTU(517);  // Set MTU size to 517 bytes

  // BLE security
  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND); //MITM authentication mode for bonding and protectino from 3rd party
  pSecurity->setCapability(ESP_IO_CAP_IO); //sets input capability of the pin
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK); //to ensure encyrption
  pSecurity->setStaticPIN(passkey); // Set static passkey

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService* pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristicTX = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY);

  BLE2902* pBLE2902 = new BLE2902(); //creating new BLE descriptor used for notifications
  pBLE2902->setNotifications(true); //enabling  notififcations
  pCharacteristicTX->addDescriptor(pBLE2902); //adding the descriptor to a characterstic

  // Set access permissions for TX characteristic
  pCharacteristicTX->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);

  // Create a BLE Characteristic for RX
  pCharacteristicRX = pService->createCharacteristic(
    CHARACTERISTIC_UUID_RX,
    BLECharacteristic::PROPERTY_WRITE);
  pCharacteristicRX->setCallbacks(new MyCallbacks());

  // Set access permissions for RX characteristic
  pCharacteristicRX->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);

  pService->start();
  // Start advertising
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x00);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  //Serial.println("Waiting a client connection to notify...");
  // Request MTU size
  pServer->getAdvertising()->setMinPreferred(0x06);  // 6 times 1.25ms advertising data rate interval

  //set up matrix constructor parameters
  HUB75_I2S_CFG::i2s_pins _pins={R1, G1, BL1, R2, G2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK};
  HUB75_I2S_CFG mxconfig(
                          PANEL_WIDTH,   // width
                          PANEL_HEIGHT,   // height
                          PANELS_NUMBER,   // chain length
                        _pins  // pin mapping
  );
  //mxconfig.clkphase = false; //if facing any problems with rows or columns alignment
  //mxconfig.gpio.e = 22; //another way to configure pin E
  //mxconfig.driver = HUB75_I2S_CFG::FM6126A; //for different driver check your schematic
  //mxconfig.latch_blanking = 4; //for ghosting problems increase time between clocking in data and turning leds on
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M; //I2S clock speed, only for experimentation
  //mxconfig.driver = HUB75_I2S_CFG::SHIFT;   // shift reg driver, default is plain shift register
  mxconfig.double_buff = true; // double ram usage & smoother graphics and less flicker

  // mxconfig.driver = HUB75_I2S_CFG::ICN2038S;
	// mxconfig.driver = HUB75_I2S_CFG::FM6124;
	// mxconfig.driver = HUB75_I2S_CFG::MBI5124;

  // Now we can create our matrix object
  matrix = new MatrixPanel_I2S_DMA(mxconfig);

  // Allocate memory and start DMA display
  if( not matrix->begin() ) {
    Serial.println("__memory allocation failed__");
    //ESP.restart();
  }
  
  //pass default values to the screen matrix object
  matrix->setBrightness8(defaultBrightness);
  matrix->clearScreen(); //to remove artifacts on start
  matrix->setTextSize(defaultTextSize);
  matrix->setFont(currentFont);
  // Set current FastLED palette
  currentPalette = RainbowColors_p;
  fps_timer = millis(); //for plasma mode

  // Display startup message
  msg = defaultMessage;
  drawBars();
}

//start main loop function
void loop() {
  //notify changed value
  if (deviceConnected) {
    sendDisplayStatus();
    delay(3);  // bluetooth stack will go into congestion, if too many packets are sent.
  }

  //disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);                   // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising();  // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
  matrix->flipDMABuffer(); // Show the back buffer, set currently output buffer to the back (i.e. no longer being sent to LED panels)
  matrix->clearScreen();   // Now clear the back-buffer
  delay(duration); //shouldn't see this clearscreen occur as it happens on the back buffer when double buffering is enabled.

  //for testing before adding bluetooth code (in essence all these boolean values can be controlled via web app and toggle switches)
  // if (Serial.available() > 0) {
  //   String input = Serial.readStringUntil('\n');
  //   input.trim(); // Remove any extra whitespace
  //   handleInput(input); 
  // }

  if (plasmaMode) {
    matrix->clearScreen();
    plasmaDemo();
  }

  if (showOneEmoji) {
    if (requestedEmojiOne=="heart") {
        currentEmojiOne = heart;
      } else if (requestedEmojiOne=="grin") {
        currentEmojiOne = grin;
      } else if (requestedEmojiOne=="smiley") {
        currentEmojiOne = smiley;
      } else if (requestedEmojiOne=="hugging_face") {
        currentEmojiOne = hugging_face;
      } else if (requestedEmojiOne=="relaxed") {
        currentEmojiOne = relaxed;
      } else if (requestedEmojiOne=="star_struck") {
        currentEmojiOne = star_struck;
      } else if (requestedEmojiOne=="thumbs_up") {
        currentEmojiOne = thumbs_up;
      } else if (requestedEmojiOne=="rose") {
        currentEmojiOne = rose;
      } else if (requestedEmojiOne=="fire") {
        currentEmojiOne = fire;
      } else if (requestedEmojiOne=="books") {
        currentEmojiOne = books;
      } else if (requestedEmojiOne=="eyes") {
        currentEmojiOne = eyes;
      } else if (requestedEmojiOne=="grad_hat") {
        currentEmojiOne = grad_hat;
      } else if (requestedEmojiOne=="male_technologist") {
        currentEmojiOne = male_technologist;
      } else if (requestedEmojiOne=="male_student") {
        currentEmojiOne = male_student;
      } else if (requestedEmojiOne=="female_student") {
        currentEmojiOne = female_student;
      } else if (requestedEmojiOne=="blank_person") {
        currentEmojiOne = blank_person;
      } else if (requestedEmojiOne=="check_mark") {
        currentEmojiOne = check_mark;
      } else if (requestedEmojiOne=="x") {
        currentEmojiOne = x;
      } else if (requestedEmojiOne=="heart_eyes") {
        currentEmojiOne = heart_eyes;
      } else if (requestedEmojiOne=="flag_sa") {
        currentEmojiOne = flag_sa;
      } else if (requestedEmojiOne=="wave") {
        currentEmojiOne = wave;
      } else if (requestedEmojiOne=="pencil") {
        currentEmojiOne = pencil;
      } else if (requestedEmojiOne=="upm2_logo") {
        currentEmojiOne = upm2_logo;
      } else if (requestedEmojiOne=="cpd_center_logo") {
        currentEmojiOne = cpd_center_logo;
      } else {
        currentEmojiOne = nullptr;
        requestedEmojiOne = "";
      }
    }

  if (showTwoEmojis) {
    if (requestedEmojiTwo=="heart") {
      currentEmojiTwo = heart;
    } else if (requestedEmojiTwo=="grin") {
      currentEmojiTwo = grin;
    } else if (requestedEmojiTwo=="smiley") {
      currentEmojiTwo = smiley;
    } else if (requestedEmojiTwo=="hugging_face") {
      currentEmojiTwo = hugging_face;
    } else if (requestedEmojiTwo=="relaxed") {
      currentEmojiTwo = relaxed;
    } else if (requestedEmojiTwo=="star_struck") {
      currentEmojiTwo = star_struck;
    } else if (requestedEmojiTwo=="thumbs_up") {
      currentEmojiTwo = thumbs_up;
    } else if (requestedEmojiTwo=="rose") {
      currentEmojiTwo = rose;
    } else if (requestedEmojiTwo=="fire") {
      currentEmojiTwo = fire;
    } else if (requestedEmojiTwo=="books") {
      currentEmojiTwo = books;
    } else if (requestedEmojiTwo=="eyes") {
      currentEmojiTwo = eyes;
    } else if (requestedEmojiTwo=="grad_hat") {
      currentEmojiTwo = grad_hat;
    } else if (requestedEmojiTwo=="male_technologist") {
      currentEmojiTwo = male_technologist;
    } else if (requestedEmojiTwo=="male_student") {
      currentEmojiTwo = male_student;
    } else if (requestedEmojiTwo=="female_student") {
      currentEmojiTwo = female_student;
    } else if (requestedEmojiTwo=="blank_person") {
      currentEmojiTwo = blank_person;
    } else if (requestedEmojiTwo=="check_mark") {
      currentEmojiTwo = check_mark;
    } else if (requestedEmojiTwo=="x") {
      currentEmojiTwo = x;
    } else if (requestedEmojiTwo=="heart_eyes") {
      currentEmojiTwo = heart_eyes;
    } else if (requestedEmojiTwo=="flag_sa") {
      currentEmojiTwo = flag_sa;
    } else if (requestedEmojiTwo=="wave") {
      currentEmojiTwo = wave;
    } else if (requestedEmojiTwo=="pencil") {
      currentEmojiTwo = pencil;
    } else if (requestedEmojiTwo=="cpd_center_logo") {
      currentEmojiTwo = cpd_center_logo;
    } else if (requestedEmojiTwo=="upm2_logo") {
      currentEmojiTwo = upm2_logo;
    } else {
      currentEmojiTwo = nullptr;
      requestedEmojiTwo = "";
    }
  }

  if (displayOn) {
    displayMessage(msg, defaultTextSize, defaultColor, wheelval, currentEmojiOne, currentEmojiTwo, emojiWidthOne, emojiWidthTwo); 
  }

  //incrementing these for chnage in color pallete and multicolor mode
  ++time_counter;
  ++cycles;
  ++fps;
  wheelval +=1;

  if (cycles >= 1024) {
    time_counter = 0;
    cycles = 0;
    wheelval = 0;
    currentPalette = palettes[random(0,sizeof(palettes)/sizeof(palettes[0]))];
  }
  
  if (fps_timer + 5000 < millis()){
    //Serial.printf_P(PSTR("Effect fps: %d\n"), fps/5); //for testing
    fps_timer = millis();
    fps = 0;
  }

  //delay(4);
} 
//end of loop

          //*******************displaying functions************************

// Function to display and scroll text with various options and emojis
void displayMessage(String text, int mysize, uint16_t color, int colorOffSet, const uint16_t *myEmoji1, const uint16_t *myEmoji2, int imgWidth1, int imgWidth2) {

  int16_t x1, y1;
  uint16_t w, h; 

  if (mysize < 1) { mysize = 1;}
  matrix->setTextWrap(false); //in scrolling don't wrap 
  matrix->setFont(currentFont); // Apply the current selected font   
  matrix->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  int totalHeight = matrix->height();
  int startingXPos = matrix->width(); //initial matrix width 128 is the start from right of screen
  int endXPos; //end position off screen to the left 

  int imgY1 = (totalHeight - imgWidth1)/2;
  int imgY2 = imgY1;
  int textY;

  if (currentFont!=nullptr) {
    textY = (totalHeight + h - 6) / 2;
    matrix->setTextSize(2);
  } else {
    textY = (totalHeight - h) / 2;
    matrix->setTextSize(mysize);
  }

  if (showOneEmoji && myEmoji1 != nullptr && showTwoEmojis && myEmoji2 != nullptr) {
    endXPos = -w - imgWidth1 - imgWidth2 - 4;
  } else if (showOneEmoji && myEmoji1 != nullptr) { 
    endXPos = -w - imgWidth1 -2; 
  } else if (showTwoEmojis && myEmoji2 != nullptr) { 
    endXPos = -w - imgWidth1 - imgWidth2 - 4;
  } else { 
    endXPos = -w;
  }

  // Set text color based on message content
  if (msg == "Unavailable" || msg == "unavailable") {
    matrix->setTextColor(myRED);
  } else if (msg == "Available" || msg == "available") {
    matrix->setTextColor(myGREEN);
  } else {
    matrix->setTextColor(color); //this overrides the setTextColor in the setup
  }

  if (!textWrapStatic) {
    if (-endXPos> matrix->width()) { // Only scroll if message width is larger than screen width (-ve cuz endXPos is -ve)
      // once x is less than endX, for loop ends and when main loop starts over, the for loop starts over and decrements x position
      for (int x = startingXPos; x > endXPos; x--) {
        matrix->clearScreen();
        int imgX1 = x + w + 2; // to position right of the scrolling text
        int imgX2 = imgX1 + imgWidth1 + 2; // to position right of the emoji1
        int textX = x;

        if (showOneEmoji && myEmoji1 != nullptr) {
          displayEmoji(myEmoji1, imgX1, imgY1, imgWidth1, imgWidth1);
        } 
        if (showTwoEmojis && myEmoji2 != nullptr) {
          displayEmoji(myEmoji2, imgX2, imgY2, imgWidth2, imgWidth2);
        }

        if (multiColor) { 
          drawTextColored(text, colorOffSet + x, textX, textY); 
        } else { 
          matrix->setCursor(textX, textY); 
          matrix->print(text); 
        }
        //matrix->flipDMABuffer(); // Show the back buffer, set currently output buffer to the back (i.e. no longer being sent to LED panels)
        //display->clearScreen();   // Now clear the back-buffer
        //delay(duration); //shouldn't see this clearscreen occur as it happens on the back buffer when double buffering is enabled.
      }

    } else { // No scrolling, just display the text if it's smaller than screen width
      matrix->clearScreen(); 
      int textX = (matrix->width() + endXPos) / 2;
      int imgX1 = textX + w + 2;
      int imgX2 = 2 + imgX1 + imgWidth1;

      if (showOneEmoji && myEmoji1 != nullptr) {
        displayEmoji(myEmoji1, imgX1, imgY1, imgWidth1, imgWidth1);
      } 
      if (showTwoEmojis && myEmoji2 != nullptr) {
        displayEmoji(myEmoji2, imgX2, imgY2, imgWidth2, imgWidth2);
      }

      if (multiColor) { 
        drawTextColored(text, colorOffSet, textX, textY); 
      } else { 
        matrix->setCursor(textX, textY);
        matrix->print(text); 
      }
      //matrix->flipDMABuffer();
    }

  } else { // if text wrapping mode
    matrix->clearScreen(); 
    matrix->setTextWrap(true);
    matrix->setFont(nullptr);
    if (multiColor) { 
      drawTextColored(text, colorOffSet, 0, 0); 
    } else {
      matrix->setCursor(0, 0);
      matrix->print(text); 
    }
    //matrix->flipDMABuffer();
  }
}

//Function to display plasma effect animation
void plasmaDemo() {
  for (int x = 0; x < PANE_WIDTH; x++) {
    for (int y = 0; y <  PANE_HEIGHT; y++) {
        int16_t v = 0;
        uint8_t wibble = sin8(time_counter);
        v += sin16(x * wibble * 3 + time_counter);
        v += cos16(y * (128 - wibble)  + time_counter);
        v += sin16(y * x * cos8(-time_counter) / 8);

        currentColor = ColorFromPalette(currentPalette, (v >> 8) + 127); //, brightness, currentBlendType);
        matrix->drawPixelRGB888(x, y, currentColor.r, currentColor.g, currentColor.b);
    }
  }
  //matrix->flipDMABuffer();
}

// Functions to display colored text
void drawTextColored(String myText, int colorWheelOffset, int myXPos, int myYPos) {
  matrix->setCursor(myXPos, myYPos);    // start at top left, with 8 pixel of spacing
  uint8_t w = 0;

  for (w=0; w<myText.length(); w++) {
    matrix->setTextColor(colorWheel((w*32)+colorWheelOffset));
    matrix->print(myText[w]);
  }
}

//color wheel for colored text effect
uint16_t colorWheel(uint8_t pos) {
  if(pos < 85) {
    return matrix->color565(pos * 3, 255 - pos * 3, 0);
  } else if(pos < 170) {
    pos -= 85;
    return matrix->color565(255 - pos * 3, 0, pos * 3);
  } else {
    pos -= 170;
    return matrix->color565(0, pos * 3, 255 - pos * 3);
  }
}

// Function to display a xx*yy emoji from an array at a specified position
void displayEmoji(const uint16_t *emojiArray, int16_t xpos, int16_t ypos, int xx1, int yy1) {
  for (int y = 0; y < yy1; y++) {
    for (int x = 0; x < xx1; x++) {
      uint16_t color = pgm_read_word(&emojiArray[y * yy1 + x]); //read word for 16-bits and & for referencing the same variable in memory
      matrix->drawPixel(x + xpos, y + ypos, color);
    }
  }
}


//animation in beginning
void drawBars() {
  int rnd;
  for (int i = 0; i < 10; i++) {
    matrix->fillScreenRGB888(0, 0, 0); //full black
    for (int x = 0; x < 128; x++) {  // Loop through all 128 columns
      rnd = random(1, 63);
      matrix->drawLine(x, 63, x, rnd, myGREEN);
      matrix->drawPixel(x, rnd - 1, myRED);
    }
    matrix->flipDMABuffer();  
    delay(300);
  }
}

        //********************Testing Functions*****************************
// void testText() {
//   matrix->clearScreen();
//   matrix->setTextColor(matrix->color565(255, 0, 0));
//   matrix->setCursor(0, 0);
//   matrix->setTextWrap(true);
//   matrix->setTextSize(1);
//   matrix->print("omg this thing is working!!");
//   matrix->flipDMABuffer();
//   delay(5000);
//   matrix->clearScreen();

//   matrix->setTextColor(matrix->color565(0, 255, 0));  
//   matrix->setCursor(0, 17);
//   matrix->setTextWrap(true);
//   matrix->setTextSize(2);
//   matrix->print("omg this thing is working again!!");
//   matrix->flipDMABuffer();
//   delay(5000);
//   matrix->clearScreen();

//   matrix->setTextColor(matrix->color565(55, 55,55));
//   matrix->setCursor(2, 32);
//   matrix->setTextWrap(false);
//   matrix->setTextSize(3);
//   matrix->print("omg this thing is working again!!");
//   matrix->flipDMABuffer();
//   delay(5000);
//   matrix->clearScreen();
// }

// void testShapes() {
//   matrix->clearScreen();
//   // fix the screen with green
//   matrix->fillRect(0, 0, matrix->width(), matrix->height(), matrix->color444(0, 15, 0));
//   matrix->flipDMABuffer();  
//   delay(1000);

//   // draw a box in yellow
//   matrix->drawRect(0, 0, matrix->width(), matrix->height(), matrix->color444(15, 15, 0));
//   matrix->flipDMABuffer();  
//   delay(1000);

//   // draw an 'X' in red
//   matrix->drawLine(0, 0, matrix->width()-1, matrix->height()-1, matrix->color444(15, 0, 0));
//   matrix->drawLine(matrix->width()-1, 0, 0, matrix->height()-1, matrix->color444(15, 0, 0));
//   matrix->flipDMABuffer();
//   delay(1000);

//   // draw a blue circle
//   matrix->drawCircle(10, 10, 10, matrix->color444(0, 0, 15));
//   matrix->flipDMABuffer();  
//   delay(1000);

//   // fill a violet circle
//   matrix->fillCircle(40, 21, 10, matrix->color444(15, 0, 15));
//   matrix->flipDMABuffer();
//   delay(1000);
// }

// void testRGB() {
//   matrix->clearScreen();
//   matrix->fillScreenRGB888(255, 0, 0); //full red
//   matrix->flipDMABuffer();
//   delay(1000);

//   matrix->fillScreenRGB888(0, 255, 0); //full green
//   matrix->flipDMABuffer();
//   delay(1000);

//   matrix->fillScreenRGB888(0, 0, 255); //full blue
//   matrix->flipDMABuffer();
//   delay(1000);

//   matrix->fillScreenRGB888(64, 64, 64); //white
//   matrix->flipDMABuffer();
//   delay(1000);

//   matrix->fillScreenRGB888(0, 0, 0); //black = off
//   matrix->flipDMABuffer();  
//   delay(1000);
// }


//for testing before uncommenting bluetooth code, use serial monitor:
// void handleInput(String &input) {
//   if (input.startsWith("yellow")) {
//     defaultColor = CPDThemeYellow;
//   } else if (input.startsWith("green")) {
//     defaultColor = CPDThemeGreen;
//   } else if (input.startsWith("size:")) {
//     int newSize = input.substring(5).toInt();
//     if (newSize < 1) {
//       newSize = 1;
//     }
//     defaultTextSize = newSize;

//   } else if (input.startsWith("speed:")) {
//     duration = input.substring(6).toInt();

//   } else if (input.startsWith("emoji1:")) {
//     showOneEmoji = true;
//     requestedEmojiOne = input.substring(7);

//   } else if (input.startsWith("emoji2:")) {
//     showTwoEmojis = true;
//     requestedEmojiTwo = input.substring(7);

//   } else if (input.startsWith("emoji1:off")) {
//     showOneEmoji = false;
//     requestedEmojiOne = "";
//     currentEmojiOne = nullptr; // Reset the emoji pointer

//   } else if (input.startsWith("emoji2:off")) {
//     showTwoEmojis = false;
//     requestedEmojiTwo = "";
//     currentEmojiTwo = nullptr; // Reset the emoji pointer
    
//   } else if (input.startsWith("multicolor:on")) {
//     multiColor = true;
//   } else if (input.startsWith("multicolor:off")) {
//     multiColor = false;
//   } else if (input.startsWith("plasma:on")) {
//     plasmaMode = true;
//     displayOn = false;
//   } else if (input.startsWith("plasma:off")) {
//     plasmaMode = false;
//     displayOn = true;
//   } else if (input.startsWith("on")) {
//     displayOn = true;
//   } else if (input.startsWith("off")) {
//     displayOn = false;
//     matrix->clearScreen();
//     matrix->flipDMABuffer();
//   } else if (input.startsWith("blue")) {
//     defaultColor = myBLUE;
//   } else if (input.startsWith("wrap:on")) {
//     textWrapStatic = true;
//   } else if (input.startsWith("wrap:off")) {
//     textWrapStatic = false;
//   } else if (input.startsWith("brightness:")) {
//     defaultBrightness = input.substring(11).toInt();
//     matrix->setBrightness8(defaultBrightness);
//   } else if (input.startsWith("test text")) {
//     displayOn = false;
//     testText();
//     displayOn = true;
//   } else if (input.startsWith("test rgb")) {
//     displayOn = false;
//     testRGB();
//     displayOn = true;
//   } else if (input.startsWith("test shapes")) {
//     displayOn = false;
//     testShapes();
//     displayOn = true;
//   } else if (input.startsWith("red")) {
//     defaultColor = myRED;
//   } else if (input.startsWith("font:serif")) {
//     currentFont = &FreeSerif9pt7b;
//   } else if (input.startsWith("font:sans")) {
//     currentFont = &FreeSans9pt7b;
//   } else if (input.startsWith("font:mono")) {
//     currentFont = &FreeMono9pt7b;
//   } else if (input.startsWith("font:serifb")) {
//     currentFont = &FreeSerifBold9pt7b;
//   } else if (input.startsWith("font:sansb")) {
//     currentFont = &FreeSansBold9pt7b;
//   } else if (input.startsWith("font:monob")) {
//     currentFont = &FreeMonoBold9pt7b;
//   } else if (input.startsWith("font:default")) {
//     currentFont = nullptr;
//   } else {
//     msg = input;
//   }
// }

//end of the code....