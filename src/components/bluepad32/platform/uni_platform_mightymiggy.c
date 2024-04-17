// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 SukkoPera <software@sukkology.net>

#include "platform/uni_platform_mightymiggy.h"

#include <driver/gpio.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <nvs.h>
#include <nvs_flash.h>

#include "sdkconfig.h"

#include "bt/uni_bt.h"
#include "controller/uni_controller.h"
#include "controller/uni_controller_type.h"
#include "uni_common.h"
#include "uni_config.h"
#include "uni_hid_device.h"
#include "uni_joystick.h"
#include "uni_log.h"

#ifndef CONFIG_IDF_TARGET_ESP32
#error "This file can only be compiled for ESP32"
#endif  // CONFIG_IDF_TARGET_ESP32

/** \def MMBOARD_REV_B
 *
 * Define this when targeting the Amiga Version of the Unijoysticle (Rev B),
 * available at https://gitlab.com/SukkoPera/unijoysticle2.
 */
#define MMBOARD_REV_B

/** \def MMBOARD_REV_A
 *
 * Define this for Rev A of the above (does NOT support CD32 mode).
 */
//~ #define MMBOARD_REV_A

/** \def MMBOARD_C64_REV_F
 *
 * Use this for the original C64 version of the Unijoysticle board (Rev F). Does
 * not support CD32 mode and only 1 button for port B
 */
//~ #define MMBOARD_C64_REV_F

/** \def USE_PEDALS_AS_TRIGGERS
 *
 * Define this to emulate triggers with the throttle/brake pedals. Note that a
 * "pedal" might not actually be what you expect, it can be just a sort of
 * "analog button", while triggers are generally digital (i.e. either pressed or
 * not)
 */
#define USE_PEDALS_AS_TRIGGERS

/** \brief Pedal trigger point
 *
 * Of course the above needs a "trigger point" which will trigger the... trigger
 * when exceeded
 */
static const unsigned int PEDAL_THRESHOLD = 800;

/** \def ENABLE_INSTRUMENTATION
 *
 * \brief Enable code instrumentation
 *
 * This makes a couple of I/O pins toggle whenever ISRs are entered and is a
 * means to measure how long they take to execute fully, by using a logic
 * analyzer or oscilloscope.
 *
 * Disabled by default.
 */
//~ #define ENABLE_INSTRUMENTATION

/******************************************************************************/

#ifdef MMBOARD_REV_B
#define ENABLE_CD32_SUPPORT
#endif

#ifdef ENABLE_INSTRUMENTATION
static const gpio_num_t PIN_INTERRUPT_TIMING = GPIO_NUM_0;
static const gpio_num_t PIN_CD32MODE = GPIO_NUM_4;
#endif

static const char* STORAGE_NAMESPACE = "storage";

static const char* NVS_KEY_CONFIG = "config";

// GPIO map is based on the MH-ET Live mini-kit board

// No use for this ATM
#ifdef MMBOARD_REV_B
static const int GPIO_PUSH_BUTTON = GPIO_NUM_15;
#else
static const int GPIO_PUSH_BUTTON = GPIO_NUM_10;
#endif

#ifdef ENABLE_CD32_SUPPORT
#define PINS_PER_PORT 8
#else
#define PINS_PER_PORT 6
#endif

/** \name OUTPUT pins for Joy Port A (Port 0, the one usually used for the
 *        mouse)
 */
//! @{
static const gpio_num_t PIN_A_UP = GPIO_NUM_26;
static const gpio_num_t PIN_A_DOWN = GPIO_NUM_18;
static const gpio_num_t PIN_A_LEFT = GPIO_NUM_19;
static const gpio_num_t PIN_A_RIGHT = GPIO_NUM_23;
static const gpio_num_t PIN_A_FIRE = GPIO_NUM_14;
static const gpio_num_t PIN_A_FIRE2 = GPIO_NUM_33;

#ifdef ENABLE_CD32_SUPPORT
/** \brief Controller mode input pin for Joy Port A
 *
 * This GPIO is connected to pin 5 of the Amiga port. It toggles between Amiga
 * (HIGH) and CD32 (LOW) mode.
 *
 * It also triggers the loading of the button status shift register.
 *
 * We need to set this to a pulled-up input and connect it through a diode to
 * make a poor man's level shifter. Cathode goes towards the Amige port.
 *
 * Note that we use GPIO 10 here, which means the ESP32 shall access its flash
 * memory in DIO rather than QIO.
 */
static const gpio_num_t PIN_A_MODE = GPIO_NUM_10;

/** \brief Controller clock input pin for Joy Port A
 *
 * This GPIO is connected to pin 6 of the Amiga port. We will use it to sample
 * the clock in CD32 mode.
 *
 * A poor man's level shifter will be needed here, too.
 *
 * And we use GPIO 9, which is freed by using DIO as well.
 */
static const gpio_num_t PIN_A_CLOCK = GPIO_NUM_9;
#endif

static const gpio_num_t PINS_PORT_A[PINS_PER_PORT] = {PIN_A_UP,    PIN_A_DOWN, PIN_A_LEFT,
                                                      PIN_A_RIGHT, PIN_A_FIRE, PIN_A_FIRE2,
#ifdef ENABLE_CD32_SUPPORT
                                                      PIN_A_MODE,  PIN_A_CLOCK
#endif
};
//! @}

/** \name OUTPUT pins for Joy Port B (Port 1, the one usually used for the first
 *        player's joystick)
 */
//! @{
static const gpio_num_t PIN_B_UP = GPIO_NUM_27;
static const gpio_num_t PIN_B_DOWN = GPIO_NUM_25;
static const gpio_num_t PIN_B_LEFT = GPIO_NUM_32;
static const gpio_num_t PIN_B_RIGHT = GPIO_NUM_17;
static const gpio_num_t PIN_B_FIRE = GPIO_NUM_12;
static const gpio_num_t PIN_B_FIRE2 = GPIO_NUM_16;

#ifdef ENABLE_CD32_SUPPORT
/** \brief Controller mode input pin for Joy Port B
 *
 * \sa PIN_A_MODE
 */
static const gpio_num_t PIN_B_MODE = GPIO_NUM_22;

/** \brief Controller clock input pin for Joy Port B
 *
 * \sa PIN_A_CLOCK
 */
static const gpio_num_t PIN_B_CLOCK = GPIO_NUM_21;
#endif

static const gpio_num_t PINS_PORT_B[PINS_PER_PORT] = {PIN_B_UP,    PIN_B_DOWN, PIN_B_LEFT,
                                                      PIN_B_RIGHT, PIN_B_FIRE, PIN_B_FIRE2,
#ifdef ENABLE_CD32_SUPPORT
                                                      PIN_B_MODE,  PIN_B_CLOCK
#endif
};
//! @}

// These are to be used as indexes for the PINS_PORT_x[] arrays
static const uint8_t PIN_NO_UP = 0;     // Port pin 1
static const uint8_t PIN_NO_DOWN = 1;   // Port pin 2
static const uint8_t PIN_NO_LEFT = 2;   // Port pin 3
static const uint8_t PIN_NO_RIGHT = 3;  // Port pin 4
static const uint8_t PIN_NO_B1 = 4;     // Port pin 6
static const uint8_t PIN_NO_B2 = 5;     // Port pin 9 (PotY)
#ifdef ENABLE_CD32_SUPPORT
static const uint8_t PIN_NO_MODE = 6;   // Port pin 5 (PotX)
static const uint8_t PIN_NO_CLOCK = 7;  // Port pin 6 (Again)
#endif

static const int16_t BLUEPAD32_ANALOG_MIN = -512;
static const int16_t BLUEPAD32_ANALOG_MAX = +511;

/** \brief Dead zone for analog sticks
 *
 * If the analog stick moves less than this value from the center position, it
 * is considered still.
 *
 * \sa ANALOG_IDLE_VALUE
 */
static const uint8_t ANALOG_DEAD_ZONE = 75U;

/** \brief Delay of the quadrature square waves when mouse is moving at the
 * \a slowest speed
 */
static const uint8_t MOUSE_SLOW_DELTA = 150U;

/** \brief Delay of the quadrature square waves when mouse is moving at the
 * \a fastest speed.
 */
static const uint8_t MOUSE_FAST_DELTA = 10U;

/** \brief LED2 pin
 *
 * Pin for led that lights up whenever the proper controller is detected.
 */
static const gpio_num_t PIN_LED_P2 = GPIO_NUM_5;

/** \brief LED1 pin
 *
 * Pin for led that lights up whenever the adapter is in CD32 mode.
 */
static const gpio_num_t PIN_LED_P1 = GPIO_NUM_13;

/** \brief CD32 mode timeout
 *
 * Normal joystick mode will be entered if PIN_PADMODE is not toggled for this
 * amount of milliseconds.
 *
 * Keep in mind that in normal conditions it is toggled once per frame (i.e.
 * every ~20 ms).
 */
static const uint8_t TIMEOUT_CD32_MODE = 100U;

/** \brief Programming mode timeout
 *
 * Programming mode will be entered if SELECT + a button are held for this
 * amount of milliseconds.
 */
static const unsigned long TIMEOUT_PROGRAMMING_MODE = 1000U;

/** \brief Single-button debounce time
 *
 * A combo will be considered valid only after it has been stable for this
 * amount of milliseconds.
 *
 * \sa debounceButtons()
 */
static const unsigned long DEBOUNCE_TIME_BUTTON = 30U;

/** \brief Combo debounce time
 *
 * A combo will be considered valid only after it has been stable for this
 * amount of milliseconds. This needs to be greater than #DEBOUNCE_TIME_BUTTON
 * to allow combos to be entered correctly, as it's hard to press several
 * buttons at exactly the same moment.
 *
 * \sa debounceButtons()
 */
static const unsigned long DEBOUNCE_TIME_COMBO = 150U;

/*******************************************************************************
 * DEBUGGING SUPPORT
 ******************************************************************************/

// Send debug messages to serial port
//~ #define ENABLE_SERIAL_DEBUG

// Print the controller status on serial. Useful for debugging.
//~ #define DEBUG_PAD

/*******************************************************************************
 * END OF SETTINGS
 ******************************************************************************/

//! \name Button bits for CD32 mode
//! @{
static const uint8_t BTN32_BLUE = 1U << 0U;     //!< \a Blue Button
static const uint8_t BTN32_RED = 1U << 1U;      //!< \a Red Button
static const uint8_t BTN32_YELLOW = 1U << 2U;   //!< \a Yellow Button
static const uint8_t BTN32_GREEN = 1U << 3U;    //!< \a Green Button
static const uint8_t BTN32_FRONT_R = 1U << 4U;  //!< \a Front \a Right Button
static const uint8_t BTN32_FRONT_L = 1U << 5U;  //!< \a Front \a Left Button
static const uint8_t BTN32_START = 1U << 6U;    //!< \a Start/Pause Button
//! @}

/** \brief Controller State machine states
 *
 * Possible states for the internal state machine that drives a single
 * controller.
 */
typedef enum {
    ST_FIRST_READ,  //!< First time the controller is read

    // Main functioning modes - All of these include mouse control with right stick
    ST_JOYSTICK,       //!< Two-button joystick mode
    ST_CD32,           //!< CD32-controller mode
    ST_JOYSTICK_TEMP,  //!< Just come out of CD32 mode, will it last?

    // States to select mapping or go into programming mode
    ST_SELECT_HELD,          //!< Select being held
    ST_SELECT_AND_BTN_HELD,  //!< Select + mapping button being held
    ST_ENABLE_MAPPING,       //!< Select + mapping button released, enable mapping

    // States for programming mode
    ST_WAIT_SELECT_RELEASE,           //!< Select released, entering programming mode
    ST_WAIT_BUTTON_PRESS,             //!< Programmable button pressed
    ST_WAIT_BUTTON_RELEASE,           //!< Programmable button released
    ST_WAIT_COMBO_PRESS,              //!< Combo pressed
    ST_WAIT_COMBO_RELEASE,            //!< Combo released
    ST_WAIT_SELECT_RELEASE_FOR_EXIT,  //!< Wait for select to be released to go back to joystick mode
} ControllerState;

/** \brief Controller State machine states
 *
 * Possible states for the internal state machine that drives the adapter as a
 * whole.
 */
typedef enum {
    AST_IDLE,            //!< No controller connected
    AST_JOY2_ONLY,       //!< One controller connected, controls joystick in port 2 and mouse in port 1
    AST_JOY1_ONLY,       //!< We had two controllers, we lost one and we're left with port 1 only
    AST_TWO_JOYS,        //!< We have two controllers, both working as joysticks
    AST_TWO_JOYS_2IDLE,  //!< We have two controllers, the first controls joystick in port 2 and mouse in port 1, the
    //!< second is idle

    // States for factory reset
    AST_FACTORY_RESET_WAIT_1,
    AST_FACTORY_RESET_WAIT_2,
    AST_FACTORY_RESET_PERFORM
} AdapterState;

/** \brief All possible button mappings
 *
 * This is only used for blinking the led when mapping is changed
 */
typedef enum { JMAP_NORMAL = 1, JMAP_RACING1, JMAP_RACING2, JMAP_PLATFORM, JMAP_CUSTOM } JoyButtonMapping;

