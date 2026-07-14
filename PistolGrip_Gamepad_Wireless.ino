/*
Pistol Grip Gamepad
By: Colby R.
Date: 10/03/2025
	V6: Added ADC for 5V rail to better manage when battery is powering the controller
	V5: fixed throttle max high side, added printouts
	    Also added persistant values with trim adjustment behind an added menu mode (hold menu/set buttons)
	    Added saving VibratorEnabled to memory/menu
	V4: Added Battery level reporting
	V3: redesigned for ESP32-S3 modules
	V2: designed for TQi 2 channel contrller (the black 2.4GHz ones)
	V1: designed and working for my old traxxas controller (the grey ones)
*/

/*
Notes for possible revisions
	- Add ADC sensing for gamepad USB (to maybe disconnect BLE?)
*/

#include <BleGamepad.h>
#include <Joystick_ESP32S2.h>
#include <Preferences.h>

#define VERSION_MAJOR 5

// +--------------------------------------------------------------+
// |                       Connection Notes                       |
// +--------------------------------------------------------------+

// +==============================+
// |         TQi Pinouts          |
// +==============================+
/*
	### Wires ###
	Red    -  +3.3V   
	Green  -  Throttle
	Black  -  Ground  
	Blue   -  Ground  
	Orange -  +3.3V   
	White  -  Steering

	### PCB ###
    & Top &
	P1_1 - Ground     - GND
	P1_2 - Switch1    - SET
	P1_3 - Switch2    - MENU
	P1_4 - DS2_Right  - LED_RED   (53Ω ~60mA needed!)
	P1_5 - DS2_Center - LED_GREEN (53Ω ~60mA needed!)
	P1_6 - R2_Center  - STR_TRIM
	P1_7 - R1_Center  - MULTI_TRIM
	P1_8 - VDD        - 3.3V
    & Bottom &
*/

// +==============================+
// |        XINPUT Buttons        |
// +==============================+
//      Ble           #   Game-Default     Ble     XINPUT (off by one?)
#define BUTTON_LOGO   0
#define BUTTON_A      1
#define BUTTON_B      2
#define BUTTON_X      3
#define BUTTON_Y      4
#define BUTTON_LB     5
#define BUTTON_RB     6
#define BUTTON_BACK   7
#define BUTTON_START  8
#define BUTTON_L3     9
#define BUTTON_R3     10
#define DPAD_UP       11
#define DPAD_DOWN     12
#define DPAD_LEFT     13
#define DPAD_RIGHT    14
// #define TRIGGER_LEFT  15
// #define TRIGGER_RIGHT 16
// #define JOY_LEFT      17
// #define JOY_RIGHT     18

// +--------------------------------------------------------------+
// |                     Compile Time Options                     |
// +--------------------------------------------------------------+
#define NEG_AXIS            false // set axis range -32767 to 32767 (Some non-Windows operating systems and web based gamepad testers don't like min axis set below 0, so 0 is set by default)
#define SIM_CONTROLS        false // enable and map throttle/steering to "sim" controls
#define PRINTOUTS           true
#define WRTIE_REGISTRATION  false // write controller ID and other info into non-volitale memory (only need once to "register" it)
#define VIB_DEFAULT         false // vibrator default to on/off (also saved to persistance memory!)

// BLE_Gamepad_Config
#define NUM_BUTTONS      8 // do be able to natrually map BUTTON_BACK in Re-Volt. we only have 6 actual
#define NUM_HAT_SWITCHES 0
#define ENABLE_X true
#define ENABLE_Y true
#define ENABLE_Z false
#define ENABLE_RX true
#define ENABLE_RY true
#define ENABLE_RZ false
#define ENABLE_SLIDER1 false
#define ENABLE_SLIDER2 false
#if SIM_CONTROLS
	#define ENABLE_RUDDER false
	#define ENABLE_THROTTLE true
	#define ENABLE_ACCELERATOR false
	#define ENABLE_BRAKE true
	#define ENABLE_STEERING true
#else
	#define ENABLE_RUDDER false
	#define ENABLE_THROTTLE false
	#define ENABLE_ACCELERATOR false
	#define ENABLE_BRAKE false
	#define ENABLE_STEERING false
#endif

// +--------------------------------------------------------------+
// |                       Mapping Defines                        |
// +--------------------------------------------------------------+
// button number through the library to end device changes depending on some things...
// +==============================+
// |     Default GAME Mapping     |
// +==============================+
//         Action        Windows  Android#  Android      Ble
// #define Fire          Button_0     1     Button_A      0 
// #define Flip_Car      Button_1     2     Button_B      0 
// #define Reposition    Button_2     3     Button_X      0 
// #define Horn          Button_5     8?    Button_Guide  0 
// #define Rear_View     Button_4     7     Button_Back   0 
// #define Change_Camera F1                 F1            0 
// #define Pause         Button_3     4     Button_Y      0 

// +==============================+
// |        Game BLE Mapping      |
// +==============================+
#define BLE_GAME_ACCEPT_ITEM 1
#define BLE_GAME_FLIP_CAR    2
#define BLE_GAME_RESET_CAR   3
#define BLE_GAME_BACK_PAUSE  4
#define BLE_GAME_LOOK_BACK   5
#define BLE_GAME_HORN        6
#define BLE_GAME_UP          10 // verify
#define BLE_GAME_DOWN        11 // verify
#define BLE_GAME_LEFT        12 // verify
#define BLE_GAME_RIGHT       13 // verify
#define BLE_GAME_FWDREV      JOY_LEFT
#define BLE_GAME_STEER       JOY_LEFT

// +==============================+
// |      Game Android Mapping    | // 7 Also back/pause?
// +==============================+ //0,1,2,3,4,5,6,7,8 aren't the ones we're looking for
#define AND_GAME_ACCEPT_ITEM 1 // Button A
#define AND_GAME_FLIP_CAR    2 // Button B
#define AND_GAME_RESET_CAR   4 // Button X
#define AND_GAME_BACK_PAUSE  5 // Button Y
#define AND_GAME_LOOK_BACK   6
#define AND_GAME_HORN        7
#define AND_GAME_UP          10 // verify
#define AND_GAME_DOWN        11 // verify
#define AND_GAME_LEFT        12 // verify
#define AND_GAME_RIGHT       13 // verify
#define AND_GAME_FWDREV      JOY_LEFT
#define AND_GAME_STEER       JOY_LEFT


// +==============================+
// |       Game USB Mapping       |
// +==============================+
#define USB_GAME_ACCEPT_ITEM 0
#define USB_GAME_FLIP_CAR    1
#define USB_GAME_RESET_CAR   2
#define USB_GAME_BACK_PAUSE  3
#define USB_GAME_LOOK_BACK   4
#define USB_GAME_HORN        5
#define USB_GAME_UP          10 // verify
#define USB_GAME_DOWN        11 // verify
#define USB_GAME_LEFT        12 // verify
#define USB_GAME_RIGHT       13 // verify
#define USB_GAME_FWDREV      JOY_LEFT
#define USB_GAME_STEER       JOY_LEFT

