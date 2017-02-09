/*
 https://forum.mysensors.org/topic/4934/wall-mounted-mood-light-v2/2
 PROJECT: MySensors / RGB light NEOPIXEL
 PROGRAMMER: AWI
 DATE: october 10, 2015/ last update: september 20, 2016
 FILE: AWI_Wall_LIght_x.ino
 LICENSE: Public domain

 Hardware: Nano and MySensors 2.0, Wall light 16 WS2812B leds (neopixel)
    
 Special:
  uses Fastled library with NeoPixel (great & fast RBG/HSV universal library)       https://github.com/FastLED/FastLED
  
 SUMMARY:
  
  Different patterns and brightness settings
  
  Button switches on/off and cycles through all Color patterns on long press
  
 Remarks:
  Fixed node-id
  
 Change log:
 20160915 - Updated to MySensors 2.0
 20160920 - Changed state change to dimmer i.s.o. switch()

*/
//****  MySensors *****
// Enable debug prints to serial monitor
#define MY_DEBUG 
#define MY_RADIO_NRF24                    // Enable and select radio type attached
//#define MY_RF24_CHANNEL 80                // radio channel, default = 76

#define MY_NODE_ID 9
#define NODE_TXT "W 9"                   // Text to add to sensor name

// #define MY_RF24_CE_PIN 7                 // Ceech board, 3.3v (7,8)  (pin default 9,10)
// #define MY_RF24_CS_PIN 8

// helpers
#define LOCAL_DEBUG                     // enable if print wanted 

#ifdef LOCAL_DEBUG
#define Sprint(a) (Serial.print(a))             // macro as substitute for print, enable if no print wanted
#define Sprintln(a) (Serial.println(a))           // macro as substitute for println
#else
#define Sprint(a)                   
#define Sprintln(a)
#endif



#include <SPI.h>                      // My Sensors
#include <MySensors.h>

#include <FastLED.h>                    // https://github.com/FastLED/FastLED
#include "Button.h"                     // https://github.com/JChristensen/Button

const int stripPin = 6 ;                  // pin where 2812 LED strip is connected
const int buttonPin = 4 ;                 // push button
const int numPixel = 16 ;                 // set to number of pixels (x top / y bottom)

const int RGB_LightChild = 0 ;                // Child Id's, standard light child on/off/ dim
const int RGB_RGBChild = 1 ;                // RGB light child (on/off/dim/color, if controller supports V_RBG))
const int RGB_SolidColorChild = 2 ;             // when set, node reads Color text from ColorTextChild
const int RGB_TextColorChild = 3 ;              // Holds Text value for color (custom colors from controller)
const int RGB_AlarmPatternChild = 4 ;           // Switches to alarm status
const int RGB_NextPatternChild = 5 ;            // Move to next pattern when set

CRGB leds[numPixel];

// Kelving colors: Light & daylight (in Fastled reference only)
/// 1900 Kelvin Candle=0xFF9329 /* 1900 K, 255, 147, 41 */,
/// 2600 Kelvin Tungsten40W=0xFFC58F /* 2600 K, 255, 197, 143 */,
/// 2850 Kelvin Tungsten100W=0xFFD6AA /* 2850 K, 255, 214, 170 */,
/// 3200 Kelvin Halogen=0xFFF1E0 /* 3200 K, 255, 241, 224 */,
/// 5200 Kelvin CarbonArc=0xFFFAF4 /* 5200 K, 255, 250, 244 */,
/// 5400 Kelvin HighNoonSun=0xFFFFFB /* 5400 K, 255, 255, 251 */,
/// 6000 Kelvin DirectSunlight=0xFFFFFF /* 6000 K, 255, 255, 255 */,
/// 7000 Kelvin OvercastSky=0xC9E2FF /* 7000 K, 201, 226, 255 */,
/// 20000 Kelvin ClearBlueSky=0x409CFF /* 20000 K, 64, 156, 255 */

