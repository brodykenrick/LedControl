/* Scrolling messages on a 7-segment display via a MAX7219.
Copyright 2013 Brody Kenrick.


Using modified LEDControl @ https://github.com/brodykenrick/MAX72xx_SPI_Arduino


Developed for the Retro Runner Readout
http://retrorunnerreadout.blogspot.com


Hardware
An Arduino Pro Mini 3v3
This 7-segment display : http://dx.com/p/8-segment-led-display-board-module-for-arduino-147814

The display board is labelled ant the connections are as follows:
MAX7219/7221 -> Arduino:Pin
  DIN       -> MOSI:11    (Arduino output)
  CLK       -> SCK:13     (Arduino output)
  LOAD/#CS  -> SS:10      (Arduino output)

Wiring to the Arduino Pro Mini 3v3 can be seen in 'mydisplay' below.
*/

#include <Arduino.h>

//#define NDEBUG
#define __ASSERT_USE_STDERR
#include <assert.h>

#define USE_NARCOLEPTIC_DELAY //<! Use Narcoleptic to save battery with power down sleep (vs. busy delay())
#define USE_NARCOLEPTIC_DISABLE //<! Use Narcoleptic to save some battery with disabling certain items but not use narco delay....


#if (defined(USE_NARCOLEPTIC_DELAY) || defined(USE_NARCOLEPTIC_DISABLE))
#include <Narcoleptic.h>
#endif

#include <LedControl.h>


#define CONSOLE_BAUD_RATE (115200)


#define DISPLAY_DURATION_MS (1300)  //!< This is the display time for 8 characters. Scrolling takes longer (not quite linear increase).
#define DELAY_BETWEEN_DISPLAYS_MS (1500) //<! Duration to have screen shut down between displaying messages
#define DISPLAY_INTENSITY (15) //<! 0..15


//Logging macros
//********************************************************************

#define SERIAL_DEBUG
#if !defined(USE_SERIAL_CONSOLE)
//Disable logging under these circumstances
#undef SERIAL_DEBUG
#endif
//F() stores static strings that come into existence here in flash (makes things a bit more stable)
#ifdef SERIAL_DEBUG
#define SERIAL_DEBUG_PRINT(x)  	        (Serial.print(x))
#define SERIAL_DEBUG_PRINTLN(x)	        (Serial.println(x))
#define SERIAL_DEBUG_PRINT_F(x)  	(Serial.print(F(x)))
#define SERIAL_DEBUG_PRINTLN_F(x)	(Serial.println(F(x)))
#define SERIAL_DEBUG_PRINT2(x,y)  	(Serial.print(x,y))
#define SERIAL_DEBUG_PRINTLN2(x,y)	(Serial.println(x,y))

#else
#define SERIAL_DEBUG_PRINT(x)
#define SERIAL_DEBUG_PRINTLN(x)
#define SERIAL_DEBUG_PRINT_F(x)
#define SERIAL_DEBUG_PRINTLN_F(x)
#define SERIAL_DEBUG_PRINT2(x,y)
#define SERIAL_DEBUG_PRINTLN2(x,y)

#endif


//********************************************************************


#define MAX_CHARS_TO_DISPLAY (56)
#define MAX_CHARS_TO_DISPLAY_STR (MAX_CHARS_TO_DISPLAY+1) //'\0' terminated



// ****************************************************************************
// ******************************  GLOBALS  ***********************************
// ****************************************************************************


static LedControl     mydisplay = LedControl(11/*DIN:MOSI*/, 13/*CLK:SCK*/, 10/*CS:SS*/, 1/*Device count*/);


//Used in setup and then in loop -- save stack on this big variable by using globals
static char    adjusted_text[MAX_CHARS_TO_DISPLAY_STR] = "";
static boolean temp_string_decimals[MAX_CHARS_TO_DISPLAY];

// ****************************************************************************
// *******************************  TEXT  *************************************
// ****************************************************************************


const char startup_text_00[] PROGMEM = "- _ - ~ - _ - ~ - _ - ~ -";
const char startup_text_01[] PROGMEM = "Oh HI! ";
const char startup_text_02[] PROGMEM = "You  ";
const char startup_text_03[] PROGMEM = "Ready ";
const char startup_text_04[] PROGMEM = "Set  ";
const char startup_text_05[] PROGMEM = "Go!!! ";

//! Texts sent to the display only at startup [in setup()]
PROGMEM const char * const startup_texts[] =
{
  startup_text_00,
  startup_text_01,
  startup_text_02,
  startup_text_03,
  startup_text_04,
  startup_text_05,
};
#define STARTUP_TEXTS_COUNT ( sizeof(startup_texts)/sizeof(const char *) )

// ****************************************************************************

const char loop_text_00[] PROGMEM        = "Test Text 00";
const char loop_text_01[] PROGMEM        = "Test Text 01";
const char loop_text_02[] PROGMEM        = "Test Text 02 is Very Much Longer than the Others....";
const char loop_text_03[] PROGMEM        = "Short 03";


