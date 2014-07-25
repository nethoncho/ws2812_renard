#include "FastLED.h"

/*
** Parts from sparkfun.com and adafruit.com
**
** RedBoard https://www.sparkfun.com/products/11575
** Joystick Shield Kit https://www.sparkfun.com/products/9760
** Slide Pot https://www.sparkfun.com/products/11620
** SPDT Slide Switch https://www.sparkfun.com/products/9609
** USB-B Mini https://www.sparkfun.com/products/11301
** Addressable RGB LED Strip https://www.sparkfun.com/products/12025
** Power Adapter http://www.adafruit.com/product/368
** 5V 4A Power Supply http://www.adafruit.com/product/1466
** 2 Pin JST Female http://www.adafruit.com/product/318
** 2 Pin JST Male http://www.adafruit.com/product/319
**
** The slide pot goes to A2 (joystick on A0 and A1)
** The toggle switch goes on D7
**
** Light Sequencer http://www.vixenlights.com/
** Using Renard protocol with serial rate of 115200
**
** This will work with out the joystick shield
**
*/


// IO Pins
#define JSTICK_X A0
#define JSTICK_Y A1
#define JSTICK_BTN 2
#define DIMMER A2
#define BUTTON_UP 3
#define BUTTON_RIGHT 4
#define BUTTON_DOWN 5
#define BUTTON_LEFT 6
#define TOGGLE_SWITCH 7

// For led chips like Neopixels, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806, define both DATA_PIN and CLOCK_PIN
#define DATA_PIN 11
#define CLOCK_PIN 13

//
#define MASTER_TIMEOUT 2000

// Renard protocol constants
#define RENARD_COMMAND_PAD 0x7D
#define RENARD_COMMAND_START_PACKET 0x7E
#define RENARD_COMMAND_ESCAPE 0x7F
#define RENARD_ESCAPE_PAD 0x2F
#define RENARD_ESCAPE_START_PACKET 0x30
#define RENARD_ESCAPE_ESCAPE 0x31
#define RENARD_CHANNELS_IN_BANK 8

typedef enum RENARD_PROTOCOL_STATES{
    RENARD_PROTOCOL_NOSYNC,
    RENARD_PROTOCOL_START_PACKET,
    RENARD_PROTOCOL_DIMMER_VALUE,
    RENARD_PROTOCOL_DIMMER_UPDATE
};

// Joystick Offset
#define CENTER_X 512
#define CENTER_Y 482

// How many leds are in the strip?
#define NUM_LEDS 60

// Serial Setting
#define SERIAL_SPEED 115200
#define SERIAL_TIMEOUT 20

// Globals
long Stick_X_Raw = 0;
long Stick_Y_Raw = 0;
float Radius = 0.0;
float Angle = 0.0;
unsigned char Hue = 0;
unsigned char Sat = 0;
int dimmerValue = 255;
unsigned long LastSerialEventTime;
unsigned char oneByteBuffer;
unsigned long RenardDimmerOffset;
RENARD_PROTOCOL_STATES RenardProtocolState;

// This is an array of leds.  One item for each led in your strip.
CRGB leds[NUM_LEDS];

void setup() {
    LastSerialEventTime = millis() + MASTER_TIMEOUT;
    pinMode(JSTICK_BTN, INPUT);
    digitalWrite(JSTICK_BTN, HIGH);
    pinMode(BUTTON_UP, INPUT);
    digitalWrite(BUTTON_UP, HIGH);
    pinMode(BUTTON_RIGHT, INPUT);
    digitalWrite(BUTTON_RIGHT, HIGH);
    pinMode(BUTTON_DOWN, INPUT);
    digitalWrite(BUTTON_DOWN, HIGH);
    pinMode(BUTTON_LEFT, INPUT);
    digitalWrite(BUTTON_LEFT, HIGH);
    pinMode(TOGGLE_SWITCH, INPUT);
    digitalWrite(TOGGLE_SWITCH, HIGH);
    
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

    Serial.begin(SERIAL_SPEED);
    Serial.setTimeout(SERIAL_TIMEOUT);

    powerOnSelfTest();

    RenardProtocolState = RENARD_PROTOCOL_NOSYNC;
    RenardDimmerOffset = 0;
}

void loop()
{
    if((millis() - LastSerialEventTime) > MASTER_TIMEOUT) {
        LocalShowLoop();
    }
}