char setRGBvalue[] = "FFC58F";                // Controller sent RGB value, default tungsten40W
uint16_t curBrightness = 0x7F, setBrightness = 0x7F ;   // Brightness globals (actualBrightness)
unsigned long updateBrightnessDelay, lastBrightnessUpdate ; // Brightness timers
int RGBonoff ;                        // OnOff flag

enum { pSolid, pOff, pOn, pAlarm, pFire, pFire2, pCandle, pCircle, pSinelon, pRainbow}  ; // Pattern states (stored in int for convenience)
const int lastPatternIdx = pRainbow + 1 ;         // use last pattern for patterncount
int curPattern = pSolid ;                 // current pattern
int setPattern = pSolid ;                 // set pattern (controller)
unsigned long updatePatternDelay, lastPatternUpdate ;   // Pattern timers

unsigned long idleTimer = millis() ;            // return to idle timer
const unsigned long idleTime = 10000UL;         // return to idle after 10 secs

const unsigned long dimTime = 1000UL;           // dim period

const unsigned long heartbeatInterval = 1 * 60UL * 1000UL ; // heartbeatinterval, just to let the controller know I am alive
unsigned long heartbeatCounter = 0 ;

MyMessage lightRGBMsg(RGB_LightChild,  V_RGB);        // standard messages, light
MyMessage lightdimmerMsG(RGB_LightChild ,V_DIMMER); 
MyMessage lightOnOffMessage(RGB_LightChild, V_STATUS);

Button myBtn(buttonPin, true, true, 20);          //Declare the button (pin, pull_up, invert, debounce_ms)

// Simple state machine for button state
enum {sIdle, sBrightness, sPattern} ;            // simple state machine for button press
int State ;

void setup() {
  FastLED.addLeds<WS2812B, stripPin, GRB >(leds, numPixel);   // initialize led strip (NEOPIXEL =WS...)
  for(int i = 0 ; i < 6 ; i++) {              // get color value from EEPROM (6 char)
    setRGBvalue[i] = loadState(i) ;
    }
  setLightPattern(pSolid, 0) ;              // default controller Solid 
  FastLED.show();
  State = sIdle ;                     // Initial state
  //randomSeed(analogRead(0));
}



void presentation(){
// MySensors
  sendSketchInfo("AWI RGB Wall " NODE_TXT, "2.0");
  present(RGB_RGBChild, S_RGB_LIGHT, "RGB Wall RGB " NODE_TXT);// present to controller
  present(RGB_LightChild, S_LIGHT, "RGB Wall Light " NODE_TXT);
  present(RGB_SolidColorChild, S_LIGHT, "RGB Set Solid color (text) " NODE_TXT);
  present(RGB_TextColorChild, S_INFO, "RGB Wall textcolor " NODE_TXT);  
  present(RGB_AlarmPatternChild, S_BINARY, "RGB Wall Alarm " NODE_TXT);
  present(RGB_NextPatternChild, S_DIMMER, "RGB Wall Pattern " NODE_TXT);
}