/** \brief Structure representing a standard 2-button Atari-style joystick
 *
 * This is used for gathering button presses according to different button
 * mappings.
 *
 * True means pressed.
 *
 * We use bitfields so that it all fits in a single uint8_t.
 */
typedef struct {
    bool up : 1;     //!< Up/Forward direction
    bool down : 1;   //!< Down/Backwards direction
    bool left : 1;   //!< Left direction
    bool right : 1;  //!< Right direction
    bool b1 : 1;     //!< Button 1
    bool b2 : 1;     //!< Button 2
} TwoButtonJoystick;

/** \brief Type that is used to store button presses
 *
 * Here 1 means pressed. Variables of this type should only be inspected and
 * manipulated through the dedicated functions.
 */
typedef uint32_t PadButtons;

/** \brief Number of digital buttons
 *
 * This is the number of entries in #PadButtons.
 */
#define PAD_BUTTONS_NO 17

/** \brief Map a PSX button to a two-button-joystick combo
 *
 * There's an entry for every button, even those that cannot be mapped, for
 * future extension.
 *
 * Use #psxButtonToIndex() to convert a button to the index to use in the array.
 *
 * \sa isButtonMappable()
 */
typedef struct {
    /** Two-button joystick combo to send out when the corresponding button is
     * pressed
     */
    TwoButtonJoystick buttonMappings[PAD_BUTTONS_NO];
} ControllerConfiguration;

/** \brief Type that is used to index a single button in #PadButtons
 */
typedef enum {
    BTN_NONE = 0x00000,
    BTN_BACK = 0x00001,  // AKA Select
    BTN_THUMB_L = 0x00002,
    BTN_THUMB_R = 0x00004,
    BTN_HOME = 0x00008,  // AKA Start
    BTN_PAD_UP = 0x00010,
    BTN_PAD_RIGHT = 0x00020,
    BTN_PAD_DOWN = 0x00040,
    BTN_PAD_LEFT = 0x00080,
    BTN_TRIGGER_L = 0x00100,
    BTN_TRIGGER_R = 0x00200,
    BTN_SHOULDER_L = 0x00400,
    BTN_SHOULDER_R = 0x00800,
    BTN_Y = 0x01000,
    BTN_B = 0x02000,
    BTN_A = 0x04000,
    BTN_X = 0x08000,
    BTN_SYSTEM = 0X10000  // AKA Home
} PadButton;

//! \brief 2-Axes Analog stick
typedef struct {
    int16_t x;
    int16_t y;
} AnalogStick;

static const bd_addr_t zero_addr = {0, 0, 0, 0, 0, 0};

//! \name Global variables
//! @{

static AdapterState adapterState;

/** \brief All possible controller configurations
 *
 * Since these are activated with SELECT + a button, ideally there can be as
 * many different ones as other buttons we have (i.e.: PAD_BUTTONS_NO). In
 * practice, we will start handling only a handful.
 */
static ControllerConfiguration controllerConfigs[PAD_BUTTONS_NO];

// Forward declaration
typedef struct RuntimeControllerInfo_s RuntimeControllerInfo;

/** \brief Joystick mapping function
 *
 * This represents a function that should inspect the buttons currently being
 * pressed on the PSX controller and somehow map them to a #TwoButtonJoystick to
 * be sent to the DB-9 port.
 */
typedef void (*JoyMappingFunc)(const RuntimeControllerInfo* cinfo, TwoButtonJoystick* j);

// Default button mapping function prototype for initialization of the following
static void mapJoystickNormal(const RuntimeControllerInfo* cinfo, TwoButtonJoystick* j);

// We have 128 bytes available here (HID_DEVICE_MAX_PLATFORM_DATA)
typedef struct RuntimeControllerInfo_s {
    //! \brief "Seat" (i.e.: port) this controller is connected to
    uni_gamepad_seat_t seat;

    //! \brief Pin driving the led for this controller
    gpio_num_t ledPin;

    //! \brief Array of GPIOs connected to the Amiga joystick port (Port 2)
    const gpio_num_t* joyPins;

    //! \brief Array of GPIOs connected to the Amiga mouse port (Port 1)
    const gpio_num_t* mousePins;

    /** \brief Current state of the internal state machine
     *
     * We start out as a simple joystick.
     */
    volatile ControllerState state;

    /** \brief Time the current state was entered at
     *
     * Used to guide time-drive transitions of the controller state machine.
     */
    unsigned long stateEnteredTime;

    //! \brief Left Analog Stick
    AnalogStick leftAnalog;

    //! \brief Right Analog Stick
    AnalogStick rightAnalog;

    /** \brief A word representing the current status of all buttons
     *
     * Note that this also includes D-Pad buttons.
     */
    PadButtons buttonWord;

    //! \brief A word representing the previous status of all buttons
    PadButtons previousButtonWord;

    //! \brief Joystick mapping function currently in effect
    JoyMappingFunc joyMappingFunc;

    //! \brief Custom controller configuration currently selected
    ControllerConfiguration* currentCustomConfig;

    /** \brief Button register for CD32 mode being updated
     *
     * This shall be updated as often as possible, and is what gets sampled when we
     * get a falling edge on #PIN_PADMODE.
     *
     * 0 means pressed, MSB must be 1 for the ID sequence.
     */
    volatile uint8_t buttonsLive;

    /** \brief Button register for CD32 mode currently being shifted out
     *
     * This is where #buttonsLive gets copied when it is sampled.
     *
     * 0 means pressed, etc.
     */
    volatile uint8_t isrButtons;

    /** \brief Commodore 64 mode
     *
     * Button 2 on the C64 is usually expected to behave differently from the other
     * buttons, so that it is HIGH when pressed and LOW when released.
     *
     * If this flag is true, we'll do exactly that.
     */
    bool c64Mode;

    /** \brief Use alternative CD32 mapping
     *
     * To me the Red button maps naturally to Square, Blue to Cross and so on. Some
     * people feel this way buttons are "rotated" with regard to the original CD32
     * controller, hence let's give them the possibility to "counter-rotate" the
     * mapping so that Cross is Red, Circle is Blue and so on.
     */
    bool useAlternativeCd32Mapping;

    //! \brief Button being pressed with SELECT during button mapping switch
    PadButton selectComboButton;

    //! \brief Button being programmed
    PadButton programmedButton;
} RuntimeControllerInfo;

_Static_assert(sizeof(RuntimeControllerInfo) < HID_DEVICE_MAX_PLATFORM_DATA, "RuntimeControllerInfo instance too big");

enum {
    EVENT_ENABLE_CD32_SEAT_A = (1 << 0),
    EVENT_DISABLE_CD32_SEAT_A = (1 << 1),
    EVENT_ENABLE_CD32_SEAT_B = (1 << 2),
    EVENT_DISABLE_CD32_SEAT_B = (1 << 3),
};

static EventGroupHandle_t evGrpCd32;

static QueueHandle_t controllerUpdateQueue = NULL;

//! @}		// End of global variables

#ifdef ENABLE_SERIAL_DEBUG
#define mmlogd(...) logd(__VA_ARGS__)
#define mmloge(...) loge(__VA_ARGS__)
#define mmlogi(...) logi(__VA_ARGS__)
#else
#define mmlogd(...)
#define mmloge(...)
#define mmlogi(...)
#endif

//! \name Stuff for Arduino compatibility
//! @{

static uint32_t millis() {
    return esp_timer_get_time() / 1000;
}

static long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static int16_t constrain16(const int16_t x, const int16_t min, const int16_t max) {
    if (x < min) {
        return min;
    } else if (x > max) {
        return max;
    } else {
        return x;
    }
}

//! @}

//! \name Manipulation functions for #PadButtons
//! @{

/** \brief Button name strings
 *
 * These are only used for debugging and will not show up in the executable if
 * it is not enabled
 */
#if defined(ENABLE_SERIAL_DEBUG) || defined(DEBUG_PAD)
static const char* const padButtonNames[PAD_BUTTONS_NO] = {
    "Back",     "ThumbL",    "ThumbR",    "Home", "Up", "Right", "Down", "Left",  "TriggerL",
    "TriggerR", "ShoulderL", "ShoulderR", "Y",    "B",  "A",     "X",    "System"};
#endif  // defined(ENABLE_SERIAL_DEBUG) || defined(DEBUG_PAD)

/** \brief Convert a button on the PSX controller to a small integer
 *
 * Output will always be in the range [0, PSX_BUTTONS_NO - 1] and is not
 * guaranteed to be valid, so it should be checked to be < PSX_BUTTONS_NO before
 * use.
 *
 * \param[in] buttons Button to be converted
 * \return A small integer corresponding to the "first" button pressed
 */
static int padButtonToIndex(PadButtons buttons) {
    int i;

    for (i = 0; i < PAD_BUTTONS_NO; ++i) {
        if (buttons & 0x01) {
            break;
        }

        buttons >>= 1U;
    }

    return i;
}

#ifdef ENABLE_SERIAL_DEBUG
/** \brief Return the name of a PSX button
 *
 * \param[in] buttons Button to be converted
 * \return A string (in flash) containing the name of the "first" buton pressed
 */
static const char* getButtonName(PadButtons psxButton) {
    const char* ret = "";

    int b = padButtonToIndex(psxButton);
    if (b < PAD_BUTTONS_NO) {
        ret = padButtonNames[b];
    }

    return ret;
}
#endif

/** \brief Print the names of all PSX buttons being pressed
 *
 * \param[in] buttons Buttons to be printed
 */
static void dumpButtons(PadButtons buttons) {
#ifdef DEBUG_PAD
    static PadButtons lastB = 0;

    if (buttons != lastB) {
        lastB = buttons;  // Save it before we alter it

        mmlogd("Pressed: ");

        for (uint8_t i = 0; i < PAD_BUTTONS_NO; ++i) {
            uint8_t b = padButtonToIndex(buttons);
            if (b < PAD_BUTTONS_NO) {
                mmlogd("%s", padButtonNames[b]);
            }

            buttons &= ~(1 << b);

            if (buttons != 0) {
                mmlogd(", ");
            }
        }

        mmlogd("\n");
    }
#else
    (void)buttons;
#endif
}

/** \brief Check if a button has changed state
 *
 * \return true if \a button has changed state with regard to the previous
 *         call to read(), false otherwise
 */
static bool buttonChanged(const PadButtons buttonWord, const PadButtons previousButtonWord, const PadButton button) {
    return ((previousButtonWord ^ buttonWord) & button) > 0;
}

static bool buttonPressed(const PadButtons buttonWord, const PadButton button) {
    return buttonWord & ((PadButtons)button);
}

static bool noButtonPressed(const PadButtons buttonWord) {
    return buttonWord == BTN_NONE;
}

/** \brief Check if a button has just been pressed
 *
 * \param[in] button The button to be checked
 * \return true if \a button was not pressed in the previous call to read()
 *         and is now, false otherwise
 */
static bool buttonJustPressed(const PadButtons buttonWord,
                              const PadButtons previousButtonWord,
                              const PadButton button) {
    return buttonChanged(buttonWord, previousButtonWord, button) & buttonPressed(buttonWord, button);
}

/** \brief Debounce button/combo presses
 *
 * Makes sure that the same button/combo has been pressed steadily for some
 * time.
 *
 * \sa DEBOUNCE_TIME_BUTTON
 * \sa DEBOUNCE_TIME_COMBO
 *
 * \param[in] holdTime Time the button/combo must be stable for
 */
// FIXME: Use previousButtons?
static PadButtons debounceButtons(const PadButtons buttonWord,
                                  const PadButtons previousButtonWord,
                                  const unsigned long holdTime) {
    static PadButtons oldButtons = BTN_NONE;
    static unsigned long pressedOn = 0;

    PadButtons ret = BTN_NONE;

    if (buttonWord == oldButtons) {
        if (millis() - pressedOn > holdTime) {
            // Same combo held long enough
            ret = buttonWord;
        } else {
            // Combo held not long enough (yet)
        }
    } else {
        // Buttons bouncing
        oldButtons = buttonWord;
        pressedOn = millis();
    }

    return ret;
}

//! @}		// End of PadButtons manipulation functions

/** \brief Enable CD32 controller support
 *
 * CD32 mode is entered automatically whenever a HIGH level is detected on
 * #PIN_PADMODE, after this function has been called.
 */
static void enableCD32Trigger(const RuntimeControllerInfo* cinfo) {
#ifdef ENABLE_CD32_SUPPORT
    if (cinfo->seat == GAMEPAD_SEAT_A) {
        xEventGroupSetBits(evGrpCd32, EVENT_ENABLE_CD32_SEAT_A);
    } else if (cinfo->seat == GAMEPAD_SEAT_B) {
        xEventGroupSetBits(evGrpCd32, EVENT_ENABLE_CD32_SEAT_B);
    } else {
        mmloge("enableCD32Trigger() called on unsupported seat\n");
    }
#endif
}

/** \brief Disable CD32 controller support
 *
 * CD32 mode will no longer be entered automatically after this function has
 * been called.
 */