PROGMEM const char * const loop_texts[] =
{
  loop_text_00,
  loop_text_01,
  loop_text_02,
  loop_text_03,
};
#define LOOP_TEXTS_COUNT ( sizeof(loop_texts)/sizeof(loop_texts[0]) )

// ****************************************************************************



// **************************************************************************************************
// **********************************  Helper *******************************************************
// **************************************************************************************************

unsigned long my_millis_function()
{
#if defined(USE_NARCOLEPTIC_DELAY)
  return( millis() + Narcoleptic.millis() );
#else
  return millis();
#endif
}

//Function allowing the LED library to do a callback to our delay functions
void my_delay_function(unsigned long duration_ms)
{
#if defined(USE_NARCOLEPTIC_DELAY)
#if defined(USE_SERIAL_CONSOLE)
  Serial.flush(); //Need to let the serial buffer finish transmissions
#endif //defined(USE_SERIAL_CONSOLE)
  Narcoleptic.delay( duration_ms );
#else
  delay( duration_ms );
#endif

}

void get_text_from_pm_char_ptr_array(char dst_text[], int dst_text_size, /*PROGMEM*/ const char * const pm_char_ptr_array[], int pm_char_ptr_array_size, int counter)
{
  const char * const* str_in_pm = &pm_char_ptr_array[ counter % pm_char_ptr_array_size ];
  
  for(int i = 0; i<dst_text_size; i++)
  {
    dst_text[i] = '\0';
  }
  strncpy_P(dst_text, (char*)pgm_read_word( str_in_pm ), MAX_CHARS_TO_DISPLAY_STR);
}

// **************************************************************************************************
// **********************************  Display  *****************************************************
// **************************************************************************************************

void adjust_string(const char * text, char * out_text, boolean * out_decimals)
{
  char replace_text[MAX_CHARS_TO_DISPLAY_STR];
  //Clear
  for(int i = 0; i<MAX_CHARS_TO_DISPLAY_STR; i++)
  {
    replace_text[i] = '\0';
  }

  SERIAL_DEBUG_PRINT_F( "Text = " );
  SERIAL_DEBUG_PRINTLN( text );

  mydisplay.modify_string_for_better_display(text, out_text, out_decimals, MAX_CHARS_TO_DISPLAY);
  SERIAL_DEBUG_PRINT_F( "Mod. = " );
  SERIAL_DEBUG_PRINTLN( out_text );
}


//NOTE: Sleeps with screen off for period -- DELAY_BETWEEN_DISPLAYS_MS
void print_and_delay(const char * text)
{
    adjust_string( text, adjusted_text, temp_string_decimals );
    mydisplay.shutdown(0, false);  // Turns on display
    mydisplay.setDisplayAndScroll(0, adjusted_text, temp_string_decimals, MAX_CHARS_TO_DISPLAY, DISPLAY_DURATION_MS, my_delay_function );
    mydisplay.clearDisplay(0);
    mydisplay.shutdown(0, true);  // Turns off display (saving battery)
    my_delay_function( DELAY_BETWEEN_DISPLAYS_MS );
}


// **************************************************************************************************
// ************************************  Setup  *****************************************************
// **************************************************************************************************
void setup()
{
#if defined(USE_SERIAL_CONSOLE)
  Serial.begin(CONSOLE_BAUD_RATE); 
#endif //defined(USE_SERIAL_CONSOLE)

  SERIAL_DEBUG_PRINTLN("Scrolling 7-Segment Display Demo!");
  SERIAL_DEBUG_PRINTLN_F("Setup.");

  mydisplay.shutdown(0, false);  // turns on display
  mydisplay.setIntensity(0, DISPLAY_INTENSITY); // 0..15 = brightest
  mydisplay.clearDisplay(0);
  mydisplay.shutdown(0, true);  // turns off display

#if defined(USE_NARCOLEPTIC_DISABLE)
  Narcoleptic.disableTimer2();
//  Narcoleptic.disableTimer1(); //Needed for SPI
  Narcoleptic.disableMillis();
#if !defined(USE_SERIAL_CONSOLE)
  Narcoleptic.disableSerial();
#endif
  Narcoleptic.disableADC();

#endif

  //Print the startup messages
  for(unsigned int counter_setup = 0;
      counter_setup < STARTUP_TEXTS_COUNT; counter_setup++)
  {
    char text[MAX_CHARS_TO_DISPLAY_STR];
    get_text_from_pm_char_ptr_array(text, MAX_CHARS_TO_DISPLAY_STR,
                                    startup_texts, STARTUP_TEXTS_COUNT, counter_setup);
    print_and_delay( text );
  }
}

// **************************************************************************************************
// ************************************  Loop *******************************************************
// **************************************************************************************************


void loop()
{
  static int counter = 0;

  char text[MAX_CHARS_TO_DISPLAY_STR];

  //Pull a string from the loop texts
  //These are one per loop() and some of them get replaced if they are magic texts
  get_text_from_pm_char_ptr_array(text, MAX_CHARS_TO_DISPLAY_STR, loop_texts, LOOP_TEXTS_COUNT, counter++);

  //Delay is in here
  print_and_delay( text );
  
}