// +==============================+
// |            PINOUT            |
// +==============================+ // ESP32-S3
#define PIN_STR_TRM   (4)  // GPIO4  // ADC1_3
#define PIN_THT_TRM   (5)  // GPIO5  // ADC1_4
#define PIN_STR       (6)  // GPIO6  // ADC1_5
#define PIN_THT       (7)  // GPIO7  // ADC1_6
#define PIN_V5DC_ADC  (9)  // GPIO9  // ADC1_8
#define PIN_BATT_ADC  (10) // GPIO10 // ADC1_9
#define PIN_THMB_BTN  (38) // GPI38  // (38)
#define PIN_MENU_BTN  (2)  // GPIO2  // (0)
#define PIN_SET_BTN   (1)  // GPIO1  // (1)
#define PIN_LED_RED   (42) // GPIO42 // (42) NOTE: PWM!
#define PIN_LED_GREEN (41) // GPIO41 // (41) NOTE: PWM!
#define PIN_TOP_BTN   (16) // GPIO16 // (16)
#define PIN_MID_BTN   (17) // GPIO17 // (17)
#define PIN_BTM_BTN   (18) // GPIO18 // (18)
#define PIN_VIBRATOR  (39) // GPIO39 // (39)

// +==============================+
// |        Button Mapping        |
// +==============================+
#define USB_THMB_BTN  USB_GAME_ACCEPT_ITEM
#define USB_MENU_BTN  USB_GAME_BACK_PAUSE
#define USB_SET_BTN   USB_GAME_RESET_CAR
#define USB_TOP_BTN   USB_GAME_LOOK_BACK
#define USB_MID_BTN   USB_GAME_HORN
#define USB_BTM_BTN   USB_GAME_FLIP_CAR

#define BLE_THMB_BTN  BLE_GAME_ACCEPT_ITEM
#define BLE_MENU_BTN  BLE_GAME_BACK_PAUSE
#define BLE_SET_BTN   BLE_GAME_RESET_CAR
#define BLE_TOP_BTN   BLE_GAME_LOOK_BACK
#define BLE_MID_BTN   BLE_GAME_HORN
#define BLE_BTM_BTN   BLE_GAME_FLIP_CAR

#define AND_THMB_BTN  AND_GAME_ACCEPT_ITEM
#define AND_MENU_BTN  AND_GAME_BACK_PAUSE
#define AND_SET_BTN   AND_GAME_RESET_CAR
#define AND_TOP_BTN   AND_GAME_LOOK_BACK
#define AND_MID_BTN   AND_GAME_HORN
#define AND_BTM_BTN   AND_GAME_FLIP_CAR

// +--------------------------------------------------------------+
// |                        Controller IDs                        |
// +--------------------------------------------------------------+
#define MANF_NAME "RSP" // "ReadySetProjects"
#define VENDOR_ID 0xC01B // Colby!

// These are where you manually set the PID and name for your controller

// // TEST ESP32-C3
// #define PROD_NAME  "TEST-C3 Re-Vamp" 0x1234
// #define CONTROLLER_ID 0x1234

// // A0148840 ****************************** now full controller (First ESP32-S3 controller)
// #define PROD_NAME  "A0148840 Re-Vamp"
// #define CONTROLLER_ID 0x8840

// // A0392285 ****************************** now full controller (Has throttle switch thing)
// #define PROD_NAME  "A0392285 Re-Vamp"
// #define CONTROLLER_ID 0x2285

// // A0381549 ****************************** now full controller
// #define PROD_NAME  "A0381549 Re-Vamp"
// #define CONTROLLER_ID 0x1549

// // A0381459 ****************************** now full controller (First S3 with nose buttons)
// #define PROD_NAME  "A0381459 Re-Vamp"
// #define CONTROLLER_ID 0x1459

// // A0359313 ****************************** now full controller
// #define PROD_NAME  "A0359313 Re-Vamp"
// #define CONTROLLER_ID 0x9313

// // A0359295 ****************************** now full controller
// #define PROD_NAME  "A0359295 Re-Vamp"
// #define CONTROLLER_ID 0x9295

// // A0306966 ****************************** now full controller
// #define PROD_NAME  "A0306966 Re-Vamp"
// #define CONTROLLER_ID 0x6966

// // A0306712 ****************************** now full controller (Could maybe us a little mor buffer on throttle side? typical pulls reach 0.97 rather than 0.99)
// #define PROD_NAME  "A0306712 Re-Vamp"
// #define CONTROLLER_ID 0x6712

// A0148987 ****************************** now full controller
#define PROD_NAME  "A0148987 Re-Vamp"
#define CONTROLLER_ID 0x8987

// // A0148750 ****************************** messed up POT (now just test plastics)
// #define PROD_NAME  "A0148750 Re-Vamp"
// #define CONTROLLER_ID 0x8750

// // A0329340 ****************************** now full controller (First S3 Controller POC)
// #define PROD_NAME  "A0329340 Re-Vamp"
// #define CONTROLLER_ID 0x9340 

// // A0148860 ****************************** now full controller (First PCB S3 controller)
// #define PROD_NAME  "A0148860 Re-Vamp"
// #define CONTROLLER_ID 0x8860

// // A0350786 ****************************** now full controller
// #define PROD_NAME  "A0350786 Re-Vamp"
// #define CONTROLLER_ID 0x0786

// // A0243249 ****************************** now full controller
// #define PROD_NAME  "A0243249 Re-Vamp"
// #define CONTROLLER_ID 0x3249

// // A0337517 ****************************** now full controller (Needed larger STARTING_BRAKE of 700 to get to -0.99)
// #define PROD_NAME  "A0337517 Re-Vamp"
// #define CONTROLLER_ID 0x7517

// // A0337117 ****************************** now full controller
// #define PROD_NAME  "A0337117 Re-Vamp"
// #define CONTROLLER_ID 0x7117

// // A0350075  ****************************** S3 POC turned full controller
// #define PROD_NAME  "A0350075 Re-Vamp"
// #define CONTROLLER_ID 0x0075

#ifndef PROD_NAME
#define PROD_NAME "Re-Vamp Gamepad"
#endif

#ifndef CONTROLLER_ID
#define CONTROLLER_ID 0x1234
#endif

// +--------------------------------------------------------------+
// |                          Constants                           |
// +--------------------------------------------------------------+
#define ADC_MAX          0xFFF // For ESP32-C3 12-Bit (4096 values, ie. 0-4095)
#define ADC_HALF         ((ADC_MAX+1)/2)
#define ADC_MAX_V        3.1 // max voltage ADC can reada
#define ADC_FILT_LEN     5 // Rolling average length
#define XINPUT_MAX       0xFFFF // 65536 i.e. 16 bit resolution
#define XINPUT_MIN       -65536 // 16 bit resolution i.e. 0x10000
#define CONV_MULTI       (XINPUT_MAX/(ADC_MAX+1)) // Conversion multiplier from ADC to XINPUT resolutions
#define STARTING_LIMITS  100 // every potentiometer is different so we'll ring the values in a bit to start
#define STARTING_BRAKE   620 // every potentiometer is different so we'll ring the values in a bit to start
#define SET_FLASH_PERIOD 250 // ms how fast to flash when in settings mode (change saved trim pos)
#define MENU_WAIT        3000 // ms how long to hold down MENU & SET buttons to enter menu