static void disableCD32Trigger(const RuntimeControllerInfo* cinfo) {
#ifdef ENABLE_CD32_SUPPORT
    if (cinfo->seat == GAMEPAD_SEAT_A) {
        xEventGroupSetBits(evGrpCd32, EVENT_DISABLE_CD32_SEAT_A);
    } else if (cinfo->seat == GAMEPAD_SEAT_B) {
        xEventGroupSetBits(evGrpCd32, EVENT_DISABLE_CD32_SEAT_B);
    } else {
        mmloge("disableCD32Trigger() called on unsupported seat\n");
    }
#endif
}

/** \brief Clear controller configurations
 *
 * All controller configuration will be reset so that:
 * - Square is button 1
 * - Cross is button 2
 * - All other buttons are inactive
 *
 * The programming function can then be used to map any button as desired.
 */
static void clearConfigurations() {
    mmlogi("Clearing controllerConfigs\n");
    memset(controllerConfigs, 0x00, sizeof(controllerConfigs));
    for (uint8_t i = 0; i < PAD_BUTTONS_NO; ++i) {
        ControllerConfiguration* config = &controllerConfigs[i];
        //~ memset (&config, 0x00, sizeof (ControllerConfiguration));
        config->buttonMappings[padButtonToIndex(BTN_X)].b1 = true;
        config->buttonMappings[padButtonToIndex(BTN_A)].b2 = true;
    }
}

/** \brief Load controller configurations from EEPROM
 *
 * If the loaded configurations are not valid, they are cleared.
 *
 * \return True if the loaded configurations are valid, false otherwise
 */
static bool loadConfigurations() {
    bool ret = false;

    nvs_handle_t nvs_handle;
    esp_err_t err;

    mmlogi("Loading controllerConfigs\n");
    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t sz = sizeof(controllerConfigs);
        err = nvs_get_blob(nvs_handle, NVS_KEY_CONFIG, &controllerConfigs, &sz);
        if (err == ESP_OK && sz == sizeof(controllerConfigs)) {
            ret = true;
        } else {
            mmloge("Load failed\n");
            clearConfigurations();
        }

        nvs_close(nvs_handle);
    } else {
        mmloge("Cannot open Non-Volatile Storage\n");
    }

    return ret;
}

/** \brief Save controller configurations to EEPROM
 *
 * Note that there's no need to store a CRC of some kind, as the ESP32 NVS
 * already includes it.
 */
static void saveConfigurations() {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    mmlogi("Saving controllerConfigs\n");
    mmlogd("Size of controllerConfigs is %u\n", (unsigned int)sizeof(controllerConfigs));
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        err = nvs_set_blob(nvs_handle, NVS_KEY_CONFIG, &controllerConfigs, sizeof(controllerConfigs));
        if (err == ESP_OK) {
            err = nvs_commit(nvs_handle);
            if (err != ESP_OK) {
                mmloge("Commit failed\n");
            }
        } else {
            mmloge("Save failed with error %d\n", err);
        }

        nvs_close(nvs_handle);
    } else {
        mmloge("Cannot open Non-Volatile Storage\n");
    }
}

/** \brief Report a button as pressed on the DB-9 port
 *
 * Output pins on the DB-9 port are driven in open-collector style, so that we
 * are compatible with the C64 too.
 *
 * Note that LOW is implicit when switching over from INPUT without pull-ups.
 *
 * This function might look a bit silly, but it is written this way so that it
 * is translated to very efficient code (basically a single ASM instruction!) by
 * using the \a DigitalIO library and with the switched pin being known at
 * compile time.
 *
 * \sa buttonRelease()
 *
 * \param[in] pin Pin corresponding to the button to be pressed
 */
static void buttonPress(const gpio_num_t pin) {
    /* We have an (open-collector) inverter gate between us and the actual port
     * pin, so we must use inverse logic... which, since joystick signals are
     * active-low, means "normal" logic, i.e.: 1 = pressed ;)
     */
    ESP_ERROR_CHECK(gpio_set_level(pin, 1));
}

/** \brief Report a button as released on the DB-9 port
 *
 * \sa buttonPress()
 *
 * \param[in] pin Pin corresponding to the button to be pressed
 */
static void buttonRelease(const gpio_num_t pin) {
    ESP_ERROR_CHECK(gpio_set_level(pin, 0));
}

/** \brief Map horizontal movements of the left analog stick to a
 *         #TwoButtonJoystick
 *
 * The stick is not considered moved if it moves less than #ANALOG_DEAD_ZONE.
 *
 * \param[out] j Mapped joystick status
 */
static void mapAnalogStickHorizontal(const RuntimeControllerInfo* cinfo, TwoButtonJoystick* j) {
    // Bluepad analog range is [-512, 511] - But it seems to be 1024+ on Wii U Pro Controller!
    j->left = cinfo->leftAnalog.x < -ANALOG_DEAD_ZONE;
    j->right = cinfo->leftAnalog.x > +ANALOG_DEAD_ZONE;

#ifdef ENABLE_SERIAL_DEBUG
    // Note that this goes crazy when we have more than one controller connected
    static int32_t oldx = -10000;
    if (abs(cinfo->leftAnalog.x - oldx) > 5) {
        mmlogd("L Analog X = %d\n", cinfo->leftAnalog.x);
        oldx = cinfo->leftAnalog.x;
    }
#endif
}

/** \brief Map vertical movements of the left analog stick to a
 *         #TwoButtonJoystick
 *
 * The stick is not considered moved if it moves less than #ANALOG_DEAD_ZONE.
 *
 * \param[out] j Mapped joystick status
 */
static void mapAnalogStickVertical(const RuntimeControllerInfo* cinfo, TwoButtonJoystick* j) {
    j->up = cinfo->leftAnalog.y < -ANALOG_DEAD_ZONE;
    j->down = cinfo->leftAnalog.y > +ANALOG_DEAD_ZONE;

#ifdef ENABLE_SERIAL_DEBUG
    static int32_t oldy = -10000;
    if (abs(cinfo->leftAnalog.y - oldy) > 5) {
        mmlogd("L Analog Y = %d\n", cinfo->leftAnalog.y);
        oldy = cinfo->leftAnalog.y;
    }
#endif
}

/** \brief Map controller buttons to two-button joystick according to the
 *         standard mapping
 *
 * This is the default mapping and must be as "simple" as possible, as it will
 * be used when in CD32 mode. Some CD32 games seem to be reading B1/B2 in Atari
 * mode, and the rest of the buttons in CD32 mode. So if we map, for instance,
 * R1 to B1, when you press R1 to achieve Front Right, you'll also get B1, which
 * you don't probably want.
 *
 * \param[out] j Mapped joystick status
 */
static void mapJoystickNormal(const RuntimeControllerInfo* cinfo, TwoButtonJoystick* j) {
    // Use both analog axes
    mapAnalogStickHorizontal(cinfo, j);
    mapAnalogStickVertical(cinfo, j);

    // D-Pad is fully functional as well
    j->up |= buttonPressed(cinfo->buttonWord, BTN_PAD_UP);
    j->down |= buttonPressed(cinfo->buttonWord, BTN_PAD_DOWN);
    j->right |= buttonPressed(cinfo->buttonWord, BTN_PAD_RIGHT);
    j->left |= buttonPressed(cinfo->buttonWord, BTN_PAD_LEFT);

    // X is button 1
    j->b1 = buttonPressed(cinfo->buttonWord, BTN_X);

    // A is button 2
    j->b2 = buttonPressed(cinfo->buttonWord, BTN_A);
}

/** \brief Map controller buttons to two-button joystick according to Racing
 *         mapping 1
 *
 * \param[out] j Mapped joystick status
 */
static void mapJoystickRacing1(const RuntimeControllerInfo* cinfo, TwoButtonJoystick* j) {
    // Use analog's horizontal axis to steer, ignore vertical
    mapAnalogStickHorizontal(cinfo, j);

    // D-Pad L/R can also be used
    j->right |= buttonPressed(cinfo->buttonWord, BTN_PAD_RIGHT);
    j->left |= buttonPressed(cinfo->buttonWord, BTN_PAD_LEFT);

    // Use D-Pad Up/Button X to accelerate and Down/A to brake
    j->up = buttonPressed(cinfo->buttonWord, BTN_PAD_UP | BTN_X);
    j->down = buttonPressed(cinfo->buttonWord, BTN_PAD_DOWN | BTN_A);

    /* Games probably did not expect up + down at the same time, so when
     * braking, don't accelerate
     */
    if (j->down)
        j->up = false;

    // Button Y/Shoulder R/Trigger R are button 1
    j->b1 = buttonPressed(cinfo->buttonWord, BTN_Y | BTN_SHOULDER_R | BTN_TRIGGER_R);

    // Button B/Shoulder L/Trigger L are button 2
    j->b2 = buttonPressed(cinfo->buttonWord, BTN_B | BTN_SHOULDER_L | BTN_TRIGGER_L);
}

/** \brief Map controller buttons to two-button joystick according to Racing
 *         mapping 2
 *
 * \param[out] j Mapped joystick status
 */
static void mapJoystickRacing2(const RuntimeControllerInfo* cinfo, TwoButtonJoystick* j) {
    // Use analog's horizontal axis to steer, ignore vertical
    mapAnalogStickHorizontal(cinfo, j);

    // D-Pad L/R can also be used
    j->right |= buttonPressed(cinfo->buttonWord, BTN_PAD_RIGHT);
    j->left |= buttonPressed(cinfo->buttonWord, BTN_PAD_LEFT);

    // Use D-Pad Up/Shoulder R/Trigger R to accelerate and Down/A/Shoulder L/Trigger L to brake
    j->up = buttonPressed(cinfo->buttonWord, BTN_PAD_UP | BTN_SHOULDER_R | BTN_TRIGGER_R);
    j->down = buttonPressed(cinfo->buttonWord, BTN_PAD_DOWN | BTN_A | BTN_SHOULDER_L | BTN_TRIGGER_L);

    /* Games probably did not expect up + down at the same time, so when
     * braking, don't accelerate
     */
    if (j->down)
        j->up = false;

    // X is button 1
    j->b1 = buttonPressed(cinfo->buttonWord, BTN_X);

    // Y is button 2
    j->b2 = buttonPressed(cinfo->buttonWord, BTN_Y);
}

/** \brief Map controller buttons to two-button joystick according to
 *         Platform mapping
 *
 * \param[out] j Mapped joystick status
 */
static void mapJoystickPlatform(const RuntimeControllerInfo* cinfo, TwoButtonJoystick* j) {
    // Use horizontal analog axis fully, but only down on vertical
    mapAnalogStickHorizontal(cinfo, j);
    mapAnalogStickVertical(cinfo, j);

    // D-Pad is fully functional, and A doubles as up/jump
    j->up = buttonPressed(cinfo->buttonWord, BTN_PAD_UP | BTN_A);  // Note the '=', will override analog UP
    j->down |= buttonPressed(cinfo->buttonWord, BTN_PAD_DOWN);
    j->right |= buttonPressed(cinfo->buttonWord, BTN_PAD_RIGHT);
    j->left |= buttonPressed(cinfo->buttonWord, BTN_PAD_LEFT);

    // Button X/Shoulder R/Trigger R are button 1
    j->b1 = buttonPressed(cinfo->buttonWord, BTN_X | BTN_SHOULDER_R | BTN_TRIGGER_R);

    // Button Y/Shoulder L/Trigger L are button 2
    j->b2 = buttonPressed(cinfo->buttonWord, BTN_Y | BTN_SHOULDER_L | BTN_TRIGGER_L);
}

/** \brief Get number of set bits in the binary representation of a number
 *
 * All hail to Brian Kernighan.
 *
 * \param[in] n The number
 * \return The number of bits set
 */
static unsigned int countSetBits(int n) {
    unsigned int count = 0;

    while (n) {
        n &= n - 1;
        ++count;
    }

    return count;
}

/** \brief Check whether a button report contains a mappable button
 *
 * That means it contains a single button which is not \a SELECT neither one
 * from the D-Pad.
 *
 * \param[in] b The button to be checked
 * \return True if \b can be mapped, false otherwise
 */
static bool isButtonMappable(PadButtons b) {
    return countSetBits(b) == 1 && !buttonPressed(b, BTN_BACK) && !buttonPressed(b, BTN_PAD_UP) &&
           !buttonPressed(b, BTN_PAD_DOWN) && !buttonPressed(b, BTN_PAD_LEFT) && !buttonPressed(b, BTN_PAD_RIGHT);
}

/** \brief Merge two #TwoButtonJoystick's
 *
 * Every button that is pressed in either \a src or \a dest will end up pressed
 * in \a dest.
 *
 * \param[inout] dest Destination
 * \param[in] src Source
 */
static void mergeButtons(TwoButtonJoystick* dest, const TwoButtonJoystick* src) {
    /* This is what we need to do:
     * dest.up |= src.up;
     * dest.down |= src.down;
     * dest.left |= src.left;
     * dest.right |= src.right;
     * dest.b1 |= src.b1;
     * dest.b2 |= src.b2;
     *
     * And this is the way we're doing it to be faaaast and save flash:
     */
    uint8_t* bd = (uint8_t*)(dest);
    const uint8_t* sd = (const uint8_t*)(src);
    *bd |= *sd;
}