// read button and act accordingly
// short press: on/off
// longer press: set patterns with following short press
// long press: set brightness increase 
void loop() {
  myBtn.read();                             //Read the button (only read)
  unsigned long now = millis();               // loop timer reference
  switch (State) {
    case sIdle:                     // default state, browse through patterns
      if (myBtn.wasReleased()){           // light on/ off in idle
        RGBonoff = !RGBonoff ;            // invert light state
        setLightBrightness((RGBonoff == 1)?setBrightness:0, dimTime);
        send(lightOnOffMessage.set(RGBonoff));  // and update controller  
      } else if (myBtn.pressedFor(800)){        // move to Pattern update state with long press
        idleTimer = now ;             // return to idle after ...
        State = sPattern ;
      }
      break ;
    case sPattern:                    // entered after long press
      if (myBtn.pressedFor(4000)){          // when press even longer move to Brightness update
        State = sBrightness ;
      } else if (myBtn.wasPressed()){
        setPattern = (setPattern + 1) % lastPatternIdx ;  // increase pattern and wrap
        setLightPattern((setPattern), 500 );
        idleTimer = now ;
      } else if ( now > idleTime + idleTimer  ){    // return to idle after ...
        State = sIdle ;
      }
      break ;
    case sBrightness:                 // entered after looong press
      if (myBtn.wasPressed()){              // if pressed again increase brightness
        setLightBrightness((curBrightness+0x1F) % 0xFF, 0) ; // increase brightness and wrap (0..0xFF)
        idleTimer = now ;
      } else if ( now > idleTime + idleTimer  ){    // return to idle after ...
        State = sIdle ;
      }
      break ;
    default :
      State = sIdle ;
      break ;
    }
  updateLightBrightness();                // update Brightness if time
  updateLightPattern();                 // update Pattern if time
  if ( now > heartbeatCounter  + heartbeatInterval){    // heartbeat every hour
      sendHeartbeat();
    heartbeatCounter = now ; 
    }
  }

// Sets the light brightness, takes value and time (ms) as input
void setLightBrightness(int newBrightness, unsigned long updateTime){
  // global: curBrightness, actualBrightness, updateBrightnessDelay
  updateBrightnessDelay = updateTime / 0xFF ;       // delay = time / max steps
  curBrightness = newBrightness ;             // set curBrightness to new value, rest is done in update
  } 
 
// Update the light brightness if time
void updateLightBrightness(){
  // global: curBrightness, actualBrightness, updateBrightnessDelay, lastBrightnessUpdate ;
  static byte actualBrightness ;              // store real brightness state for slow dim
  unsigned long now = millis() ;
  if (now > lastBrightnessUpdate + updateBrightnessDelay){// check if time for update
    if ( actualBrightness > curBrightness) {
      FastLED.setBrightness( actualBrightness-- );
      FastLED.show();
    } else if ( actualBrightness < curBrightness){
      FastLED.setBrightness( actualBrightness++ );
      FastLED.show();
      }
    lastBrightnessUpdate = now ;
    }
  }

// **** Pattern routines *****
// Sets and initializes the light pattern if nescessary
void setLightPattern( int newPattern, unsigned long updateDelay){
  // global: curPattern, updatePatternDelay
  static int lastPattern = pSolid ;           // last pattern for pOn / pOff virtual patterns
  if (newPattern == pOff) lastPattern = curPattern ;    // remember last pattern
  if (newPattern == pOn) curPattern = lastPattern ;   // only for pOn switch to last pattern
  else curPattern = newPattern ;
  updatePatternDelay = updateDelay ;            // delay for next pattern update, can be changed in pattern 
  switch(curPattern){
    case pSolid:                    //  solid is set value in all pixels (and on)
      for(int i = 0 ; i < numPixel ; i++) leds[i] = strtol( setRGBvalue, NULL, 16);
      setLightBrightness(setBrightness, dimTime) ;  // slow dim to on
      FastLED.show();
      break ;
    case pOn:                     //  On is set Brightness in all pixels
      setLightBrightness(setBrightness, dimTime) ;    // slow dim to on
      FastLED.show();
      break ;
    case pOff:                      //  off state all pixels off (add dim and pOn)
      setLightBrightness(0, dimTime) ;          // slow dim to off
      FastLED.show();
      break ;
    case pCircle:                   //  all pixels off
      for(int i = 0 ; i < numPixel ; i++) leds[i] = 0 ;
      for(int i = 0 ; i < 2 ; i++){
        leds[i] = strtol( setRGBvalue, NULL, 16) ; // 1 pixel on
        }
      FastLED.show();
      break ;
    default :
      setLightBrightness(setBrightness, dimTime) ;  // slow dim to on
      FastLED.show();
      break ;
      }
  } 