void serialEvent()
{
    LastSerialEventTime = millis();
    LEDS.setBrightness(255);
    while(Serial.available()) {
        oneByteBuffer = Serial.read();

        if(oneByteBuffer == RENARD_COMMAND_PAD) {
            continue;
        }
        if(oneByteBuffer == RENARD_COMMAND_START_PACKET) {
            RenardProtocolState = RENARD_PROTOCOL_START_PACKET;
            continue;
        }

        switch(RenardProtocolState)
        {
            case RENARD_PROTOCOL_NOSYNC:
                break;
            case RENARD_PROTOCOL_DIMMER_UPDATE:
                RenardProtocolState = RENARD_PROTOCOL_NOSYNC;
                FastLED.show();
                continue;
                break;
            case RENARD_PROTOCOL_START_PACKET:
                if(oneByteBuffer < 0x80) {
                    // Protocol Error
                    RenardDimmerOffset = 0;
                    RenardProtocolState = RENARD_PROTOCOL_NOSYNC;
                    continue;
                }
                if(oneByteBuffer == 0x80) {
                    RenardDimmerOffset = 0;
                    fill_solid(leds, NUM_LEDS, CHSV( 0, 0, 0));
                        RenardProtocolState = RENARD_PROTOCOL_DIMMER_VALUE;
                    continue;
                }
                if(oneByteBuffer > 0x80) {
                    // Leave RenardDimmerOffset alone
//                    RenardDimmerOffset = ((int)(oneByteBuffer)) * RENARD_CHANNELS_IN_BANK;
                    RenardProtocolState = RENARD_PROTOCOL_DIMMER_VALUE;
                    continue;
                }
                break;
            case RENARD_PROTOCOL_DIMMER_VALUE:
                if(oneByteBuffer == RENARD_COMMAND_ESCAPE) {
                    Serial.readBytes((char *)oneByteBuffer, 1); // Use readBytes because it is synchronous
                    // The value of oneByteBuffer should be changed based on the escape value
                    // Setting to 0x7f as it is close to any possible value
                    oneByteBuffer = 0x7F;
                }
                if(RenardDimmerOffset >= NUM_LEDS * 3) {
                    RenardDimmerOffset = 0;
                    RenardProtocolState = RENARD_PROTOCOL_DIMMER_UPDATE;
                } else {
                    ((char *)(leds))[RenardDimmerOffset] = oneByteBuffer;
                    RenardDimmerOffset++;
                }
                break;
        }
    }
}

void powerOnSelfTest()
{
    leds[0] = CRGB::Red;
    leds[1] = CRGB::Green;
    leds[2] = CRGB::Green;
    leds[3] = CRGB::Blue;
    leds[4] = CRGB::Blue;
    leds[5] = CRGB::Blue;
    FastLED.show();
    delay(10);
    fill_solid(leds, NUM_LEDS, CHSV( 0, 0, 0));
    FastLED.show();
}

void LocalShowLoop()
{
    CRGB MyRGB[1];
    if(digitalRead(TOGGLE_SWITCH) == LOW) {
        dimmerValue = map(analogRead(DIMMER), 0, 1023, 0, 255);
        Stick_X_Raw = analogRead(JSTICK_X) - CENTER_X;
        Stick_Y_Raw = analogRead(JSTICK_Y) - CENTER_Y;
        Radius = (sqrt(sq(Stick_X_Raw)+ sq(Stick_Y_Raw)) / 2.0);
        Angle = atan2((float)Stick_Y_Raw, (float)Stick_X_Raw) * RAD_TO_DEG;
        if(Angle < 0.0) {
            Angle = 360.0 - (Angle * -1.0);
        }
        Hue = (unsigned char)(Angle * (255.0 / 360.0));
        if(Radius >= 255.0) {
            Sat = 255;
        } else {
            Sat = Radius;
        }
        MyHSVtoRGB(Angle, Sat, 255, MyRGB[0]);
        fill_solid(leds, NUM_LEDS, MyRGB[0]);
        LEDS.setBrightness(dimmerValue);
        FastLED.show();
    }
}

void MyHSVtoRGB(uint16_t h, uint8_t s, uint8_t v, struct CRGB & rgb)
{
    uint8_t base;
    if(s == 0) {
        rgb.r = v;
        rgb.g = v;
        rgb.b = v;
        return;
    }
    base = ((255 - s) * v) >> 8;
    switch(h/60) {
        case 0:
            rgb.r = v;
            rgb.g = (((v - base) * h) / 60) + base;
            rgb.b = base;
            break;
        case 1:
            rgb.r = (((v - base) * (60 - (h % 60))) / 60) + base;
            rgb.g = v;
            rgb.b = base;
            break;
        case 2:
            rgb.r = base;
            rgb.g = v;
            rgb.b = (((v - base) * (h % 60)) / 60) + base;
            break;
        case 3:
            rgb.r = base;
            rgb.g = (((v - base) * (60 - (h % 60))) / 60) + base;
            rgb.b = v;
            break;
        case 4:
            rgb.r = (((v - base) * (h % 60)) / 60) + base;
            rgb.g = base;
            rgb.b = v;
            break;
        default:
            rgb.r = v;
            rgb.g = base;
            rgb.b = (((v - base) * (60 - (h % 60))) / 60) + base;
            break;
    }
}