/** \brief Map PSX controller buttons to two-button joystick according to the
 *         current Custom mapping
 *
 * \param[out] j Mapped joystick status
 */
static void mapJoystickCustom(const RuntimeControllerInfo* cinfo, TwoButtonJoystick* j) {
    // Use both analog axes fully
    mapAnalogStickHorizontal(cinfo, j);
    mapAnalogStickVertical(cinfo, j);

    // D-Pad is fully functional as well
    j->up |= buttonPressed(cinfo->buttonWord, BTN_PAD_UP);
    j->down |= buttonPressed(cinfo->buttonWord, BTN_PAD_DOWN);
    j->right |= buttonPressed(cinfo->buttonWord, BTN_PAD_RIGHT);
    j->left |= buttonPressed(cinfo->buttonWord, BTN_PAD_LEFT);

    for (uint8_t i = 0; i < PAD_BUTTONS_NO; ++i) {
        PadButton button = (PadButton)(1 << i);
        if (isButtonMappable(button) && buttonPressed(cinfo->buttonWord, button)) {
            uint8_t buttonIdx = padButtonToIndex(button);
            mergeButtons(j, &(cinfo->currentCustomConfig->buttonMappings[buttonIdx]));
        }
    }
}

//! \name Helper functions
//! @{

static RuntimeControllerInfo* getControllerInstance(uni_hid_device_t* d) {
    return (RuntimeControllerInfo*)&d->platform_data[0];
}

static void setSeat(uni_hid_device_t* d, uni_gamepad_seat_t seat) {
    RuntimeControllerInfo* cinfo = getControllerInstance(d);
    cinfo->seat = seat;

    mmlogi("unijoysticle: device %s has new gamepad seat: %d\n", bd_addr_to_str(d->conn.btaddr), seat);

    if (d->report_parser.set_lightbar_color != NULL) {
        // First try with color LED (best experience)
        uint8_t red = 0;
        uint8_t green = 0;
        if (seat & 0x01)
            green = 0xff;
        if (seat & 0x02)
            red = 0xff;
        d->report_parser.set_lightbar_color(d, red, green, 0x00 /* blue*/);
    } else if (d->report_parser.set_player_leds != NULL) {
        // 2nd best option: set player LEDs
        d->report_parser.set_player_leds(d, seat);
    } else if (d->report_parser.play_dual_rumble != NULL) {
        // Finally, as last resort, rumble
        d->report_parser.play_dual_rumble(d, 0 /* delayed start ms */, 30 /* duration ms */, 0x80 /* weak magnitude */,
                                          0x40 /* strong magnitude */);
    }
}

/** \brief Flash the Mode LED
 *
 * \param[in] n Desired number of flashes
 */
static void flashLed(gpio_num_t pin, int n) {
    for (int i = 0; i < n; ++i) {
        gpio_set_level(pin, 1);
        vTaskDelay(pdMS_TO_TICKS(40));
        gpio_set_level(pin, 0);
        vTaskDelay(pdMS_TO_TICKS(80));
    }
}

/** \brief Check if the right analog stick has been moved
 *
 * The stick is not considered moved if it moves less than #ANALOG_DEAD_ZONE.
 *
 * \param[out] x Movement on the horizontal axis [-511 ... 511]
 * \param[out] y Movement on the vertical axis [-511 ... 511]
 * \return True if the stick is not in the center position, false otherwise
 */
static bool rightAnalogMoved(const RuntimeControllerInfo* cinfo, int16_t* x, int16_t* y) {
    bool ret = false;

    // Bluepad analog range is [-512, 511]
    uint16_t deltaRXabs = abs(cinfo->rightAnalog.x);
    if (deltaRXabs > ANALOG_DEAD_ZONE) {
        *x = cinfo->rightAnalog.x;
        if (*x == -512)
            *x = -511;
        ret = true;
    } else {
        *x = 0;
    }

    uint16_t deltaRYabs = abs(cinfo->rightAnalog.y);
    if (deltaRYabs > ANALOG_DEAD_ZONE) {
        *y = cinfo->rightAnalog.y;
        if (*y == -512)
            *y = -511;
        ret = true;
    } else {
        *y = 0;
    }

#ifdef ENABLE_SERIAL_DEBUG
    if (ret) {
        static int16_t oldx = -10000;
        if (*x != oldx) {
            mmlogd("R Analog X = %d\n", (int)*x);
            oldx = *x;
        }

        static int16_t oldy = -10000;
        if (*y != oldy) {
            mmlogd("R Analog Y = %d\n", (int)*y);
            oldy = *y;
        }
    }
#endif

    return ret;
}

/** \brief Dump a TwoButtonJoystick structure to serial
 *
 * This function is meant for debugging.
 *
 * \param[in] j Two-button joystick to be dumped
 */
static void dumpJoy(const TwoButtonJoystick* j) {
    if (j->up) {
        mmlogi("Up ");
    }
    if (j->down) {
        mmlogi("Down ");
    }
    if (j->left) {
        mmlogi("Left ");
    }
    if (j->right) {
        mmlogi("Right ");
    }
    if (j->b1) {
        mmlogi("B1 ");
    }
    if (j->b2) {
        mmlogi("B2");
    }
    mmlogi("\n");
}

/** \brief Update the output direction pins
 *
 * This functions updates the status of the 4 directional pins of the DB-9 port.
 * It does so after calling the current joystick mapping function, whose output
 * will also be exported in \a j.
 *
 * It also updates \a buttonsLive.
 *
 * \param[out] j Mapped joystick status
 */
static void handleJoystickDirections(RuntimeControllerInfo* cinfo, TwoButtonJoystick* j) {
    if (cinfo->joyPins != NULL) {
#ifdef ENABLE_SERIAL_DEBUG
        static TwoButtonJoystick oldJoy = {false, false, false, false, false, false};

        if (memcmp(j, &oldJoy, sizeof(TwoButtonJoystick)) != 0) {
            mmlogi("Sending to DB-9: ");
            dumpJoy(j);
            oldJoy = *j;
        }
#endif

        // Make mapped buttons affect the actual pins
        if (j->up) {
            buttonPress(cinfo->joyPins[PIN_NO_UP]);
        } else {
            buttonRelease(cinfo->joyPins[PIN_NO_UP]);
        }

        if (j->down) {
            buttonPress(cinfo->joyPins[PIN_NO_DOWN]);
        } else {
            buttonRelease(cinfo->joyPins[PIN_NO_DOWN]);
        }

        if (j->left) {
            buttonPress(cinfo->joyPins[PIN_NO_LEFT]);
        } else {
            buttonRelease(cinfo->joyPins[PIN_NO_LEFT]);
        }

        if (j->right) {
            buttonPress(cinfo->joyPins[PIN_NO_RIGHT]);
        } else {
            buttonRelease(cinfo->joyPins[PIN_NO_RIGHT]);
        }
    }

    /* Map buttons, working on a temporary variable to avoid the sampling
     * interrupt to happen while we are filling in button statuses and catch a
     * value that has not yet been fully populated.
     *
     * Note that 0 means pressed and that MSB must be 1 for the ID
     * sequence.
     */
    uint8_t buttonsTmp = 0xFF;

    // FIXME: No better place for this? It prevents us from keeping cinfo const
    if (buttonJustPressed(cinfo->buttonWord, cinfo->previousButtonWord, BTN_BACK)) {
        cinfo->useAlternativeCd32Mapping = !cinfo->useAlternativeCd32Mapping;
        flashLed(cinfo->ledPin, ((uint8_t)cinfo->useAlternativeCd32Mapping) + 1);  // Flash-saving hack
    }

    if (cinfo->useAlternativeCd32Mapping) {
        if (buttonPressed(cinfo->buttonWord, BTN_X)) {
            buttonsTmp &= ~BTN32_GREEN;
        }

        if (buttonPressed(cinfo->buttonWord, BTN_A)) {
            buttonsTmp &= ~BTN32_RED;
        }

        if (buttonPressed(cinfo->buttonWord, BTN_B)) {
            buttonsTmp &= ~BTN32_BLUE;
        }

        if (buttonPressed(cinfo->buttonWord, BTN_Y)) {
            buttonsTmp &= ~BTN32_YELLOW;
        }
    } else {
        if (buttonPressed(cinfo->buttonWord, BTN_Y)) {
            buttonsTmp &= ~BTN32_GREEN;
        }

        if (buttonPressed(cinfo->buttonWord, BTN_X)) {
            buttonsTmp &= ~BTN32_RED;
        }

        if (buttonPressed(cinfo->buttonWord, BTN_A)) {
            buttonsTmp &= ~BTN32_BLUE;
        }

        if (buttonPressed(cinfo->buttonWord, BTN_B)) {
            buttonsTmp &= ~BTN32_YELLOW;
        }
    }

    if (buttonPressed(cinfo->buttonWord, BTN_HOME)) {
        buttonsTmp &= ~BTN32_START;
    }

    if (buttonPressed(cinfo->buttonWord, BTN_SHOULDER_L | BTN_TRIGGER_L)) {
        buttonsTmp &= ~BTN32_FRONT_L;
    }

    if (buttonPressed(cinfo->buttonWord, BTN_SHOULDER_R | BTN_TRIGGER_R)) {
        buttonsTmp &= ~BTN32_FRONT_R;
    }

    // Atomic operation, interrupt either happens before or after this
    cinfo->buttonsLive = buttonsTmp;
}

/** \brief Update the output fire button pins when in joystick mode
 *
 * This functions updates the status of the 2 fire button pins of the DB-9 port.
 * It shall only be called when state is ST_JOYSTICK.
 *
 * \param[in] j Mapped joystick status, as returned by
 *              handleJoystickDirections().
 */
static void handleJoystickButtons(const RuntimeControllerInfo* cinfo, const TwoButtonJoystick* j) {
    /* If the interrupt that switches us to CD32 mode is
     * triggered while we are here we might end up setting pin states after
     * we should have relinquished control of the pins, so let's avoid this
     * disabling interrupts, we will handle them in a few microseconds.
     */
    taskDISABLE_INTERRUPTS();

    /* Ok, this breaks the state machine abstraction a bit, but we *have* to do
     * this check now, as the interrupt that makes us switch to ST_CD32 might
     * have occurred after this function was called but before we disabled
     * interrupts, and we absolutely have to avoid modifying pin
     * directions/states if the ISR has already been called.
     */
    if (cinfo->state == ST_JOYSTICK) {
        if (cinfo->joyPins != NULL) {
            if (j->b1) {
                buttonPress(cinfo->joyPins[PIN_NO_B1]);
            } else {
                buttonRelease(cinfo->joyPins[PIN_NO_B1]);
            }

#ifdef MMBOARD_C64_REV_F
            /* On the original C64 version of the Unijoysticle 2 board, only
             * port 1 has pin 9 connected to the MCU
             */
            if (cinfo->seat == GAMEPAD_SEAT_A) {
#endif
                if (!cinfo->c64Mode) {
                    if (j->b2) {
                        buttonPress(cinfo->joyPins[PIN_NO_B2]);
                    } else {
                        buttonRelease(cinfo->joyPins[PIN_NO_B2]);
                    }
                } else {
                    // C64 works the opposite way
                    if (j->b2) {
                        buttonRelease(cinfo->joyPins[PIN_NO_B2]);
                    } else {
                        buttonPress(cinfo->joyPins[PIN_NO_B2]);
                    }
                }
#ifdef MMBOARD_C64_REV_F
            }
#endif
        }
    }

    taskENABLE_INTERRUPTS();
}

/** \brief Update the primary output fire button pins when in CD32 mode
 *
 * This functions updates the status of the 2 main fire buttons inbetween two
 * consecutive CD32-style readings. It might seem counter-intuitive, but many
 * games (including lowlevel.library, I think!) read the state of the two main
 * buttons this way, rather than extracting them from the full 7-button reply,
 * don't ask me why. See \a Banshee, for instance.
 *
 * So we need a different function that does not perform button mapping as when
 * in 2-button joystick mode, but that rather uses the current CD32 mapping.
 * This means it shall also be coherent with what onPadModeChange() is doing
 * when PIN_PADMODE goes high.
 *
 * Of course, this function shall only be called when state is ST_CD32.
 */
static void handleJoystickButtonsTemp(const RuntimeControllerInfo* cinfo) {
    // Use the same logic as in handleJoystickButtons()
    taskDISABLE_INTERRUPTS();

    if (cinfo->state == ST_JOYSTICK_TEMP) {
        if (cinfo->joyPins != NULL) {
            /* Relying on buttonsLive guarantees we do the right thing even when
             * useAlternativeCd32Mapping is true
             */
            if (!(cinfo->buttonsLive & BTN32_RED)) {
                buttonPress(cinfo->joyPins[PIN_NO_B1]);
            } else {
                buttonRelease(cinfo->joyPins[PIN_NO_B1]);
            }

            if (!(cinfo->buttonsLive & BTN32_BLUE)) {
                buttonPress(cinfo->joyPins[PIN_NO_B2]);
            } else {
                buttonRelease(cinfo->joyPins[PIN_NO_B2]);
            }
        }
    }

    taskENABLE_INTERRUPTS();
}