// +==============================+
// |       Battery Defines        |
// +==============================+ // ADC before the protection Diode D2
#define BATT_FILT_LEN      10 // Rolling average length
#define BATT_ADC_OFFSET    ((ADC_MAX / ADC_MAX_V) * 0.2) // where is this coming from? (Apply pre-upscaling) (in volts)
#define BATT_RHIGH         221000 // Reistor in voltage devider network
#define BATT_RLOW          100000 // Reistor in voltage devider network
#define BATT_CONV_ADJ      0.000  // need to adjust the conversion? (actually not needed after adjusting pre conversion value?)
#define BATT_CONVERSION    (((BATT_RHIGH + BATT_RLOW * 1.0) / BATT_RLOW) + BATT_CONV_ADJ) // multiplier to conver to voltagealc
#define BATT_MAX_V         (ADC_MAX_V * BATT_CONVERSION) // actual voltage the ADC can read (should be ~9.951)
#define BATT_LOW           4.484 // Below this is when battery voltage starts to affect 3.3v regulated power. read before the protection diode
#define BATT_NO_NOISE      4.650 // Below this is when power rail sag from transmits starts to become noticable
#define BATT_NO_V          3.600 // Above this there is battery voltage present (when positive disconnected input floats to ~3.3V)
#define BATT_FULL          6.600 // Batteries full (100% battery) (Alkaline. Lithium will be higher)
#define BATT_D_HYST        0.150 // Diode drop is 0.3V. use half that as a hysteresis for "on battery" 
#define BATT_REP_DELTA     ((BATT_FULL - BATT_NO_NOISE) * 0.02) // 2% how much change in level before sending new value
#define BATT_UPDATE_PERIOD 10000 // ms
#define BATT_FLASH_PERIOD  500 // ms

// +==============================+
// |          5V Defines          |
// +==============================+ // ADC on the "5V" rail going into the LDO (can get power from either of the USB or the battery)
#define V5DC_FILT_LEN   10 // Rolling average length
#define V5DC_ADC_OFFSET ((ADC_MAX / ADC_MAX_V) * 0.2) // where is this coming from? (Apply pre-upscaling) (in volts)
#define V5DC_RHIGH      221000 // Reistor in voltage devider network
#define V5DC_RLOW       100000 // Reistor in voltage devider network
#define V5DC_CONV_ADJ   0.000  // need to adjust the conversion? (actually not needed after adjusting pre conversion value?)
#define V5DC_CONVERSION (((V5DC_RHIGH + V5DC_RLOW * 1.0) / V5DC_RLOW) + V5DC_CONV_ADJ) // multiplier to conver to voltagealc
#define V5DC_MAX_V      (ADC_MAX_V * V5DC_CONVERSION) // actual voltage the ADC can read (should be ~9.951)
#define V5DC_LOW        4.263 // this is when battery voltage starts to affect 3.3v regulated power. Read at regulator input
#define V5DC_NO_NOISE   4.404 // Below this is when power rail sag from transmits starts to become noticable

#define PWM_LED_MAX 255
#define PWM_LED_MIN 0 // LEDs don't turn on till this?
#define PWM_CONV_MULTI (PWM_LED_MAX - PWM_LED_MIN) // Conversion multiplier from ADC to XINPUT resolutions

#define VIB_DURATION 50 // ms
#define PRINT_PERIOD 1000 // ms

#define TRIM_PERCENT 0.30 //% amount of range trim can adjust center point
#define TRIM_MIN 10 // trim pots get crazy around the edges
#define TRIM_MAX (ADC_MAX - TRIM_MIN)

#if NEG_AXIS
	#define SIM_MIN     0x8000 // -32767
	#define AXIS_CENTER 0x0000      
	#define SIM_MAX     0x7FFF // 32767 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
	#define AXIS_MIN    0x8000 // -32767
	#define AXIS_CENTER 0x0000      
	#define AXIS_MAX    0x7FFF
#else
	#define SIM_MIN     0x0000
	#define SIM_CENTER  0x3FFF
	#define SIM_MAX     0x7FFF // 32767 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
	#define AXIS_MIN    0x0000
	#define AXIS_CENTER 0x3FFF
	#define AXIS_MAX    0x7FFF
#endif

// +--------------------------------------------------------------+
// |                           Globals                            |
// +--------------------------------------------------------------+

BleGamepad bleGamepad(PROD_NAME, MANF_NAME, 100);
Joystick_  usbGamepad(JOYSTICK_DEFAULT_REPORT_ID,JOYSTICK_TYPE_GAMEPAD,
					  NUM_BUTTONS, NUM_HAT_SWITCHES,                      // Button Count, Hat Switch Count
					  ENABLE_X, ENABLE_Y, ENABLE_Z,                       // X and Y, but no Z Axis
					  ENABLE_RX, ENABLE_RY, ENABLE_RZ,                    // Rx and Ry, but no Rz Axis
					  ENABLE_RUDDER, ENABLE_THROTTLE,                     // No rudder or throttle
					  ENABLE_ACCELERATOR, ENABLE_BRAKE, ENABLE_STEERING); // No accelerator, brake, or steering
hw_timer_t *Timer0_Cfg = NULL;

int PrintCountdown = 0;
int LedFlashCountdown = 0;

int MenuCount = 0;

int  VibCountdown = 0;
bool VibActivated = false; // Is the vibrator active?
bool AndroidMode = false; // but press startup to have different button map? (wasn't really working out for some reason)
bool MenuMode = false; // trim pots actually adjust trim
bool VibEnabled = VIB_DEFAULT; // activate vibrator when THUMB button pressed
bool LedFlashState = false; // when flashing are the LEDs on or off?
bool MenuEntrance = false; // count up to enter menu

int ThrottleMin = STARTING_LIMITS;
int ThrottleMax = ADC_MAX - STARTING_BRAKE;
int SteeringMin = STARTING_LIMITS;
int SteeringMax = ADC_MAX - STARTING_LIMITS;
int ThrottleTrimMin = STARTING_LIMITS;
int ThrottleTrimMax = ADC_MAX - STARTING_LIMITS;
int SteeringTrimMin = STARTING_LIMITS;
int SteeringTrimMax = ADC_MAX - STARTING_LIMITS;

String ProductName = PROD_NAME;
int16_t ControllerID = CONTROLLER_ID;
int ThrottleTrimSaved = ADC_HALF;
int SteeringTrimSaved = ADC_HALF;
Preferences prefs;

int  LastThrottleTrim = 0;
int  LastSteeringTrim = 0;
int  LastSteering     = 0;
int  LastThrottle     = 0;

bool LastThumbBtn     = false;
bool LastMenuBtn      = false;
bool LastSetBtn       = false;
bool LastTopBtn       = false;
bool LastMidBtn       = false;
bool LastBtmBtn       = false;

uint AdcBufferThrottle[ADC_FILT_LEN] = {0};
uint AdcBufferSteering[ADC_FILT_LEN] = {0};
uint AdcBufferSteeringTrim[ADC_FILT_LEN] = {0};
uint AdcBufferThrottleTrim[ADC_FILT_LEN] = {0};

float LastV5dc = 0.0;
float LastSentBattery = 0.0;
uint  AdcBufferBattery[BATT_FILT_LEN] = {0};
uint  AdcBufferPwrV5dc[V5DC_FILT_LEN] = {0};
int   AdcUpdateCountdown = 0;
bool  PowerFromUSB = false; // mainly used to stop constant battery updates when on USB power
bool  BatteryLowIndication = false; // mainly used to give the LED flashing mode some hysteresis