// Update the light pattern when time for it
void updateLightPattern(){
  // global: curPattern, updatePatternDelay, lastPatternUpdate
  unsigned long now = millis() ;
  if (now > lastPatternUpdate + updatePatternDelay){    // check if time for update
    switch (curPattern) {
      case pAlarm:                  // flash light
        patternAlarm();
        break ;
      case pFire:                   // wild fire
        patternFire();
        break ;
      case pFire2:                  // cosy fire
        patternFire2();
        break ;
      case pCandle:                 // flame
        patternCandle();
        break ;
      case pCircle:                 // flame
        patternCircle();
        break ;
      case pRainbow:                  // rotating rainbow
        patternRainbow();
        break ;
      case pSinelon:                  // rotating rainbow
        patternSinelon();
        break ;
      case pSolid:                  // do nothing fall through
      case pOff:
      case pOn:
      default :                   // def  
        break ;
      }
    lastPatternUpdate = now ;
    }
  }

// Define the different patterns
// Alarm - intermittent white and red color, full intensity, intermittent top & bottom half
void patternAlarm() {
    static boolean topBot ;                 // indicates direction for next entry
  const CRGB colorTop = CRGB(0xFF, 0, 0 );        // red color
  const CRGB colorBottom = CRGB(0xFF, 0xFF, 0xFF );   // white color
  FastLED.setBrightness(0xFF);              // set the strip brightness to max for Alarm
  for(int i=0; i <= (numPixel / 2 - 1) ; i++) {     // for half of strip size
    leds[i] = topBot?colorTop:colorBottom ; 
    leds[i+ (numPixel/2)] = topBot?colorBottom:colorTop ;
    }
  topBot = !topBot ;                    // switch direction
  FastLED.show();
  }

// Simulate fire with red color, varying number of leds intensity & tempo
void patternFire() {
    byte numberLeds = random(0,numPixel);         // start number and end of led's for flickering
    int lum = ((random(100,255) * curBrightness)) / 0xFF ;  // set brightness and scale
    CRGB color = CRGB(200, random(70,230),0 );        // get red color with varying green
    for(int i=0; i <= numberLeds; i++) {
      leds[i] = color ;
      FastLED.setBrightness(lum);             // set the strip brightness
      FastLED.show();
      wait(random(0,10));                 // (blocking, need to be changed)
    }
    updatePatternDelay = 100 ; 
}

// Simulate fire with red color and varying intensity & tempo
void patternFire2() {
    CRGB color = CRGB(200, random(100,150),0);        // get red color with varying green
    for (byte p=0; p < numPixel; p++) {
      leds[p] = color;
    }
    FastLED.setBrightness((random(50,255) * curBrightness)/ 0xFF ); // set Brightness and scale
    FastLED.show();
    updatePatternDelay = random(20,300);          // variable delay
}

// Simulate candle based on fire with red color, varying number of leds intensity & tempo
void patternCandle() {
    byte numberLeds = random(0,numPixel);         // start number and end of led's for flickering
    byte lum = ((random(100, 255) * curBrightness)/ 0xFF);  // set brightness
    CRGB color = CRGB(200, random(90,130),0 );        // get red color with varying green
    for(int i=0; i <= numberLeds; i++) {
      leds[i] = color ;
      FastLED.setBrightness(lum);             // set the strip brightness
      FastLED.show();
      wait(random(5,10));                 // (blocking, need to be changed)
    }
    updatePatternDelay = 100 ; 
}

// a colored dot sweeping back and forth, with fading trails, adapted from Fastled sinelon
void patternSinelon()
{
  fadeToBlackBy( leds, numPixel, 10);           // fade all leds a small amount 
  int pos = beatsin8(25,0,numPixel);            // get a new position for the led (BPM = 13, min, max, )
  leds[pos] += strtol( setRGBvalue, NULL, 16);
  FastLED.show();
  updatePatternDelay = 2 ;
}