/** \brief Update all output pins for Mouse mode
 *
 * This function updates all the output pins as necessary to emulate mouse
 * movements and button presses. It shall only be called when state is ST_MOUSE.
 */
static void handleMouse(const RuntimeControllerInfo* cinfo) {
    static unsigned long tx = 0, ty = 0;

    if (cinfo->mousePins != NULL) {
        int16_t x, y;
        rightAnalogMoved(cinfo, &x, &y);

        // Horizontal axis
        if (x != 0) {
            mmlogd("x = %d", (int)x);

            unsigned int period = map(abs(x), ANALOG_DEAD_ZONE, 512, MOUSE_SLOW_DELTA, MOUSE_FAST_DELTA);
            mmlogd(" --> period = %u\n", period);

            if (x > 0) {
                // Right
                if (millis() - tx >= period) {
                    gpio_set_level(cinfo->mousePins[PIN_NO_RIGHT], !gpio_get_level(cinfo->mousePins[PIN_NO_RIGHT]));
                    tx = millis();
                }

                if (millis() - tx >= period / 2) {
                    gpio_set_level(cinfo->mousePins[PIN_NO_DOWN], !gpio_get_level(cinfo->mousePins[PIN_NO_RIGHT]));
                }
            } else {
                // Left
                if (millis() - tx >= period) {
                    gpio_set_level(cinfo->mousePins[PIN_NO_DOWN], !gpio_get_level(cinfo->mousePins[PIN_NO_DOWN]));
                    tx = millis();
                }

                if (millis() - tx >= period / 2) {
                    gpio_set_level(cinfo->mousePins[PIN_NO_RIGHT], !gpio_get_level(cinfo->mousePins[PIN_NO_DOWN]));
                }
            }
        }

        // Vertical axis
        if (y != 0) {
            mmlogd("y = %d", (int)y);

            unsigned int period = map(abs(y), ANALOG_DEAD_ZONE, 512, MOUSE_SLOW_DELTA, MOUSE_FAST_DELTA);
            mmlogd(" --> period = %u\n", period);

            if (y > 0) {
                // Up
                if (millis() - ty >= period) {
                    gpio_set_level(cinfo->mousePins[PIN_NO_LEFT], !gpio_get_level(cinfo->mousePins[PIN_NO_LEFT]));
                    ty = millis();
                }

                if (millis() - ty >= period / 2) {
                    gpio_set_level(cinfo->mousePins[PIN_NO_UP], !gpio_get_level(cinfo->mousePins[PIN_NO_LEFT]));
                }
            } else {
                // Down
                if (millis() - ty >= period) {
                    gpio_set_level(cinfo->mousePins[PIN_NO_UP], !gpio_get_level(cinfo->mousePins[PIN_NO_UP]));
                    ty = millis();
                }

                if (millis() - ty >= period / 2) {
                    gpio_set_level(cinfo->mousePins[PIN_NO_LEFT], !gpio_get_level(cinfo->mousePins[PIN_NO_UP]));
                }
            }
        }

        /* Buttons - This won't happen while in CD32 mode, so there's no need to
         * disable interrupts
         */

        // Thumb L is Left button
        if (buttonPressed(cinfo->buttonWord, BTN_THUMB_L)) {
            buttonPress(cinfo->mousePins[PIN_NO_B1]);
        } else {
            buttonRelease(cinfo->mousePins[PIN_NO_B1]);
        }

        // Thumb R is Right button
        if (buttonPressed(cinfo->buttonWord, BTN_THUMB_R)) {
            buttonPress(cinfo->mousePins[PIN_NO_B2]);
        } else {
            buttonRelease(cinfo->mousePins[PIN_NO_B2]);
        }
    }
}

/** \brief Translate a button combo to what would be sent to the DB-9 port
 *
 * This is used during programming mode to enter the combo that should be sent
 * whenever a mapped button is pressed.
 *
 * \param[in] buttons PSX controller button combo
 * \param[out] j Two-button joystick configuration corresponding to input
 * \return True if the output contains at least one pressed button
 */
static bool psxButton2Amiga(const PadButtons buttons, TwoButtonJoystick* j) {
    memset(j, 0x00, sizeof(TwoButtonJoystick));

    j->up = buttonPressed(buttons, BTN_PAD_UP);
    j->down = buttonPressed(buttons, BTN_PAD_DOWN);
    j->left = buttonPressed(buttons, BTN_PAD_LEFT);
    j->right = buttonPressed(buttons, BTN_PAD_RIGHT);
    j->b1 = buttonPressed(buttons, BTN_X);
    j->b2 = buttonPressed(buttons, BTN_A);

    return *((uint8_t*)j);
}

/** \brief Check if a PSX button is programmable
 *
 * \param[in] b The button to be checked
 * \return True if \b is programmable, false otherwise
 */
static bool isButtonProgrammable(const PadButtons b) {
    return buttonPressed(b, BTN_SHOULDER_L) || buttonPressed(b, BTN_TRIGGER_L) || buttonPressed(b, BTN_SHOULDER_R) ||
           buttonPressed(b, BTN_TRIGGER_R);
}

//! \brief Handle the internal state machine
/*
 * Virtual Gamepad layout
 *          Y
 *          ^
 *      X<-   ->B
 *          v
 *          A
 */
static void stateMachine(RuntimeControllerInfo* cinfo) {
    PadButtons buttons = BTN_NONE;
    TwoButtonJoystick j;  // = {false, false, false, false, false, false};

    memset(&j, 0x00, sizeof(TwoButtonJoystick));

    if (cinfo == NULL) {
        mmloge("ERROR: stateMachine() called with NULL cinfo\n");
        return;
    }

    switch (cinfo->state) {
        case ST_FIRST_READ:
            /* Time to roll! We'll default to joystick mode, so let's
             * set everything up now. Let's abuse the mouseToJoystick()
             * function, hope she won't mind :).
             */
            enableCD32Trigger(cinfo);
            cinfo->state = ST_JOYSTICK;
            break;

            /**********************************************************************
             * MAIN MODES
             **********************************************************************/
        case ST_JOYSTICK:
            if (buttonPressed(cinfo->buttonWord, BTN_BACK)) {
                cinfo->state = ST_SELECT_HELD;
            } else {
                // Handle normal joystick movements
                cinfo->joyMappingFunc(cinfo, &j);
                handleJoystickDirections(cinfo, &j);
                handleJoystickButtons(cinfo, &j);
                handleMouse(cinfo);
            }
            break;
        case ST_CD32:
            cinfo->joyMappingFunc(cinfo, &j);
            handleJoystickDirections(cinfo, &j);
            handleMouse(cinfo);
            cinfo->stateEnteredTime = 0;
            break;
        case ST_JOYSTICK_TEMP:
            cinfo->joyMappingFunc(cinfo, &j);
            handleJoystickDirections(cinfo, &j);
            handleJoystickButtonsTemp(cinfo);
            handleMouse(cinfo);

            if (cinfo->stateEnteredTime == 0) {
                // ControllerState was just entered
                cinfo->stateEnteredTime = millis();
            } else if (millis() - cinfo->stateEnteredTime > TIMEOUT_CD32_MODE) {
                // CD32 mode was exited once for all
                mmlogd("CD32 -> Joystick\n");
                cinfo->stateEnteredTime = 0;
                cinfo->state = ST_JOYSTICK;
            }
            break;

            /**********************************************************************
             * SELECT MAPPING/SWITCH TO PROGRAMMING MODE
             **********************************************************************/
        case ST_SELECT_HELD:
            if (!buttonPressed(cinfo->buttonWord, BTN_BACK)) {
                // Select was released
                cinfo->state = ST_JOYSTICK;
            } else if (buttonPressed(cinfo->buttonWord, BTN_X)) {
                cinfo->selectComboButton = BTN_X;
                cinfo->state = ST_SELECT_AND_BTN_HELD;
            } else if (buttonPressed(cinfo->buttonWord, BTN_Y)) {
                cinfo->selectComboButton = BTN_Y;
                cinfo->state = ST_SELECT_AND_BTN_HELD;
            } else if (buttonPressed(cinfo->buttonWord, BTN_B)) {
                cinfo->selectComboButton = BTN_B;
                cinfo->state = ST_SELECT_AND_BTN_HELD;
            } else if (buttonPressed(cinfo->buttonWord, BTN_A)) {
                cinfo->selectComboButton = BTN_A;
                cinfo->state = ST_SELECT_AND_BTN_HELD;
            } else if (buttonPressed(cinfo->buttonWord, BTN_SHOULDER_L)) {
                cinfo->state = ST_SELECT_AND_BTN_HELD;
            } else if (buttonPressed(cinfo->buttonWord, BTN_SHOULDER_R)) {
                cinfo->selectComboButton = BTN_SHOULDER_R;
                cinfo->state = ST_SELECT_AND_BTN_HELD;
            } else if (buttonPressed(cinfo->buttonWord, BTN_TRIGGER_L)) {
                cinfo->selectComboButton = BTN_TRIGGER_L;
                cinfo->state = ST_SELECT_AND_BTN_HELD;
            } else if (buttonPressed(cinfo->buttonWord, BTN_TRIGGER_R)) {
                cinfo->selectComboButton = BTN_TRIGGER_R;
                cinfo->state = ST_SELECT_AND_BTN_HELD;
            } else if (buttonPressed(cinfo->buttonWord, BTN_HOME)) {
                cinfo->selectComboButton = BTN_HOME;
                cinfo->state = ST_SELECT_AND_BTN_HELD;
            }
            break;
        case ST_SELECT_AND_BTN_HELD:
            if (cinfo->stateEnteredTime == 0) {
                // State was just entered
                cinfo->stateEnteredTime = millis();
            } else if (isButtonProgrammable(cinfo->selectComboButton) &&
                       millis() - cinfo->stateEnteredTime > TIMEOUT_PROGRAMMING_MODE) {
                // Combo kept pressed, enter programming mode
                mmlogi("Entering programming mode for %s\n", getButtonName(cinfo->selectComboButton));
                cinfo->stateEnteredTime = 0;
                cinfo->state = ST_WAIT_SELECT_RELEASE;
            } else if (!buttonPressed(cinfo->buttonWord, BTN_BACK) ||
                       !buttonPressed(cinfo->buttonWord, cinfo->selectComboButton)) {
                // Combo released, switch to desired mapping
                cinfo->stateEnteredTime = 0;
                cinfo->state = ST_ENABLE_MAPPING;
            }
            break;
        case ST_ENABLE_MAPPING:
            // Change button mapping
            switch (cinfo->selectComboButton) {
                case BTN_X:
                    mmlogi("Setting normal mapping\n");
                    cinfo->joyMappingFunc = mapJoystickNormal;
                    flashLed(cinfo->ledPin, JMAP_NORMAL);
                    break;
                case BTN_Y:
                    mmlogi("Setting Racing1 mapping\n");
                    cinfo->joyMappingFunc = mapJoystickRacing1;
                    flashLed(cinfo->ledPin, JMAP_RACING1);
                    break;
                case BTN_B:
                    mmlogi("Setting Racing2 mapping\n");
                    cinfo->joyMappingFunc = mapJoystickRacing2;
                    flashLed(cinfo->ledPin, JMAP_RACING2);
                    break;
                case BTN_A:
                    mmlogi("Setting Platform mapping\n");
                    cinfo->joyMappingFunc = mapJoystickPlatform;
                    flashLed(cinfo->ledPin, JMAP_PLATFORM);
                    break;
                case BTN_SHOULDER_L:
                case BTN_SHOULDER_R:
                case BTN_TRIGGER_L:
                case BTN_TRIGGER_R: {
                    uint8_t configIdx = padButtonToIndex(cinfo->selectComboButton);
                    if (configIdx < PAD_BUTTONS_NO) {
                        mmlogi("Setting Custom mapping for controllerConfig %u\n", (unsigned int)configIdx);
                        cinfo->currentCustomConfig = &(controllerConfigs[configIdx]);
                        cinfo->joyMappingFunc = mapJoystickCustom;
                        flashLed(cinfo->ledPin, JMAP_CUSTOM);
                    } else {
                        /* Something went wrong, just ignore it and pretend
                         * nothing ever happened
                         */
                    }
                    break;
                }
                case BTN_HOME:
                    if (cinfo->c64Mode) {
                        flashLed(cinfo->ledPin, 2);
                    } else {
                        flashLed(cinfo->ledPin, 1);
                    }
                    cinfo->c64Mode = !cinfo->c64Mode;
                    break;
                default:
                    // Shouldn't be reached
                    break;
            }
            cinfo->selectComboButton = BTN_NONE;
            cinfo->state = ST_JOYSTICK;
            break;

            /**********************************************************************
             * PROGRAMMING
             **********************************************************************/
        case ST_WAIT_SELECT_RELEASE:
            if (!buttonPressed(cinfo->buttonWord, BTN_BACK)) {
                cinfo->state = ST_WAIT_BUTTON_PRESS;
            }
            break;
        case ST_WAIT_BUTTON_PRESS:
            if (buttonPressed(cinfo->buttonWord, BTN_BACK)) {
                // Exit programming mode
                mmlogi("Leaving programming mode\n");
                saveConfigurations();  // No need to check for changes as this uses EEPROM.update()
                cinfo->state = ST_WAIT_SELECT_RELEASE_FOR_EXIT;
            } else {
                buttons = debounceButtons(cinfo->buttonWord, cinfo->previousButtonWord, DEBOUNCE_TIME_BUTTON);
                if (isButtonMappable(buttons)) {
                    // Exactly one key pressed, go on
                    cinfo->programmedButton = (PadButton)buttons;
                    mmlogi("Programming button %s\n", getButtonName(buttons));
                    flashLed(cinfo->ledPin, 3);
                    cinfo->state = ST_WAIT_BUTTON_RELEASE;
                }
            }
            break;
        case ST_WAIT_BUTTON_RELEASE:
            if (noButtonPressed(cinfo->buttonWord)) {
                cinfo->state = ST_WAIT_COMBO_PRESS;
            }
            break;
        case ST_WAIT_COMBO_PRESS:
            buttons = debounceButtons(cinfo->buttonWord, cinfo->previousButtonWord, DEBOUNCE_TIME_COMBO);
            if (buttons != BTN_NONE && psxButton2Amiga(buttons, &j)) {
                mmlogi("Programmed to ");
                dumpJoy(&j);

                // First look up the config the mapping shall be saved to
                uint8_t configIdx = padButtonToIndex(cinfo->selectComboButton);
                if (configIdx < PAD_BUTTONS_NO) {
                    mmlogi("Storing to controllerConfig %u\n", (unsigned int)configIdx);

                    ControllerConfiguration* config = &controllerConfigs[configIdx];

                    // Then look up the mapping according to the programmed button
                    uint8_t buttonIdx = padButtonToIndex(cinfo->programmedButton);
                    config->buttonMappings[buttonIdx] = j;
                }

                cinfo->programmedButton = BTN_NONE;
                flashLed(cinfo->ledPin, 5);
                cinfo->state = ST_WAIT_COMBO_RELEASE;
            }
            break;
        case ST_WAIT_COMBO_RELEASE:
            if (noButtonPressed(cinfo->buttonWord)) {
                cinfo->state = ST_WAIT_BUTTON_PRESS;
            }
            break;
        case ST_WAIT_SELECT_RELEASE_FOR_EXIT:
            if (!buttonPressed(cinfo->buttonWord, BTN_BACK)) {
                cinfo->state = ST_JOYSTICK;
            }
            break;
    }

    // Save current Button Word for next call
    cinfo->previousButtonWord = cinfo->buttonWord;
}