// +--------------------------------------------------------------+
// |                         Timer 0 ISR                          |
// +--------------------------------------------------------------+
void IRAM_ATTR Timer0_ISR()
{
	if(VibCountdown      > 0) { VibCountdown --; }
	if(PrintCountdown    > 0) { PrintCountdown --; }
	if(LedFlashCountdown > 0) { LedFlashCountdown --; }
	if(AdcUpdateCountdown > 0) { AdcUpdateCountdown --; }

	if(MenuEntrance) { MenuCount ++; } else { MenuCount = 0; }
}

// +--------------------------------------------------------------+
// |                             Init                             |
// +--------------------------------------------------------------+
void setup()
{

	// +==============================+
	// |            Timers            |
	// +==============================+
	Timer0_Cfg = timerBegin(10000); // 10KHz
    timerAttachInterrupt(Timer0_Cfg, &Timer0_ISR);
    timerAlarm(Timer0_Cfg, 10, true, 0); // fire ISR every 1ms (or every 10 timer ticks), repeat, unlimited times

	// +==============================+
	// |            Micro             |
	// +==============================+
	pinMode(PIN_STR,       INPUT); // No Pullup for ADC?
	pinMode(PIN_STR_TRM,   INPUT); // No Pullup for ADC?
	pinMode(PIN_THT,       INPUT); // No Pullup for ADC?
	pinMode(PIN_THT_TRM,   INPUT); // No Pullup for ADC?
	pinMode(PIN_V5DC_ADC,  INPUT); // No Pullup for ADC?
	pinMode(PIN_BATT_ADC,  INPUT); // No Pullup for ADC?
	pinMode(PIN_THMB_BTN,  INPUT_PULLUP);
	pinMode(PIN_MENU_BTN,  INPUT_PULLUP);
	pinMode(PIN_SET_BTN,   INPUT_PULLUP);
	pinMode(PIN_LED_RED,   OUTPUT);
	pinMode(PIN_LED_GREEN, OUTPUT);
	pinMode(PIN_TOP_BTN,   INPUT_PULLUP);
	pinMode(PIN_MID_BTN,   INPUT_PULLUP);
	pinMode(PIN_BTM_BTN,   INPUT_PULLUP);
	pinMode(PIN_VIBRATOR,  OUTPUT);

	digitalWrite(PIN_VIBRATOR, HIGH); // turn off vibrator

	// +==============================+
	// |         Init Values          |
	// +==============================+
	LastThrottleTrim = analogRead(PIN_THT_TRM);
	LastSteeringTrim = analogRead(PIN_STR_TRM);
	LastThrottle     = analogRead(PIN_THT);
	LastSteering     = analogRead(PIN_STR);
	LastV5dc         = GetVoltageFromAdc(analogRead(PIN_V5DC_ADC) + V5DC_ADC_OFFSET);
	LastSentBattery  = GetVoltageFromAdc(analogRead(PIN_BATT_ADC) + BATT_ADC_OFFSET);
	LastThumbBtn     = !digitalRead(PIN_THMB_BTN);
	LastMenuBtn      = !digitalRead(PIN_MENU_BTN);
	LastSetBtn       = !digitalRead(PIN_SET_BTN);
	LastTopBtn       = !digitalRead(PIN_TOP_BTN);
	LastMidBtn       = !digitalRead(PIN_MID_BTN);
	LastBtmBtn       = !digitalRead(PIN_BTM_BTN);

	// +==============================+
	// |      Persistant Values       |
	// +==============================+
	prefs.begin("revamp_prefs", false);
	#if WRTIE_REGISTRATION
	prefs.putString("ProductName", PROD_NAME);
	prefs.putInt("ControllerID", CONTROLLER_ID);
	#endif
	ProductName  = prefs.getString("ProductName", PROD_NAME);
	ControllerID = prefs.getInt("ControllerID", CONTROLLER_ID);
	ThrottleTrimSaved = prefs.getInt("ThrottleTrim", ADC_HALF);
	SteeringTrimSaved = prefs.getInt("SteeringTrim", ADC_HALF);
	VibEnabled = prefs.getBool("Vibrator", VIB_DEFAULT);
	prefs.end();

	// +==============================+
	// |            Serial            |
	// +==============================+
	#if PRINTOUTS
		Serial.begin(115200);
		Serial.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
		Serial.printf ("       RE-VAMP CONTROLLER V%d\r\n", VERSION_MAJOR);
		Serial.printf ("       %s %s\r\n", MANF_NAME, ProductName.c_str());
		Serial.printf ("     PID: 0x%04X  VID: 0x%04X\r\n", (uint16_t)ControllerID, VENDOR_ID);
		Serial.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
		#if SIM_CONTROLS
		Serial.println("SIM controls enabled!");
		#endif

		if(ThrottleTrimSaved > ADC_HALF)      { Serial.printf("Saved Throttle Trim: +%02d%% (%04d/%04d)\r\n", mapRange(ThrottleTrimSaved, ADC_HALF, ADC_MAX, 0, 100), ThrottleTrimSaved, ADC_MAX); }
		else if(ThrottleTrimSaved < ADC_HALF) { Serial.printf("Saved Throttle Trim: -%02d%% (%04d/%04d)\r\n", mapRange(ThrottleTrimSaved, 0, ADC_HALF, 100, 0), ThrottleTrimSaved, ADC_MAX); }
		else/*ThrottleTrimSaved == ADC_HALF*/ { Serial.printf("Default Throttle Trim: %02d%% (%04d/%04d)\r\n", 0, ThrottleTrimSaved, ADC_MAX); }

		if(SteeringTrimSaved > ADC_HALF)      { Serial.printf("Saved Steering Trim: +%02d%% (%04d/%04d)\r\n", mapRange(SteeringTrimSaved, ADC_HALF, ADC_MAX, 0, 100), SteeringTrimSaved, ADC_MAX); }
		else if(SteeringTrimSaved < ADC_HALF) { Serial.printf("Saved Steering Trim: -%02d%% (%04d/%04d)\r\n", mapRange(SteeringTrimSaved, 0, ADC_HALF, 100, 0), SteeringTrimSaved, ADC_MAX); }
		else/*SteeringTrimSaved == ADC_HALF*/ { Serial.printf("Default Steering Trim: %02d%% (%04d/%04d)\r\n", 0, SteeringTrimSaved, ADC_MAX); }

	#endif

	// +==============================+
	// |     Persistant Trim Mode     |
	// +==============================+
	if(LastMenuBtn)
	{
		#if 0 // enter manu through startup button holds
		MenuMode = true;
		#if PRINTOUTS
		Serial.println("TRIM Mode!!");
		Serial.println("Please adjust trim pots to desired levels and press SET button to resume normal operation");
		#endif
		#endif
	}

	// +==============================+
	// |       Vibrator Enabled       |
	// +==============================+
	if(LastSetBtn || VibEnabled)
	{
		VibEnabled = true;
		// Activate Vibrator to indicate
		VibCountdown = VIB_DURATION;
		digitalWrite(PIN_VIBRATOR, HIGH); // turn on vibrator
		VibActivated = true;

		#if PRINTOUTS
		Serial.println("Vibrator Activated!");
		#endif
	}

	// +==============================+
	// |          Bluetooth           |
	// +==============================+
	BleGamepadConfiguration bleGamepadConfig;
	bleGamepadConfig.setAutoReport(false);
	bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD); // CONTROLLER_TYPE_JOYSTICK, CONTROLLER_TYPE_GAMEPAD (DEFAULT), CONTROLLER_TYPE_MULTI_AXIS
	bleGamepadConfig.setButtonCount(NUM_BUTTONS);
	bleGamepadConfig.setHatSwitchCount(NUM_HAT_SWITCHES);
    bleGamepadConfig.setWhichAxes(ENABLE_X, ENABLE_Y, ENABLE_Z, ENABLE_RX, ENABLE_RY, ENABLE_RZ, ENABLE_SLIDER1, ENABLE_SLIDER2);      // Can also be done per-axis individually. All are true by default
    bleGamepadConfig.setAxesMin(AXIS_MIN);
	bleGamepadConfig.setAxesMax(AXIS_MAX);

    #if SIM_CONTROLS
    bleGamepadConfig.setWhichSimulationControls(ENABLE_RUDDER, ENABLE_THROTTLE, ENABLE_ACCELERATOR, ENABLE_BRAKE, ENABLE_STEERING); // Can also be done per-control individually. All are false by default
    bleGamepadConfig.setSimulationMin(SIM_MIN);
    bleGamepadConfig.setSimulationMax(SIM_MAX);
    #endif

	bleGamepad.setBatteryLevel(mapRangeFloat(LastSentBattery, BATT_NO_NOISE, BATT_FULL, 0.0, 100.0)); // map voltage to percentage

    // bleGamepadConfig.setHidReportId(0x5); // unneeded right?
	bleGamepadConfig.setVid(VENDOR_ID);
	bleGamepadConfig.setPid(ControllerID);
	bleGamepad.begin(&bleGamepadConfig); // Changing bleGamepadConfig after the begin function has no effect, unless you call the begin function again

    #if SIM_CONTROLS
    // Set steering to center
    bleGamepad.setSteering(SIM_CENTER);
    bleGamepad.setBrake(SIM_MIN);
    bleGamepad.setAccelerator(SIM_MIN);
    #endif

	// +==============================+
	// |          usbGamepad          |
	// +==============================+
	USB.PID(ControllerID);
	USB.VID(VENDOR_ID);
	USB.productName(ProductName.c_str()); // convert to const char*
	USB.manufacturerName(MANF_NAME);
	USB.begin();

	usbGamepad.setXAxisRange(AXIS_MIN, AXIS_MAX);
	usbGamepad.setYAxisRange(AXIS_MIN, AXIS_MAX);
	usbGamepad.setRxAxisRange(AXIS_MIN, AXIS_MAX);
	usbGamepad.setRyAxisRange(AXIS_MIN, AXIS_MAX);
	usbGamepad.begin(false);
}