// Rotate all Leds with current content and trail
void patternCircle() {
  static int currentLed ;                 // indicated current led to light
  // CRGB tempLed = leds[0];                // temporary variable for color
  fadeToBlackBy( leds, numPixel, 128);          // fade all leds for trail..
  leds[currentLed] = strtol( setRGBvalue, NULL, 16);    // set to current color
  currentLed = (currentLed + 1) % numPixel ;        // wrap
    FastLED.show();
    updatePatternDelay = 100 ; 
}

void patternRainbow() {
  static uint16_t hue ;                 // starting color
  FastLED.clear();
  // for(hue=10; hue<255*3; hue++) {
  hue = (hue+1) % 0xFF ;                  // incerease hue and wrap
  fill_rainbow( leds, numPixel , hue /*static hue value */, 1);// set a rainbow from hue to last in stepsize 1
  FastLED.show();
  updatePatternDelay = 100 ;
  }

// Incoming messages from MySensors
void receive(const MyMessage &message) {
  int ID = message.sensor;
  Serial.print("Sensor: ");
  Serial.println(ID);
  switch (ID){
    case RGB_LightChild:                // same behaviour as RGB child/ fall through
    case RGB_RGBChild:                  // if controller can handle V_RGB
      if (message.type == V_RGB) {          // check for RGB type
        strcpy(setRGBvalue, message.getString()); // get the payload
        setLightPattern(pSolid, 0);         // and set solid pattern 
      } else if (message.type == V_DIMMER) {      // if DIMMER type, adjust brightness
        setBrightness = map(message.getInt(), 0, 100, 0, 255);
        setLightBrightness(setBrightness, dimTime) ;
      } else if (message.type == V_STATUS) {      // if on/off type, toggle brightness
        RGBonoff = message.getInt();
        setLightBrightness((RGBonoff == 1)?setBrightness:0, dimTime);
      }
      break ;
    case RGB_SolidColorChild:             // request color from controller
      if (message.type == V_STATUS) {         // if get color from text child
        request(RGB_TextColorChild, V_TEXT);
        setLightPattern(pSolid, 0);         // and set solid pattern (if not alre)
        }
      break ;
    case RGB_TextColorChild:              // Text color from controller
      if (message.type == V_TEXT) {         // if get color from text child
        strcpy(setRGBvalue, message.getString()); // get the payload
        for(int i = 0 ; i < 6 ; i++) {        // save color value to EEPROM (6 char)
          saveState(i, setRGBvalue[i]) ;}     // Save to EEPROM
        }
      break ;
    case RGB_AlarmPatternChild:             // set Alarm pattern
      if (message.type == V_STATUS) {         // if get color from text child
        if (message.getInt() == 1){
          setLightPattern(pAlarm, 500);     // set slow alarm pattern
        } else {
          setLightPattern(setPattern, 0);     // and reset pattern
          FastLED.setBrightness(setBrightness);
          }
        }
      break ;
    case RGB_NextPatternChild:              // next pattern
      if (message.type == V_PERCENTAGE) {       //  Percentage indicates the pattern
        setPattern = map(message.getInt(), 0, 100, 0, 15) % lastPatternIdx  ; // mapper dimmer value to state 0..9  and wrap
        setLightPattern((setPattern), 500 );
        Sprint("Pattern: ") ; Sprintln(setPattern) ;
      } else if (message.type == V_STATUS){     // if off switch pattern to default == 0
        setPattern = 0  ;
        setLightPattern((setPattern), 500 );
        Sprint("Pattern: ") ; Sprintln(setPattern) ;
      }
      break ;
    }
    FastLED.show();
  dispRGBstat();
  }
// debug  
// display the status of all RGB: controller, requested, real
void dispRGBstat(void){
    Serial.print(" Color: "); Serial.print(setRGBvalue); 
    Serial.print(" Brightness: "); Serial.println(setBrightness);
  }