static void gpio_isr_handler_button(void* arg) {
    mmlogd("Button ISR running on core %d\n", xPortGetCoreID());

    // Button released?
    if (gpio_get_level(GPIO_PUSH_BUTTON)) {
        //~ g_last_time_pressed_us = esp_timer_get_time ();
        mmlogi("SWAP Button released\n");
        return;
    }

    // Button pressed!
    mmlogi("SWAP Button pressed\n");
    //~ BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    //~ xEventGroupSetBitsFromISR (g_event_group, EVENT_BIT_BUTTON,
    //~ &xHigherPriorityTaskWoken);
    //~ if (xHigherPriorityTaskWoken == pdTRUE)
    //~ portYIELD_FROM_ISR ();
}

//! @}

#ifdef ENABLE_CD32_SUPPORT

//! \name Interrupt handlers for CD32 mode
//! @{

/** \brief ISR servicing rising edges on #pins_port[PIN_NO_B1]
 *
 * Called on clock pin rising, this function shall shift out next bit.
 */
static void onClockEdge(void* arg) {
    RuntimeControllerInfo* cinfo = (RuntimeControllerInfo*)arg;

#ifdef ENABLE_INSTRUMENTATION
    gpio_set_level(PIN_INTERRUPT_TIMING, 1);
#endif

    if (!(cinfo->isrButtons & 0x01)) {
        gpio_set_level(cinfo->joyPins[PIN_NO_B2], 1);
    } else {
        gpio_set_level(cinfo->joyPins[PIN_NO_B2], 0);
    }

    cinfo->isrButtons >>= 1U; /* Again, non-existing button 10 will be
                               * reported as pressed for the ID sequence
                               */

#ifdef ENABLE_INSTRUMENTATION
    gpio_set_level(PIN_INTERRUPT_TIMING, 0);
#endif
}

/** \brief ISR servicing edges on #pins_port[PIN_NO_MODE]
 *
 * Called on pins_port[PIN_NO_MODE] changing, this function shall set things up
 * for CD32 mode, sample buttons and shift out the first bit on FALLING edges,
 * and restore Atari-style signals on RISING edges.
 */
static void onPadModeChange(void* arg) {
    RuntimeControllerInfo* cinfo = (RuntimeControllerInfo*)arg;

    if (gpio_get_level(cinfo->joyPins[PIN_NO_MODE]) == 0) {
        // Switch to CD32 mode
#ifdef ENABLE_INSTRUMENTATION
        gpio_set_level(PIN_CD32MODE, 0);
#endif
        // Output status of first button as soon as possible
        if (!(cinfo->buttonsLive & 0x01)) {
            gpio_set_level(cinfo->joyPins[PIN_NO_B2], 1);
        } else {
            gpio_set_level(cinfo->joyPins[PIN_NO_B2], 0);
        }

        /* Disable output on clock pin. No need to rush here, as when the CD32
         * drives it high, it's doing so open-collector-style as well.
         *
         * Remember there's an inverter between us and the Amiga!
         */
        gpio_set_level(cinfo->joyPins[PIN_NO_B1], 0);

        /* Sample input values, they will be shifted out on subsequent clock
         * inputs.
         *
         * At this point MSB must be 1 for ID sequence. Then it will be zeroed
         * by the shift. This will report non-existing buttons 8 as released and
         * 9 as pressed as required by the ID sequence.
         */
        cinfo->isrButtons = cinfo->buttonsLive >> 1U;

        // Enable interrupt on clock edges
        ESP_ERROR_CHECK(gpio_isr_handler_add(cinfo->joyPins[PIN_NO_CLOCK], onClockEdge, (void*)cinfo));

        // Set state to ST_CD32
        if (cinfo->state != ST_CD32 && cinfo->state != ST_JOYSTICK_TEMP) {
            mmlogd("Joystick -> CD32\n");
        }
        cinfo->stateEnteredTime = 0;
        cinfo->state = ST_CD32;
#ifdef ENABLE_INSTRUMENTATION
        gpio_set_level(PIN_CD32MODE, 1);
        gpio_set_level(PIN_CD32MODE, 0);
#endif
    } else {
#ifdef ENABLE_INSTRUMENTATION
        gpio_set_level(PIN_CD32MODE, 1);
        gpio_set_level(PIN_CD32MODE, 0);
#endif

        /* Set pin directions and set levels according to buttons, as waiting
         * for the main loop to do it takes too much time (= a few ms), for some
         * reason
         */
        if (!(cinfo->buttonsLive & BTN32_RED)) {
            buttonPress(cinfo->joyPins[PIN_NO_B1]);
        } else {
            buttonRelease(cinfo->joyPins[PIN_NO_B1]);
        }

        if (!(cinfo->buttonsLive & BTN32_BLUE)) {
            buttonPress(cinfo->joyPins[PIN_NO_B2]);
        } else {
            buttonRelease(cinfo->joyPins[PIN_NO_B2]);
        }

        // Disable interrupt on clock edges
        gpio_isr_handler_remove(cinfo->joyPins[PIN_NO_CLOCK]);

        // Set state to ST_JOYSTICK_TEMP
        cinfo->state = ST_JOYSTICK_TEMP;

#ifdef ENABLE_INSTRUMENTATION
        gpio_set_level(PIN_CD32MODE, 1);
#endif
    }
}

//! @}

#endif

static uni_hid_device_t* getControllerForSeat(const uni_gamepad_seat_t seat) {
    uni_hid_device_t* ret = NULL;

    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES && ret == NULL; i++) {
        uni_hid_device_t* dev = uni_hid_device_get_instance_for_idx(i);
        if (dev && bd_addr_cmp(dev->conn.btaddr, zero_addr) != 0) {
            RuntimeControllerInfo* cinfo = getControllerInstance(dev);
            if (cinfo && cinfo->seat == seat) {
                ret = dev;
            }
        }
    }

    return ret;
}

/** \brief Update leds
 *
 * We have a separate function for this as several machine states share the same
 * led state.
 */
static void updateLeds() {
    // LOL, how crap! :X
    for (int i = 0; i < 2; ++i) {
        uni_gamepad_seat_t seat = GAMEPAD_SEAT_A;
        gpio_num_t pin = PIN_LED_P1;

        if (i == 1) {
            seat = GAMEPAD_SEAT_B;
            pin = PIN_LED_P2;
        }

        uni_hid_device_t* dev = getControllerForSeat(seat);
        RuntimeControllerInfo* cinfo;
        if (dev && (cinfo = getControllerInstance(dev))) {
            switch (cinfo->state) {
                case ST_FIRST_READ:
                case ST_WAIT_SELECT_RELEASE_FOR_EXIT:
                case ST_JOYSTICK:
                case ST_SELECT_HELD:
                case ST_SELECT_AND_BTN_HELD:
                case ST_ENABLE_MAPPING:
                case ST_CD32:
                case ST_JOYSTICK_TEMP:
                    // Led lit up steadily
                    gpio_set_level(cinfo->ledPin, 1);
                    break;
                case ST_WAIT_SELECT_RELEASE:
                case ST_WAIT_BUTTON_PRESS:
                case ST_WAIT_BUTTON_RELEASE:
                case ST_WAIT_COMBO_PRESS:
                case ST_WAIT_COMBO_RELEASE:
                    // Programming mode, blink fast
                    gpio_set_level(cinfo->ledPin, (millis() / 250) % 2 == 0);
                    break;
                default:
                    // WTF?! Blink fast... er!
                    gpio_set_level(cinfo->ledPin, (millis() / 100) % 2 == 0);
                    break;
            }
        } else {
            gpio_set_level(pin, 0);
        }
    }
}

static void loopCore0(void* arg) {
    RuntimeControllerInfo* cinfo = NULL;

    mmlogi("loopCore0() running on core %d\n", xPortGetCoreID());

    const TickType_t timeout = pdMS_TO_TICKS(10);
    while (true) {
        if (xQueueReceive(controllerUpdateQueue, &cinfo, timeout)) {
            // Process new data from controller
            stateMachine(cinfo);
        } else {
            /* Force the state machine to run even if no update was received,
             * since some transitions are time-driven
             */
            for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
                uni_hid_device_t* dev = uni_hid_device_get_instance_for_idx(i);
                cinfo = getControllerInstance(dev);
                if (cinfo->seat == GAMEPAD_SEAT_A || cinfo->seat == GAMEPAD_SEAT_B) {
                    stateMachine(cinfo);
                }
            }
        }

        updateLeds();
    }
}