// Function to map a number from one range to another
int mapRange(int numIn, int minIn, int maxIn, int minOut, int maxOut)
{
    if (maxIn - minIn == 0) { return 0; } // Input range cannot be zero.
    return (int)((((float)(numIn - minIn) / (maxIn - minIn)) * (maxOut - minOut)) + minOut);
}

float mapRangeFloat(float numIn, float minIn, float maxIn, float minOut, float maxOut)
{
    if (maxIn - minIn == 0.0) { return 0.0; } // Input range cannot be zero.
    if (numIn <= minIn) { return minOut; }
    if (numIn >= maxIn) { return maxOut; }
    return ((((numIn - minIn) / (maxIn - minIn)) * (maxOut - minOut)) + minOut);
}

uint AverageAdc(int adcIn, uint *buffer, int len)
{
	int bIndex = 0;
	uint32_t average = 0;

	for(bIndex = (len-1); bIndex > 0; bIndex--)
	{
		buffer[bIndex] = buffer[bIndex-1];
		average += buffer[bIndex];
	}
	buffer[0] = adcIn;
	average += adcIn;
	return (uint)(average/len);
}

float GetVoltageFromAdc(int adcIn)
{
	return (adcIn * (BATT_MAX_V / ADC_MAX));
}


// +--------------------------------------------------------------+
// |                          Main Loop                           |
// +--------------------------------------------------------------+
void loop()
{
	int throttlePosition = AverageAdc(analogRead(PIN_THT), AdcBufferSteering, ADC_FILT_LEN); // NOTE: 12 bit ADC (0-4095)
	int steeringPosition = AverageAdc(analogRead(PIN_STR), AdcBufferThrottle, ADC_FILT_LEN); // NOTE: 12 bit ADC (0-4095)
	int throttleTrimPosition = AverageAdc(analogRead(PIN_THT_TRM), AdcBufferSteeringTrim, ADC_FILT_LEN); // NOTE: 12 bit ADC (0-4095)
	int steeringTrimPosition = AverageAdc(analogRead(PIN_STR_TRM), AdcBufferThrottleTrim, ADC_FILT_LEN); // NOTE: 12 bit ADC (0-4095)
	float pwrV5dcLevel = GetVoltageFromAdc(AverageAdc(analogRead(PIN_V5DC_ADC) + V5DC_ADC_OFFSET, AdcBufferPwrV5dc, V5DC_FILT_LEN)); // NOTE: 12 bit ADC (0-4095)
	float batteryLevel = GetVoltageFromAdc(AverageAdc(analogRead(PIN_BATT_ADC) + BATT_ADC_OFFSET, AdcBufferBattery, BATT_FILT_LEN)); // NOTE: 12 bit ADC (0-4095)

	bool newThumbBtn = !digitalRead(PIN_THMB_BTN);
	bool newMenuBtn  = !digitalRead(PIN_MENU_BTN);
	bool newSetBtn   = !digitalRead(PIN_SET_BTN);
	bool newTopBtn   = !digitalRead(PIN_TOP_BTN);
	bool newMidBtn   = !digitalRead(PIN_MID_BTN);
	bool newBtmBtn   = !digitalRead(PIN_BTM_BTN);

	// +==============================+
	// |      Analog Adjustments      |
	// +==============================+
	if(ThrottleMin > throttlePosition) { ThrottleMin = throttlePosition; }
	if(ThrottleMax < throttlePosition) { ThrottleMax = throttlePosition; }
	if(SteeringMin > steeringPosition) { SteeringMin = steeringPosition; }
	if(SteeringMax < steeringPosition) { SteeringMax = steeringPosition; }
	if(ThrottleTrimMin > throttleTrimPosition) { ThrottleTrimMin = throttleTrimPosition; }
	if(ThrottleTrimMax < throttleTrimPosition) { ThrottleTrimMax = throttleTrimPosition; }
	if(SteeringTrimMin > steeringTrimPosition) { SteeringTrimMin = steeringTrimPosition; }
	if(SteeringTrimMax < steeringTrimPosition) { SteeringTrimMax = steeringTrimPosition; }

	int adjustedThrottle = mapRange(throttlePosition, ThrottleMin, ThrottleMax, 0, ADC_MAX);
	int adjustedSteering = mapRange(steeringPosition, SteeringMin, SteeringMax, 0, ADC_MAX);

	// +==============================+
	// |        Throttle Trim         |
	// +==============================+
	if(throttleTrimPosition < TRIM_MIN) { throttleTrimPosition = TRIM_MIN; }
	else if(throttleTrimPosition > TRIM_MAX) { throttleTrimPosition = TRIM_MAX; }
	int adjustedThrottleTrim = mapRange(throttleTrimPosition, TRIM_MIN, TRIM_MAX, 0, ADC_MAX); // max out range
	int trimmedCenter = ADC_HALF;
	if(MenuMode) { trimmedCenter = mapRange(adjustedThrottleTrim, 0, ADC_MAX, (ADC_HALF - ((ADC_MAX * TRIM_PERCENT)/2)), (ADC_HALF + ((ADC_MAX * TRIM_PERCENT)/2))); }
	else         { trimmedCenter = mapRange(   ThrottleTrimSaved, 0, ADC_MAX, (ADC_HALF - ((ADC_MAX * TRIM_PERCENT)/2)), (ADC_HALF + ((ADC_MAX * TRIM_PERCENT)/2))); }
	int trimmedThrottle = adjustedThrottle;
	if(adjustedThrottle > trimmedCenter) // Above trimmed center
	{
		trimmedThrottle = mapRange(adjustedThrottle, trimmedCenter, ADC_MAX, ADC_HALF, ADC_MAX); // map from 50-100%
	}
	else if (adjustedThrottle < trimmedCenter) // Below trimmed center
	{
		trimmedThrottle = mapRange(adjustedThrottle, 0, trimmedCenter, 0, ADC_HALF); // map from 0-50%
	}

	// +==============================+
	// |        Steering Trim         |
	// +==============================+
	if(steeringTrimPosition < TRIM_MIN) { steeringTrimPosition = TRIM_MIN; }
	else if(steeringTrimPosition > TRIM_MAX) { steeringTrimPosition = TRIM_MAX; }
	int adjustedSteeringTrim = ADC_MAX - mapRange(steeringTrimPosition, TRIM_MIN, TRIM_MAX, 0, ADC_MAX); // max out range
	if(MenuMode) { trimmedCenter = mapRange(adjustedSteeringTrim, 0, ADC_MAX, (ADC_HALF - ((ADC_MAX * TRIM_PERCENT)/2)), (ADC_HALF + ((ADC_MAX * TRIM_PERCENT)/2))); }
	else         { trimmedCenter = mapRange(   SteeringTrimSaved, 0, ADC_MAX, (ADC_HALF - ((ADC_MAX * TRIM_PERCENT)/2)), (ADC_HALF + ((ADC_MAX * TRIM_PERCENT)/2))); }
	int trimmedSteering = adjustedSteering;
	if(adjustedSteering > trimmedCenter) // Above trimmed center
	{
		trimmedSteering = mapRange(adjustedSteering, trimmedCenter, ADC_MAX, ADC_HALF, ADC_MAX); // map from 50-100%
	}
	else if (adjustedSteering < trimmedCenter) // Below trimmed center
	{
		trimmedSteering = mapRange(adjustedSteering, 0, trimmedCenter, 0, ADC_HALF); // map from 0-50%
	}

	// +==============================+
	// |          Enter Menu          |
	// +==============================+
	if     ( newMenuBtn &&  newSetBtn && !MenuEntrance) { MenuEntrance = true;  }
	else if(!newMenuBtn && !newSetBtn &&  MenuEntrance) { MenuEntrance = false; }

	if(MenuCount > MENU_WAIT && !MenuMode)
	{
		MenuMode = true;
		#if PRINTOUTS
		Serial.println("Entering Menu Mode!!");
		Serial.println("Please adjust trim pots to desired levels and press SET button to resume normal operation");
		Serial.println("Or press THUMB button to toggle vibrator enable. Press MENU to exit");
		#endif
	}


	// +==============================+
	// |          Save Trims          |
	// +==============================+
	if(MenuMode && newSetBtn && !MenuEntrance)
	{
		#if PRINTOUTS
			Serial.println("Saving trim positions!");
			if(adjustedThrottleTrim > ADC_HALF)      { Serial.printf("Throttle Trim: +%02d%% (%04d/%04d)     ", mapRange(adjustedThrottleTrim, ADC_HALF, ADC_MAX, 0, 100), adjustedThrottleTrim, ADC_MAX); }
			else if(adjustedThrottleTrim < ADC_HALF) { Serial.printf("Throttle Trim: -%02d%% (%04d/%04d)     ", mapRange(adjustedThrottleTrim, 0, ADC_HALF, 100, 0), adjustedThrottleTrim, ADC_MAX); }
			else/*adjustedThrottleTrim == ADC_HALF*/ { Serial.printf("Throttle Trim: %02d%% (%04d/%04d)     ", 0, adjustedThrottleTrim, ADC_MAX); }

			if(adjustedSteeringTrim > ADC_HALF)      { Serial.printf("Steering Trim: +%02d%% (%04d/%04d)\r\n", mapRange(adjustedSteeringTrim, ADC_HALF, ADC_MAX, 0, 100), adjustedSteeringTrim, ADC_MAX); }
			else if(adjustedSteeringTrim < ADC_HALF) { Serial.printf("Steering Trim: -%02d%% (%04d/%04d)\r\n", mapRange(adjustedSteeringTrim, 0, ADC_HALF, 100, 0), adjustedSteeringTrim, ADC_MAX); }
			else/*adjustedSteeringTrim == ADC_HALF*/ { Serial.printf("Steering Trim: %02d%% (%04d/%04d)\r\n", 0, adjustedSteeringTrim, ADC_MAX); }
		#endif

		prefs.begin("revamp_prefs", false);
		prefs.putInt("ThrottleTrim", adjustedThrottleTrim);
		prefs.putInt("SteeringTrim", adjustedSteeringTrim);
		prefs.end();

		ThrottleTrimSaved = adjustedThrottleTrim;
		SteeringTrimSaved = adjustedSteeringTrim;

		MenuMode = false;
	}
	
	// +==============================+
	// |     Save Vibrator Enable     |
	// +==============================+
	if(MenuMode && !newThumbBtn && LastThumbBtn && !MenuEntrance)
	{
		VibEnabled = !VibEnabled;
		// Activate Vibrator to indicate
		if(VibEnabled)
		{
			VibCountdown = VIB_DURATION;
			digitalWrite(PIN_VIBRATOR, HIGH); // turn on vibrator
			VibActivated = true;
		}

		#if PRINTOUTS
		Serial.printf("Vibrator %s\r\n", VibEnabled? "Enabled!" : "Disabled!");
		#endif
	}

	// +==============================+
	// |        Exit Menu Mode        |
	// +==============================+
	if(MenuMode && newMenuBtn && !MenuEntrance)
	{
		prefs.begin("revamp_prefs", false);
		if(VibEnabled != prefs.getBool("Vibrator", VIB_DEFAULT))
		{
			prefs.putBool("Vibrator", VibEnabled);
			#if PRINTOUTS
			Serial.println("Saving new Vibrator Enable value");
			#endif
		}
		prefs.end();
		#if PRINTOUTS
		Serial.println("Exiting Menu...");
		#endif
		MenuMode = false;
	}

	// +==============================+
	// |          Printouts           |
	// +==============================+
	#if PRINTOUTS
		if(PrintCountdown == 0)
		{
			PrintCountdown = PRINT_PERIOD;
			if(MenuMode)
			{
				if(adjustedThrottleTrim > ADC_HALF)      { Serial.printf("Throttle Trim: +%02d%% (%04d/%04d)     ", mapRange(adjustedThrottleTrim, ADC_HALF, ADC_MAX, 0, 100), adjustedThrottleTrim, ADC_MAX); }
				else if(adjustedThrottleTrim < ADC_HALF) { Serial.printf("Throttle Trim: -%02d%% (%04d/%04d)     ", mapRange(adjustedThrottleTrim, 0, ADC_HALF, 100, 0), adjustedThrottleTrim, ADC_MAX); }
				else/*adjustedThrottleTrim == ADC_HALF*/ { Serial.printf("Throttle Trim: %02d%% (%04d/%04d)     ", 0, adjustedThrottleTrim, ADC_MAX); }

				if(adjustedSteeringTrim > ADC_HALF)      { Serial.printf("Steering Trim: +%02d%% (%04d/%04d)\r\n", mapRange(adjustedSteeringTrim, ADC_HALF, ADC_MAX, 0, 100), adjustedSteeringTrim, ADC_MAX); }
				else if(adjustedSteeringTrim < ADC_HALF) { Serial.printf("Steering Trim: -%02d%% (%04d/%04d)\r\n", mapRange(adjustedSteeringTrim, 0, ADC_HALF, 100, 0), adjustedSteeringTrim, ADC_MAX); }
				else/*adjustedSteeringTrim == ADC_HALF*/ { Serial.printf("Steering Trim: %02d%% (%04d/%04d)\r\n", 0, adjustedSteeringTrim, ADC_MAX); }
			}
			else
			{
				Serial.printf("BATT %0.2f%% %0.04f V  V5DC: %0.04f V   TH:%d    ST:%d\r\n", mapRangeFloat(batteryLevel, BATT_NO_NOISE, BATT_FULL, 0.0, 99.0), 
																							batteryLevel, 
																							pwrV5dcLevel, 
																							throttlePosition, 
																							steeringPosition);
			}
		}
	#endif

	// +==============================+
	// |             LEDS             |
	// +==============================+
	if(MenuMode) // user can save trim position to memory
	{
		if(LedFlashCountdown == 0)
		{
			LedFlashCountdown = SET_FLASH_PERIOD;
			LedFlashState = !LedFlashState;
			if(!LedFlashState) // turn LEDs off
			{
				analogWrite(PIN_LED_RED,   PWM_LED_MAX);
				analogWrite(PIN_LED_GREEN, PWM_LED_MAX);
			}
		}
		if(LedFlashState) // Control LEDs like normal
		{
			analogWrite(PIN_LED_RED,   mapRange(adjustedThrottleTrim, 0, ADC_MAX, PWM_LED_MIN, PWM_LED_MAX));
			analogWrite(PIN_LED_GREEN, mapRange(adjustedSteeringTrim, 0, ADC_MAX, PWM_LED_MIN, PWM_LED_MAX));
		}

	}
	else if(batteryLevel <= BATT_NO_NOISE && BatteryLowIndication) // Low battery voltage! flash LEDs (red only?)
	{
		if(batteryLevel < pwrV5dcLevel) // someone else is powering the 5V rail
		{
			BatteryLowIndication = false; // stop flashing!
			#if PRINTOUTS
				Serial.println("Battery isn't power us, so no flashing!");
			#endif
		}
		else if(LedFlashCountdown == 0)
		{
			LedFlashCountdown = BATT_FLASH_PERIOD;
			LedFlashState = !LedFlashState;
			if(!LedFlashState) // turn LEDs off
			{
				analogWrite(PIN_LED_RED,   PWM_LED_MAX);
				analogWrite(PIN_LED_GREEN, PWM_LED_MAX);
			}
		}
		if(LedFlashState) // Control LEDs like normal
		{
			analogWrite(PIN_LED_RED,   mapRange(adjustedThrottle, 0, ADC_MAX, PWM_LED_MIN, PWM_LED_MAX));
			analogWrite(PIN_LED_GREEN, mapRange(adjustedSteering, 0, ADC_MAX, PWM_LED_MIN, PWM_LED_MAX));
		}
	}
	else if(!BatteryLowIndication && (batteryLevel - pwrV5dcLevel) > BATT_D_HYST) // we are running on battery
	{
		BatteryLowIndication = true; // Allow low battery indications
		#if PRINTOUTS
			Serial.println("Battery is power us, so flashing allowed!");
		#endif
	}
	else
	{
		analogWrite(PIN_LED_RED,   mapRange(adjustedThrottle, 0, ADC_MAX, PWM_LED_MIN, PWM_LED_MAX));
		analogWrite(PIN_LED_GREEN, mapRange(adjustedSteering, 0, ADC_MAX, PWM_LED_MIN, PWM_LED_MAX));
	}

	// +==============================+
	// |           Vibrator           |
	// +==============================+
	if(VibEnabled && newThumbBtn && VibCountdown == 0 && !VibActivated && !MenuMode)
	{
		VibCountdown = VIB_DURATION;
		digitalWrite(PIN_VIBRATOR, HIGH); // turn on vibrator
		VibActivated = true;
	}
	else if(!newThumbBtn && VibCountdown == 0 && VibActivated)
	{
		VibActivated = false;
	}
	else if(digitalRead(PIN_VIBRATOR) && VibCountdown == 0)
	{
		digitalWrite(PIN_VIBRATOR, LOW); // turn off vibrator
	}

	// +--------------------------------------------------------------+
	// |                         BLE Gamepad                          |
	// +--------------------------------------------------------------+
    if (bleGamepad.isConnected())
    {
		if(AndroidMode)
		{
	        if     (!bleGamepad.isPressed(AND_THMB_BTN) &&  newThumbBtn){ bleGamepad.press  (AND_THMB_BTN); }
	        else if( bleGamepad.isPressed(AND_THMB_BTN) && !newThumbBtn){ bleGamepad.release(AND_THMB_BTN); }
	        if     (!bleGamepad.isPressed(AND_SET_BTN)  &&  newSetBtn)  { bleGamepad.press  (AND_SET_BTN);  }
	        else if( bleGamepad.isPressed(AND_SET_BTN)  && !newSetBtn)  { bleGamepad.release(AND_SET_BTN);  }
	        if     (!bleGamepad.isPressed(AND_MENU_BTN) &&  newMenuBtn) { bleGamepad.press  (AND_MENU_BTN); }
	        else if( bleGamepad.isPressed(AND_MENU_BTN) && !newMenuBtn) { bleGamepad.release(AND_MENU_BTN); }
	        if     (!bleGamepad.isPressed(AND_TOP_BTN)  &&  newTopBtn)  { bleGamepad.press  (AND_TOP_BTN);  }
	        else if( bleGamepad.isPressed(AND_TOP_BTN)  && !newTopBtn)  { bleGamepad.release(AND_TOP_BTN);  }
	        if     (!bleGamepad.isPressed(AND_MID_BTN)  &&  newMidBtn)  { bleGamepad.press  (AND_MID_BTN);  }
	        else if( bleGamepad.isPressed(AND_MID_BTN)  && !newMidBtn)  { bleGamepad.release(AND_MID_BTN);  }
	        if     (!bleGamepad.isPressed(AND_BTM_BTN)  &&  newBtmBtn)  { bleGamepad.press  (AND_BTM_BTN);  }
	        else if( bleGamepad.isPressed(AND_BTM_BTN)  && !newBtmBtn)  { bleGamepad.release(AND_BTM_BTN);  }
	    }
	    else
	    {
	    	if     (!bleGamepad.isPressed(BLE_THMB_BTN) &&  newThumbBtn){ bleGamepad.press  (BLE_THMB_BTN); }
	        else if( bleGamepad.isPressed(BLE_THMB_BTN) && !newThumbBtn){ bleGamepad.release(BLE_THMB_BTN); }
	        if     (!bleGamepad.isPressed(BLE_SET_BTN)  &&  newSetBtn)  { bleGamepad.press  (BLE_SET_BTN);  }
	        else if( bleGamepad.isPressed(BLE_SET_BTN)  && !newSetBtn)  { bleGamepad.release(BLE_SET_BTN);  }
	        if     (!bleGamepad.isPressed(BLE_MENU_BTN) &&  newMenuBtn) { bleGamepad.press  (BLE_MENU_BTN); }
	        else if( bleGamepad.isPressed(BLE_MENU_BTN) && !newMenuBtn) { bleGamepad.release(BLE_MENU_BTN); }
	        if     (!bleGamepad.isPressed(BLE_TOP_BTN)  &&  newTopBtn)  { bleGamepad.press  (BLE_TOP_BTN);  }
	        else if( bleGamepad.isPressed(BLE_TOP_BTN)  && !newTopBtn)  { bleGamepad.release(BLE_TOP_BTN);  }
	        if     (!bleGamepad.isPressed(BLE_MID_BTN)  &&  newMidBtn)  { bleGamepad.press  (BLE_MID_BTN);  }
	        else if( bleGamepad.isPressed(BLE_MID_BTN)  && !newMidBtn)  { bleGamepad.release(BLE_MID_BTN);  }
	        if     (!bleGamepad.isPressed(BLE_BTM_BTN)  &&  newBtmBtn)  { bleGamepad.press  (BLE_BTM_BTN);  }
	        else if( bleGamepad.isPressed(BLE_BTM_BTN)  && !newBtmBtn)  { bleGamepad.release(BLE_BTM_BTN);  }
	    }

        bleGamepad.setX(mapRange(trimmedSteering, 0, ADC_MAX, AXIS_MIN, AXIS_MAX));
	    bleGamepad.setY(mapRange(trimmedThrottle, 0, ADC_MAX, AXIS_MIN, AXIS_MAX));
	    bleGamepad.setRX(mapRange(adjustedSteeringTrim, 0, ADC_MAX, AXIS_MIN, AXIS_MAX));
	    bleGamepad.setRY(mapRange(adjustedThrottleTrim, 0, ADC_MAX, AXIS_MIN, AXIS_MAX));
		
		#if SIM_CONTROLS
			// +==============================+
			// |         Sim Controls         |
			// +==============================+
		    bleGamepad.setSteering(mapRange(trimmedSteering, 0, ADC_MAX, SIM_MIN, SIM_MAX));
		    bleGamepad.setBrake(mapRange(trimmedThrottle, ADC_HALF, ADC_MAX, SIM_MIN, SIM_MAX)); // TODO: verify this is mapped correctly
		    bleGamepad.setAccelerator(mapRange(trimmedThrottle, 0, ADC_HALF, SIM_MIN, SIM_MAX)); // TODO: verify this is mapped correctly
        #endif

    	// +==============================+
		// |           Battery            |
		// +==============================+ // this needs to only update like every 5% or 30s or something
		if(batteryLevel < pwrV5dcLevel && !PowerFromUSB) // Battery is lower than 5V rail so we are USB powered (report 100% to have windows hide battery level)
		{
			PowerFromUSB = true;
			bleGamepad.setBatteryLevel(101);
			#if PRINTOUTS
				Serial.println("On USB Power!");
			#endif
		}
		else if(PowerFromUSB && (batteryLevel - pwrV5dcLevel) > BATT_D_HYST) // back to battery power!
		{
			PowerFromUSB = false;
			// bleGamepad.setBatteryLevel(mapRangeFloat(batteryLevel, BATT_NO_NOISE, BATT_FULL, 0.0, 100.0)); // map voltage to percentage
			#if PRINTOUTS
				Serial.println("On BATT Power!");
			#endif
		}

		if(!PowerFromUSB) // Battery is powering the 5V rail (running on bettery)
		{
			if(AdcUpdateCountdown == 0 && (batteryLevel > (LastSentBattery + BATT_REP_DELTA) || batteryLevel < (LastSentBattery - BATT_REP_DELTA)))
			{
				bleGamepad.setBatteryLevel(mapRangeFloat(batteryLevel, BATT_NO_NOISE, BATT_FULL, 0.0, 100.0)); // map voltage to percentage
				LastSentBattery = batteryLevel;
				AdcUpdateCountdown = BATT_UPDATE_PERIOD;
			}
		}

        bleGamepad.sendReport();
    }
    else
    {
    	// +--------------------------------------------------------------+
    	// |                         USB Gamepad                          |
    	// +--------------------------------------------------------------+

		usbGamepad.setButton(USB_THMB_BTN, newThumbBtn);
		usbGamepad.setButton(USB_SET_BTN,  newSetBtn);
		usbGamepad.setButton(USB_MENU_BTN, newMenuBtn);
		usbGamepad.setButton(USB_TOP_BTN,  newTopBtn);
		usbGamepad.setButton(USB_MID_BTN,  newMidBtn);
		usbGamepad.setButton(USB_BTM_BTN,  newBtmBtn);

		// +==============================+
		// |          Joysticks           |
		// +==============================+
		usbGamepad.setXAxis(mapRange(trimmedSteering, 0, ADC_MAX, 0, 0x7FFF));
		usbGamepad.setYAxis(mapRange(trimmedThrottle, 0, ADC_MAX, 0, 0x7FFF));
		usbGamepad.setRxAxis(mapRange(adjustedSteeringTrim, 0, ADC_MAX, 0, 0x7FFF));
		usbGamepad.setRyAxis(mapRange(adjustedThrottleTrim, 0, ADC_MAX, 0, 0x7FFF));
		usbGamepad.sendState();
    }

    // +--------------------------------------------------------------+
    // |                      Last State Update                       |
    // +--------------------------------------------------------------+
    LastSteeringTrim = steeringPosition;
	LastThrottleTrim = throttlePosition;
	LastSteering     = throttleTrimPosition;
	LastThrottle     = steeringTrimPosition;
	LastSentBattery  = batteryLevel;
	LastThumbBtn     = newThumbBtn;
	LastMenuBtn      = newMenuBtn;
	LastSetBtn       = newSetBtn;
	LastTopBtn       = newTopBtn;
	LastMidBtn       = newMidBtn;
	LastBtmBtn       = newBtmBtn;

}