// This task will run on core 1
static void loopCore1(void* arg) {
    gpio_config_t io_conf;

    mmlogi("loopCore1() running on core %d\n", xPortGetCoreID());

    /* NOTE: We need to do this GPIO configuration here so that the
     * corresponding interrupts are executed on core 1. Please see
     * https://esp32.com/viewtopic.php?t=13432 for more info.
     */

    /* We're going to need interrupts below
     *
     * NOTE: Only do this ONCE!!!
     */
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

#ifdef ENABLE_CD32_SUPPORT
    // Inputs for CD32 mode

    // This pin tells us when to toggle in/out of CD32 mode
    io_conf.intr_type = GPIO_INTR_ANYEDGE;  // On both rising and falling edges
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pin_bit_mask = (1ULL << PINS_PORT_A[PIN_NO_MODE]) | (1ULL << PINS_PORT_B[PIN_NO_MODE]);
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    io_conf.intr_type = GPIO_INTR_POSEDGE;  // Only on rising edges
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pin_bit_mask = (1ULL << PINS_PORT_A[PIN_NO_CLOCK]) | (1ULL << PINS_PORT_B[PIN_NO_CLOCK]);
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Interrupts are not activated here, preparation is enough :)
#endif

    // Setup GPIO for SWAP button
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pin_bit_mask = (1ULL << GPIO_PUSH_BUTTON);
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Check for factory reset
    unsigned long startPress = millis();
    if (gpio_get_level(GPIO_PUSH_BUTTON) == 0) {
        mmlogi("SWAP button pressed at power-up, starting factory reset\n");
        while (gpio_get_level(GPIO_PUSH_BUTTON) == 0) {
            if (millis() - startPress < 3000UL) {
                gpio_set_level(PIN_LED_P1, (millis() / 333) % 2 == 0);
            } else if (millis() - startPress < 5000UL) {
                gpio_set_level(PIN_LED_P1, (millis() / 80) % 2 == 0);
            } else {
                // OK, user has convinced us to actually perform the reset
                mmlogi("Performing factory reset\n");
                gpio_set_level(PIN_LED_P1, 1);
                clearConfigurations();
                saveConfigurations();
                uni_bt_del_keys_safe();  // Also delete BT keys
                while (gpio_get_level(GPIO_PUSH_BUTTON) == 0) {
                    vTaskDelay(10);
                }
            }

            // Avoid triggering the task watchdog
            vTaskDelay(1);
        }

        if (millis() - startPress <= 5000UL) {
            mmlogi("Factory reset aborted\n");
        }
    }

    // OK, we can enable the button interrupt now
    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_PUSH_BUTTON, gpio_isr_handler_button, (void*)GPIO_PUSH_BUTTON));

    // Actual task loop
    while (true) {
#ifdef ENABLE_CD32_SUPPORT
        RuntimeControllerInfo* cinfo;
#endif

        EventBits_t uxBits = xEventGroupWaitBits(evGrpCd32,
                                                 (EVENT_ENABLE_CD32_SEAT_A | EVENT_DISABLE_CD32_SEAT_A |
                                                  EVENT_ENABLE_CD32_SEAT_B | EVENT_DISABLE_CD32_SEAT_B),
                                                 pdTRUE, pdFALSE, portMAX_DELAY);

        if (uxBits & EVENT_ENABLE_CD32_SEAT_A) {
#ifdef ENABLE_CD32_SUPPORT
            // Enable interrupt watching for changes of the MODE pin of the Joy Port
            mmlogi("Enabling CD32 trigger for Seat A on core %d\n", xPortGetCoreID());
            uni_hid_device_t* dev = getControllerForSeat(GAMEPAD_SEAT_A);
            if (dev && (cinfo = getControllerInstance(dev))) {
                ESP_ERROR_CHECK(gpio_isr_handler_add(cinfo->joyPins[PIN_NO_MODE], onPadModeChange, (void*)cinfo));
            }
#endif
        }

        if (uxBits & EVENT_DISABLE_CD32_SEAT_A) {
#ifdef ENABLE_CD32_SUPPORT
            // Disable both interrupts, as this might happen halfway during a shift
            mmlogi("Disabling CD32 trigger for Seat A on core %d\n", xPortGetCoreID());
            uni_hid_device_t* dev = getControllerForSeat(GAMEPAD_SEAT_A);
            if (dev && (cinfo = getControllerInstance(dev))) {
                //~ taskDISABLE_INTERRUPTS ();
                gpio_isr_handler_remove(cinfo->joyPins[PIN_NO_CLOCK]);
                gpio_isr_handler_remove(cinfo->joyPins[PIN_NO_MODE]);
                //~ taskENABLE_INTERRUPTS ();
            }
#endif
        }

        if (uxBits & EVENT_ENABLE_CD32_SEAT_B) {
#ifdef ENABLE_CD32_SUPPORT
            mmlogi("Enabling CD32 trigger for Seat B on core %d\n", xPortGetCoreID());
            uni_hid_device_t* dev = getControllerForSeat(GAMEPAD_SEAT_B);
            if (dev && (cinfo = getControllerInstance(dev))) {
                ESP_ERROR_CHECK(gpio_isr_handler_add(cinfo->joyPins[PIN_NO_MODE], onPadModeChange, (void*)cinfo));
            }
#endif
        }

        if (uxBits & EVENT_DISABLE_CD32_SEAT_B) {
#ifdef ENABLE_CD32_SUPPORT
            // Disable both interrupts, as this might happen halfway during a shift
            mmlogi("Disabling CD32 trigger for Seat B on core %d\n", xPortGetCoreID());
            uni_hid_device_t* dev = getControllerForSeat(GAMEPAD_SEAT_B);
            if (dev && (cinfo = getControllerInstance(dev))) {
                //~ taskDISABLE_INTERRUPTS ();
                gpio_isr_handler_remove(cinfo->joyPins[PIN_NO_CLOCK]);
                gpio_isr_handler_remove(cinfo->joyPins[PIN_NO_MODE]);
                //~ taskENABLE_INTERRUPTS ();
            }
#endif
        }
    }
}

//! \name Platform implementation overrides
//! @{

static void mightymiggy_init(int argc, const char** argv) {
    UNUSED(argc);
    UNUSED(argv);

    // Prepare leds
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pin_bit_mask = (1ULL << PIN_LED_P2) | (1ULL << PIN_LED_P1);
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Initialize Non-Volatile Storage (NVS)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        mmlogi("Erasing flash memory\n");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Load custom mappings from EEPROM, this will also initialize them if
     * EEPROM data is invalid
     */
    loadConfigurations();

#ifdef ENABLE_INSTRUMENTATION
    // Prepare pins for instrumentation
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pin_bit_mask = (1ULL << PIN_INTERRUPT_TIMING) | (1ULL << PIN_CD32MODE);
    ESP_ERROR_CHECK(gpio_config(&io_conf));
#endif

    // Init Adapter state machine
    adapterState = AST_IDLE;

    // Init Port GPIOs - All Outputs
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //~ io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT;  // We need to be able to read the pins in handleMouse()
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;

    // Port A
    io_conf.pin_bit_mask = ((1ULL << PIN_A_UP) | (1ULL << PIN_A_DOWN) | (1ULL << PIN_A_LEFT) | (1ULL << PIN_A_RIGHT) |
                            (1ULL << PIN_A_FIRE) | (1ULL << PIN_A_FIRE2));

    // Port B
    io_conf.pin_bit_mask |= ((1ULL << PIN_B_UP) | (1ULL << PIN_B_DOWN) | (1ULL << PIN_B_LEFT) | (1ULL << PIN_B_RIGHT) |
                             (1ULL << PIN_B_FIRE) | (1ULL << PIN_B_FIRE2));

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Release all buttons
    for (int i = 0; i < PINS_PER_PORT; i++) {
        buttonRelease(PINS_PORT_A[i]);
        buttonRelease(PINS_PORT_B[i]);
    }

    // Create task stuff
    controllerUpdateQueue = xQueueCreate(3, sizeof(RuntimeControllerInfo*));
    xTaskCreatePinnedToCore(loopCore0, "loopCore0", 2048, NULL, 10, NULL, 0);

    // This other task sets up GPIO interrupts before starting its loop
    evGrpCd32 = xEventGroupCreate();
    xTaskCreatePinnedToCore(loopCore1, "loopCore1", 2048, NULL, 5, NULL, 1);

    // Blink to signal we're ready to roll!
    for (int i = 0; i < 3; i++) {
        gpio_set_level(PIN_LED_P2, 1);
        gpio_set_level(PIN_LED_P1, 0);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        gpio_set_level(PIN_LED_P2, 0);
        gpio_set_level(PIN_LED_P1, 1);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    gpio_set_level(PIN_LED_P1, 0);
    vTaskDelay(200 / portTICK_PERIOD_MS);
}

static void mightymiggy_on_init_complete(void) {
    // Start Scanning
    uni_bt_enable_new_connections_safe(true);

    // Hi-released, Low-pressed
    bool pushed = !gpio_get_level(GPIO_PUSH_BUTTON);
    if (pushed)
        uni_bt_del_keys_safe();
    else
        uni_bt_list_keys_safe();
}

static void mightymiggy_on_device_connected(uni_hid_device_t* d) {
    if (d == NULL) {
        mmloge("ERROR: mightymiggy_on_device_connected: Invalid NULL device\n");
    }
}

static void mightymiggy_on_device_disconnected(uni_hid_device_t* d) {
    if (d == NULL) {
        mmloge("ERROR: mightymiggy_on_device_disconnected: Invalid NULL device\n");
        return;
    }

    RuntimeControllerInfo* cinfo = getControllerInstance(d);

    mmlogi("Controller at seat %c disconnected\n", cinfo->seat == GAMEPAD_SEAT_A ? 'A' : 'B');

    // Disable CD32 interrupt
    disableCD32Trigger(cinfo);

    switch (adapterState) {
        default:
        case AST_IDLE:
            mmloge("ERROR: Controller disconnected while adapter is IDLE\n");
            break;
        case AST_JOY2_ONLY:
        case AST_JOY1_ONLY:
            // We lost the only controller we had
            cinfo->seat = GAMEPAD_SEAT_NONE;
            adapterState = AST_IDLE;
            break;
        case AST_TWO_JOYS:
            // Two controllers connected, see which one we lost
            if (cinfo->seat == GAMEPAD_SEAT_A) {
                // Make joystick in port B control the mouse again
                uni_hid_device_t* dev = getControllerForSeat(GAMEPAD_SEAT_B);
                if (dev) {
                    RuntimeControllerInfo* cinfoB = getControllerInstance(dev);
                    cinfoB->mousePins = PINS_PORT_A;
                }

                cinfo->seat = GAMEPAD_SEAT_NONE;
                adapterState = AST_JOY2_ONLY;
            } else if (cinfo->seat == GAMEPAD_SEAT_B) {
                cinfo->seat = GAMEPAD_SEAT_NONE;
                adapterState = AST_JOY1_ONLY;
            } else {
                // WTF?!
                mmloge("Unknown seat\n");
            }
            break;
    }
}

static bool seatInUse(uni_gamepad_seat_t seat) {
    bool inUse = false;

    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES && !inUse; i++) {
        uni_hid_device_t* dev = uni_hid_device_get_instance_for_idx(i);
        if (dev && bd_addr_cmp(dev->conn.btaddr, zero_addr) != 0) {
            RuntimeControllerInfo* cinfo = getControllerInstance(dev);
            if (cinfo && cinfo->seat == seat) {
                inUse = true;
            }
        }
    }

    //~ // ... unless it is a mouse which should try with PORT A. Amiga/Atari ST use
    //~ // mice in PORT A. Undefined on the C64, but most apps use it in PORT A as
    //~ // well.
    //~ uint32_t mouse_cod = MASK_COD_MAJOR_PERIPHERAL | MASK_COD_MINOR_POINT_DEVICE;
    //~ if ((d->cod & mouse_cod) == mouse_cod) {
    //~ wanted_seat = GAMEPAD_SEAT_A;
    //~ }

    return inUse;
}

static uni_error_t mightymiggy_on_device_ready(uni_hid_device_t* d) {
    uni_error_t ret = UNI_ERROR_SUCCESS;

    if (d == NULL) {
        mmloge("ERROR: mightymiggy_on_device_ready: Invalid NULL device\n");
        return UNI_ERROR_INVALID_DEVICE;
    }

    RuntimeControllerInfo* cinfo = getControllerInstance(d);

    // Some safety checks. These conditions should not happen
    if ((cinfo->seat != GAMEPAD_SEAT_NONE) || (!uni_hid_device_has_controller_type(d))) {
        mmloge("ERROR: mightymiggy_on_device_ready: pre-condition not met\n");
        return UNI_ERROR_INVALID_DEVICE;
    }

    mmlogi("Controller of type %u connected!\n", (unsigned int)d->controller_type);

    switch (adapterState) {
        case AST_IDLE:
            // No controller connected, this one is the first and will drive port B
            // FIXME: Make sure it's a joystick
            mmlogi("Assigning to port B\n");
            if (seatInUse(GAMEPAD_SEAT_B)) {
                mmloge("Seat already in use, refusing (!?)\n");
                ret = UNI_ERROR_NO_SLOTS;
            } else {
                setSeat(d, GAMEPAD_SEAT_B);
                cinfo->ledPin = PIN_LED_P2;
                cinfo->joyPins = PINS_PORT_B;
                cinfo->mousePins = PINS_PORT_A;
                adapterState = AST_JOY2_ONLY;
            }
            break;
        case AST_JOY2_ONLY:
            // One controller already connected, this new one will drive port A
            mmlogi("Assigning to port A\n");
            if (seatInUse(GAMEPAD_SEAT_A)) {
                mmloge("Seat already in use, refusing (!?)\n");
                ret = UNI_ERROR_NO_SLOTS;
            } else {
                setSeat(d, GAMEPAD_SEAT_A);
                cinfo->ledPin = PIN_LED_P1;
                cinfo->joyPins = PINS_PORT_A;
                cinfo->mousePins = NULL;

                // We also need to disconnect the joystick in port B from the mouse
                uni_hid_device_t* dev = getControllerForSeat(GAMEPAD_SEAT_B);
                if (dev) {
                    RuntimeControllerInfo* cinfoB = getControllerInstance(dev);
                    cinfoB->mousePins = NULL;
                }

                adapterState = AST_TWO_JOYS;
            }
            break;
        case AST_JOY1_ONLY:
            // We had 2 controllers but lost the one in port B, now it's back
            mmlogi("Assigning to port B\n");
            if (seatInUse(GAMEPAD_SEAT_B)) {
                mmloge("Seat already in use, refusing (!?)\n");
                ret = UNI_ERROR_NO_SLOTS;
            } else {
                setSeat(d, GAMEPAD_SEAT_B);
                cinfo->ledPin = PIN_LED_P2;
                cinfo->joyPins = PINS_PORT_B;
                cinfo->mousePins = NULL;
                adapterState = AST_TWO_JOYS;
            }
            break;
        case AST_TWO_JOYS:
        default:
            // Two controllers already connected, cannot accept a new one
            mmloge("Refusing\n");
            ret = UNI_ERROR_NO_SLOTS;
    }

    if (ret == UNI_ERROR_SUCCESS) {
        // Init controller runtime data structure
        cinfo->state = ST_FIRST_READ;
        cinfo->stateEnteredTime = 0;
        cinfo->leftAnalog.x = 0;
        cinfo->leftAnalog.y = 0;
        cinfo->rightAnalog.x = 0;
        cinfo->rightAnalog.y = 0;
        cinfo->buttonWord = BTN_NONE;
        cinfo->previousButtonWord = BTN_NONE;
        cinfo->joyMappingFunc = mapJoystickNormal;
        cinfo->currentCustomConfig = NULL;
        cinfo->c64Mode = false;
        cinfo->useAlternativeCd32Mapping = false;
        cinfo->selectComboButton = BTN_NONE;
        cinfo->programmedButton = BTN_NONE;

        /* Well, at this point we'd love to notify the SM that we have a
         * controller, but if we do this, everything will crash badly, not sure
         * why :(. Anyway, we'll get a reading soon.
         */
        //~ xQueueSendToBack (controllerUpdateQueue, &cinfo, 0);
        //~ taskYIELD ();
    }

    return ret;
}

static void mightymiggy_on_oob_event(uni_platform_oob_event_t event, void* data) {
    ARG_UNUSED(event);
    ARG_UNUSED(data);
    //~ logi ("'Misc' button pressed\n");

    //~ RuntimeControllerInfo* cinfo = getControllerInstance (d);

    //~ mightymiggy_instance_t* ins = get_mightymiggy_instance(d);

    //~ if (ins->gamepad_seat == GAMEPAD_SEAT_NONE) {
    //~ logi(
    //~ "GAMEPAD_SEAT_NONE\n");
    //~ "unijoysticle: cannot swap port since device has joystick_port = "
    //~ return;
    //~ }

    //~ // This could happen if device is any Combo emu mode.
    //~ if (ins->gamepad_seat == (GAMEPAD_SEAT_A | GAMEPAD_SEAT_B)) {
    //~ logi(
    //~ "unijoysticle: cannot swap port since has more than one port "
    //~ "associated with. "
    //~ "Leave emu mode and try again.\n");
    //~ return;
    //~ }

    //~ // Swap joysticks iff one device is attached.
    //~ int num_devices = 0;
    //~ for (int j = 0; j < CONFIG_BLUEPAD32_MAX_DEVICES; j++) {
    //~ uni_hid_device_t* tmp_d = uni_hid_device_get_instance_for_idx(j);
    //~ if ((bd_addr_cmp(tmp_d->conn.btaddr, zero_addr) != 0) &&
    //~ (get_mightymiggy_instance(tmp_d)->gamepad_seat > 0)) {
    //~ num_devices++;
    //~ if (num_devices > 1) {
    //~ logi(
    //~ "unijoysticle: cannot swap joystick ports when more than one "
    //~ "device is "
    //~ "attached\n");
    //~ uni_hid_device_dump_all();
    //~ return;
    //~ }
    //~ }
    //~ }

    //~ // swap joystick A with B
    //~ uni_gamepad_seat_t seat =
    //~ (ins->gamepad_seat == GAMEPAD_SEAT_A) ? GAMEPAD_SEAT_B : GAMEPAD_SEAT_A;
    //~ setSeat(d, seat);

    //~ // Clear joystick after switch to avoid having a line "On".
    //~ uni_joystick_t joy;
    //~ memset(&joy, 0, sizeof(joy));
    //~ process_joystick(&joy, GAMEPAD_SEAT_A);
    //~ process_joystick(&joy, GAMEPAD_SEAT_B);
}

static void mightymiggy_on_gamepad_data(uni_hid_device_t* d, uni_gamepad_t* gp) {
    if (d == NULL) {
        mmloge("ERROR: mightymiggy_on_device_gamepad_data: Invalid NULL device\n");
        return;
    }

    RuntimeControllerInfo* cinfo = getControllerInstance(d);

    // Convert data to our internal representation, starting with analog sticks
    cinfo->leftAnalog.x = constrain16(gp->axis_x, BLUEPAD32_ANALOG_MIN, BLUEPAD32_ANALOG_MAX);
    cinfo->leftAnalog.y = constrain16(gp->axis_y, BLUEPAD32_ANALOG_MIN, BLUEPAD32_ANALOG_MAX);
    cinfo->rightAnalog.x = constrain16(gp->axis_rx, BLUEPAD32_ANALOG_MIN, BLUEPAD32_ANALOG_MAX);
    cinfo->rightAnalog.y = constrain16(gp->axis_ry, BLUEPAD32_ANALOG_MIN, BLUEPAD32_ANALOG_MAX);

    // D-Pad
    if ((gp->dpad & 0x01) != 0) {
        cinfo->buttonWord |= BTN_PAD_UP;
    } else {
        cinfo->buttonWord &= ~BTN_PAD_UP;
    }
    if ((gp->dpad & 0x02) != 0) {
        cinfo->buttonWord |= BTN_PAD_DOWN;
    } else {
        cinfo->buttonWord &= ~BTN_PAD_DOWN;
    }
    if ((gp->dpad & 0x04) != 0) {
        cinfo->buttonWord |= BTN_PAD_RIGHT;
    } else {
        cinfo->buttonWord &= ~BTN_PAD_RIGHT;
    }
    if ((gp->dpad & 0x08) != 0) {
        cinfo->buttonWord |= BTN_PAD_LEFT;
    } else {
        cinfo->buttonWord &= ~BTN_PAD_LEFT;
    }

    // "Ordinary" buttons
    switch (d->controller_subtype) {
        default:
            if ((gp->buttons & BUTTON_A) != 0) {
                cinfo->buttonWord |= BTN_A;
            } else {
                cinfo->buttonWord &= ~BTN_A;
            }
            if ((gp->buttons & BUTTON_B) != 0) {
                cinfo->buttonWord |= BTN_B;
            } else {
                cinfo->buttonWord &= ~BTN_B;
            }
            if ((gp->buttons & BUTTON_X) != 0) {
                cinfo->buttonWord |= BTN_X;
            } else {
                cinfo->buttonWord &= ~BTN_X;
            }
            if ((gp->buttons & BUTTON_Y) != 0) {
                cinfo->buttonWord |= BTN_Y;
            } else {
                cinfo->buttonWord &= ~BTN_Y;
            }
            break;
        case CONTROLLER_SUBTYPE_WIIMOTE_HORIZONTAL:
            /* The way the Wii driver maps buttons to the Virtual Gamepad when
             * using a Wiimote in sideways mode does not work out too well with
             * how we map them to the DB-9 port, so let's make a special case.
             */
            // This is actually button "1", fits nicely to our B1!
            if ((gp->buttons & BUTTON_A) != 0) {
                cinfo->buttonWord |= BTN_X;
            } else {
                cinfo->buttonWord &= ~BTN_X;
            }
            // Button "2" -> B2
            if ((gp->buttons & BUTTON_B) != 0) {
                cinfo->buttonWord |= BTN_A;
            } else {
                cinfo->buttonWord &= ~BTN_A;
            }
            // Button "A" (The big one)
            if ((gp->buttons & BUTTON_X) != 0) {
                cinfo->buttonWord |= BTN_B;
            } else {
                cinfo->buttonWord &= ~BTN_B;
            }
            // Button "B" (Trigger)
            if ((gp->buttons & BUTTON_Y) != 0) {
                cinfo->buttonWord |= BTN_Y;
            } else {
                cinfo->buttonWord &= ~BTN_Y;
            }
            break;
    }

    if ((gp->buttons & BUTTON_SHOULDER_L) != 0) {
        cinfo->buttonWord |= BTN_SHOULDER_L;
    } else {
        cinfo->buttonWord &= ~BTN_SHOULDER_L;
    }
    if ((gp->buttons & BUTTON_SHOULDER_R) != 0) {
        cinfo->buttonWord |= BTN_SHOULDER_R;
    } else {
        cinfo->buttonWord &= ~BTN_SHOULDER_R;
    }
    if ((gp->buttons & BUTTON_TRIGGER_L) != 0) {
        cinfo->buttonWord |= BTN_TRIGGER_L;
    } else {
        cinfo->buttonWord &= ~BTN_TRIGGER_L;
    }
    if ((gp->buttons & BUTTON_TRIGGER_R) != 0) {
        cinfo->buttonWord |= BTN_TRIGGER_R;
    } else {
        cinfo->buttonWord &= ~BTN_TRIGGER_R;
    }
    if ((gp->buttons & BUTTON_THUMB_L) != 0) {
        cinfo->buttonWord |= BTN_THUMB_L;
    } else {
        cinfo->buttonWord &= ~BTN_THUMB_L;
    }
    if ((gp->buttons & BUTTON_THUMB_R) != 0) {
        cinfo->buttonWord |= BTN_THUMB_R;
    } else {
        cinfo->buttonWord &= ~BTN_THUMB_R;
    }

#ifdef USE_PEDALS_AS_TRIGGERS
    /* Some controllers (Android?) have "analog triggers" that get reported as
     * accelerator (right) and brake (left). Let's map them to the triggers.
     */
    if (d->controller_type == CONTROLLER_TYPE_AndroidController) {
        if (gp->throttle > PEDAL_THRESHOLD) {
            cinfo->buttonWord |= BTN_TRIGGER_R;
        } else {
            cinfo->buttonWord &= ~BTN_TRIGGER_R;
        }
        if (gp->brake > PEDAL_THRESHOLD) {
            cinfo->buttonWord |= BTN_TRIGGER_L;
        } else {
            cinfo->buttonWord &= ~BTN_TRIGGER_L;
        }
    }
#endif

    // "Misc" buttons
    /* Note: for some reason, the Back button is not reported by the Wii
     * controller decoding functions, they have to manually patched. We'll
     * need to ask the author why!
     */
    if ((gp->misc_buttons & MISC_BUTTON_SELECT) != 0) {
        cinfo->buttonWord |= BTN_BACK;
    } else {
        cinfo->buttonWord &= ~BTN_BACK;
    }
    if ((gp->misc_buttons & MISC_BUTTON_START) != 0) {
        cinfo->buttonWord |= BTN_HOME;
    } else {
        cinfo->buttonWord &= ~BTN_HOME;
    }
    if ((gp->misc_buttons & MISC_BUTTON_SYSTEM) != 0) {
        cinfo->buttonWord |= BTN_SYSTEM;
    } else {
        cinfo->buttonWord &= ~BTN_SYSTEM;
    }

    dumpButtons(cinfo->buttonWord);

    switch (adapterState) {
        case AST_TWO_JOYS:
            if (cinfo->seat == GAMEPAD_SEAT_B) {
                int16_t x, y;
                if (rightAnalogMoved(cinfo, &x, &y)) {
                    // Right analog moved on first controller, take over mouse control
                    mmlogi("Controller 2 taking over mouse\n");

                    adapterState = AST_TWO_JOYS_2IDLE;

                    // First we need to disconnect the joystick in port A from the other controller
                    uni_hid_device_t* dev = getControllerForSeat(GAMEPAD_SEAT_A);
                    if (dev) {
                        RuntimeControllerInfo* cinfoA = getControllerInstance(dev);

                        // Disable CD32 interrupt first
                        disableCD32Trigger(cinfoA);
                        vTaskDelay(pdMS_TO_TICKS(100));

                        cinfoA->joyPins = NULL;
                    }

                    // Now assign mouse control
                    cinfo->mousePins = PINS_PORT_A;
                }
            }
            break;
        case AST_TWO_JOYS_2IDLE:
            if (cinfo->seat == GAMEPAD_SEAT_A) {
                if (buttonPressed(cinfo->buttonWord, BTN_PAD_UP | BTN_PAD_DOWN | BTN_PAD_LEFT | BTN_PAD_RIGHT)) {
                    // D-Pad pressed on second controller, take over control of joystick in port A
                    mmlogi("Controller 1 taking over second joystick\n");
                    adapterState = AST_TWO_JOYS;

                    // First we need to disconnect the mouse from the other controller
                    uni_hid_device_t* dev = getControllerForSeat(GAMEPAD_SEAT_B);
                    if (dev) {
                        RuntimeControllerInfo* cinfoB = getControllerInstance(dev);
                        cinfoB->mousePins = NULL;
                    }

                    // Now assign joystick control
                    cinfo->joyPins = PINS_PORT_A;

                    // Bring the controller into a safe state
                    cinfo->state = ST_FIRST_READ;
                }
            }
            break;
        default:
            break;
    }

    xQueueSendToBack(controllerUpdateQueue, &cinfo, 0);
    //~ taskYIELD ();
}

static const uni_property_t* mightymiggy_get_property(uni_property_idx_t idx) {
    ARG_UNUSED(idx);
    return NULL;
}

//
// MightyMiggy platform entry point
//
struct uni_platform* uni_platform_mightymiggy_create(void) {
    static struct uni_platform plat = {
        .name = "MightyMiggy by SukkoPera <software@sukkology.net>",
        .init = mightymiggy_init,
        .on_init_complete = mightymiggy_on_init_complete,
        .on_device_connected = mightymiggy_on_device_connected,
        .on_device_disconnected = mightymiggy_on_device_disconnected,
        .on_device_ready = mightymiggy_on_device_ready,
        .on_oob_event = mightymiggy_on_oob_event,
        .on_gamepad_data = mightymiggy_on_gamepad_data,
        .get_property = mightymiggy_get_property,
    };

    return &plat;
}

//! @}
