/*
 *    LedControl.cpp - A library for controling Leds with a MAX7219/MAX7221
 *    Copyright (c) 2007 Eberhard Fahle
 * 
 *    Permission is hereby granted, free of charge, to any person
 *    obtaining a copy of this software and associated documentation
 *    files (the "Software"), to deal in the Software without
 *    restriction, including without limitation the rights to use,
 *    copy, modify, merge, publish, distribute, sublicense, and/or sell
 *    copies of the Software, and to permit persons to whom the
 *    Software is furnished to do so, subject to the following
 *    conditions:
 * 
 *    This permission notice shall be included in all copies or 
 *    substantial portions of the Software.
 * 
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *    OTHER DEALINGS IN THE SOFTWARE.
 */
 

#include "LedControl.h"

//the opcodes for the MAX7221 and MAX7219
#define OP_NOOP   0
#define OP_DIGIT0 1
#define OP_DIGIT1 2
#define OP_DIGIT2 3
#define OP_DIGIT3 4
#define OP_DIGIT4 5
#define OP_DIGIT5 6
#define OP_DIGIT6 7
#define OP_DIGIT7 8
#define OP_DECODEMODE  9
#define OP_INTENSITY   10
#define OP_SCANLIMIT   11
#define OP_SHUTDOWN    12
#define OP_DISPLAYTEST 15

LedControl::LedControl(int dataPin, int clkPin, int csPin, int numDevices) {
    SPI_MOSI=dataPin;
    SPI_CLK=clkPin;
    SPI_CS=csPin;
    if(numDevices<=0 || numDevices>8 )
    	numDevices=8;
    maxDevices=numDevices;
    pinMode(SPI_MOSI,OUTPUT);
    pinMode(SPI_CLK,OUTPUT);
    pinMode(SPI_CS,OUTPUT);
    digitalWrite(SPI_CS,HIGH);
    SPI_MOSI=dataPin;
    for(int i=0;i<64;i++) 
	    status[i]=0x00;
    for(int i=0;i<maxDevices;i++) {
	    spiTransfer(i,OP_DISPLAYTEST,0);
	//scanlimit is set to max on startup
	setScanLimit(i,7);
	//decode is done in source
	spiTransfer(i,OP_DECODEMODE,0);
	clearDisplay(i);
	//we go into shutdown-mode on startup
	shutdown(i,true);
    }
}

int LedControl::getDeviceCount() {
    return maxDevices;
}

void LedControl::shutdown(int addr, bool b) {
    if(addr<0 || addr>=maxDevices)
    	return;
    if(b)
	    spiTransfer(addr, OP_SHUTDOWN,0);
    else
	    spiTransfer(addr, OP_SHUTDOWN,1);
}
	
void LedControl::setScanLimit(int addr, int limit) {
    if(addr<0 || addr>=maxDevices)
	    return;
    if(limit>=0 || limit<8)
    	spiTransfer(addr, OP_SCANLIMIT,limit);
}

void LedControl::setIntensity(int addr, int intensity) {
    if(addr<0 || addr>=maxDevices)
	    return;
    if(intensity>=0 || intensity<16)	
    	spiTransfer(addr, OP_INTENSITY,intensity);
    
}

void LedControl::clearDisplay(int addr) {
    int offset;

    if(addr<0 || addr>=maxDevices)
    	return;
    offset=addr*8;
    for(int i=0;i<8;i++) {
	    status[offset+i]=0;
    	spiTransfer(addr, i+1,status[offset+i]);
    }
}

#if !defined(NO_LED_MATRIX)
void LedControl::setLed(int addr, int row, int column, boolean state) {
    int offset;
    byte val=0x00;

    if(addr<0 || addr>=maxDevices)
    	return;
    if(row<0 || row>7 || column<0 || column>7)
	    return;
    offset=addr*8;
    val=B10000000 >> column;
    if(state)
    	status[offset+row]=status[offset+row]|val;
    else {
    	val=~val;
    	status[offset+row]=status[offset+row]&val;
    }
    spiTransfer(addr, row+1,status[offset+row]);
}
	
void LedControl::setRow(int addr, int row, byte value) {
    int offset;
    if(addr<0 || addr>=maxDevices)
    	return;
    if(row<0 || row>7)
	    return;
    offset=addr*8;
    status[offset+row]=value;
    spiTransfer(addr, row+1,status[offset+row]);
}

//TODO: Investigate why this doesn't set anything in status?
void LedControl::setColumn(int addr, int col, byte value) {
    byte val;

    if(addr<0 || addr>=maxDevices)
    	return;
    if(col<0 || col>7) 
	    return;
    for(int row=0;row<8;row++) {
    	val=value >> (7-row);
	    val=val & 0x01;
    	setLed(addr,row,col,val);
    }
}
#endif

void LedControl::setDigit(int addr, int digit, byte value, boolean dp) {
    int offset;
    byte v;

    if(addr<0 || addr>=maxDevices)
    	return;
    if(digit<0 || digit>7 || value>15)
	    return;
    offset=addr*8;
    v=pgm_read_byte(charTable + value);
    if(dp)
	    v|=B10000000;
    status[offset+digit]=v;
    spiTransfer(addr, digit+1,v);
    
}

void LedControl::setChar(int addr, int digit, char value, boolean dp) {
    int offset;
    byte index,v;

    if(addr<0 || addr>=maxDevices)
    	return;
    if(digit<0 || digit>7)
 	    return;
    offset=addr*8;
    index=(byte)value;
    if(index >= (sizeof(charTable)/sizeof(charTable[0])) ) {
	    //Nothing defined in our mapping table
	    //Use ' ' - The space char
	    value=32;
    }
    v=pgm_read_byte(charTable + index);
    if(dp)
    	v|=B10000000;
    status[offset+digit]=v;
    spiTransfer(addr, digit+1,v);
}


//TODO: Make this happen "inline" in the other functions, so we don't need to have a big buffer and two steps
//A static helper function that changes some characters to others that are better representations
void LedControl::modify_string_for_better_display(const char * in_text, char * out_text, boolean * out_decimals, int length_of_out_arrays)
{
    //Clear
  for(int i = 0; i<length_of_out_arrays; i++)
  {
    out_decimals[i] = false;
    out_text[i] = '\0';
    //TODO: Tidy this up.....
    out_text[i+1] = '\0';//The string is one larger (wasteful but quick)
  }
  
  int new_string_index = 0;
  for(unsigned int i = 0; i<strlen(in_text) && (new_string_index<length_of_out_arrays); i++)
  {
//    Serial.print( "in_text["); Serial.flush();
    
    if((in_text[i] == '.') && (i>0) )
    {
      //If it is consecutive '.'s -- then add a new entry
      //else just pad on to previous letter
      if(in_text[i-1] == '.')
      {
//        Serial.println( "consec ."); Serial.flush();
        out_decimals[new_string_index] = true;
        out_text[new_string_index]     = in_text[i];
        new_string_index++;
      }
      else
      {
//        Serial.println( "." ); Serial.flush();
        out_decimals[new_string_index - 1] = true;
        //No increase of output pointer
      }
    }
    else
    if((in_text[i] == '?') && (i>0))
    {
      //Make the preceding DP active for a '?'
      out_text[new_string_index] = in_text[i];
      out_decimals[new_string_index - 1] = true;
      new_string_index++;
    }
    else
    if(
      (new_string_index<(length_of_out_arrays-1))
      &&
      (
      (in_text[i] == 'm')
      ||
      (in_text[i] == 'w')
      )
    )
    {
      //Double these letters
      out_text[new_string_index] = in_text[i];
      new_string_index++;
      out_text[new_string_index] = in_text[i];
      new_string_index++;
    }
    else
    if((new_string_index<(length_of_out_arrays-1)) && (in_text[i] == 'W'))
    {
        //Insert these repalcement 'letters' to create a two char version
        out_text[new_string_index] = DOUBLE_CHAR_UPPERCASE_W_LHS;
        new_string_index++;
        out_text[new_string_index] = DOUBLE_CHAR_UPPERCASE_W_RHS;
        new_string_index++;
    }
    else
    if((new_string_index<(length_of_out_arrays-1)) && (in_text[i] == 'M'))
    {
        //Insert these repalcement 'letters' to create a two char version
        out_text[new_string_index] = DOUBLE_CHAR_UPPERCASE_M_LHS;
        new_string_index++;
        out_text[new_string_index] = DOUBLE_CHAR_UPPERCASE_M_RHS;
        new_string_index++;
    }
    else
    {
      //Copy through unchanged....
      out_text[new_string_index] = in_text[i];
      new_string_index++;
    }
  }
}

#define EACH_SCREEN_DWELL_PER_DIGIT_PERCENT       (115) //!< Percent of duration_ms/DIGITS_PER_DISPLAY wanted for each screen
#define FIRST_SCREEN_EXTRA_DWELL_PERCENT          (55)  //!< Percent of duration_ms wanted in addition for the first screen
#define LAST_SCREEN_EXTRA_DWELL_PERCENT           (50)  //!< Percent of duration_ms wanted in addition for the last screen
//TODO: Extend to multiple displays
void LedControl::setDisplayAndScroll(int addr, const char * text, const boolean * decimals, int max_length_of_in_arrays /*TODO:: Remove this...*/,
                                     unsigned long duration_ms, void (*delay_function)(unsigned long  duration_ms) /*Make delay() if NULL*/   )
{
  int text_offset    = 0;
  int digits_left    = strlen(text);
  
  //One "screen" - simple display
  //TODO: Add centering?
  if(digits_left <= (DIGITS_PER_DISPLAY))
  {
    digits_left-=1;
    //TODO: More efficient to have the display off here (can program whilst asleep)
    do
    {
      this->setChar(addr, digits_left, text[text_offset], decimals[text_offset]);
      text_offset++;
      digits_left--;
    }while(digits_left >= 0);
    //TODO: and only enable here
    //We are deep sleeping but the screen is still active
    delay_function( duration_ms );
  }
  else
  {
    //Clamp if too long
    digits_left = min(digits_left, max_length_of_in_arrays);
    int     num_digits_over_screen = digits_left - DIGITS_PER_DISPLAY;
    char    to_display_text[DIGITS_PER_DISPLAY_STR];
    boolean to_display_decimals[DIGITS_PER_DISPLAY];
//    Serial.print( "Digits over  =" );
//    Serial.println( num_digits_over_screen );
    unsigned long first_screen_extra_duration_ms = (duration_ms * FIRST_SCREEN_EXTRA_DWELL_PERCENT)/100;
    unsigned long last_screen_extra_duration_ms  = (duration_ms * LAST_SCREEN_EXTRA_DWELL_PERCENT)/100;
    unsigned long each_screen_duration_ms  = (((duration_ms * EACH_SCREEN_DWELL_PER_DIGIT_PERCENT)/DIGITS_PER_DISPLAY)/100);
    
    for(int j = 0; j < num_digits_over_screen+1; j++)
    {
      for(int i = 0; i < DIGITS_PER_DISPLAY; i++)
      {
        to_display_text[i]     = text[i+j];
        to_display_text[i+1]   = '\0'; //Wasteful but beats two loops
        to_display_decimals[i] = decimals[i+j];
      }
      this->setDisplayAndScroll(addr, to_display_text, to_display_decimals, max_length_of_in_arrays, each_screen_duration_ms, delay_function);
      if(j==0)
      {
        // An extra pause at the start of the scrolling message
        //Sleeping but the screen is still active
        delay_function( first_screen_extra_duration_ms );
      }
    }
    // An extra pause at the end of the scrolling message
    //Sleeping but the screen is still active
    delay_function( last_screen_extra_duration_ms );
  }
}

#if 0
void loop_test_chars()
{
  static int counter_tc = 0;

  mydisplay.setChar(0, 0, 'A' + counter_tc%26, false);
  mydisplay.setChar(0, 1, ' ', false);
  mydisplay.setChar(0, 2, 'a' + counter_tc%26, false);
  mydisplay.setChar(0, 3, ' ', false);
  mydisplay.setChar(0, 4, '0' + counter_tc%10, true);
  mydisplay.setChar(0, 5, ' ', false);
  mydisplay.setChar(0, 6, 0 + counter_tc%16, false);
  mydisplay.setChar(0, 7, ' ', false);

  counter_tc++;  
  delay(500);
}
#endif

void LedControl::spiTransfer(int addr, volatile byte opcode, volatile byte data) {
    //Create an array with the data to shift out
    int offset=addr*2;
    int maxbytes=maxDevices*2;

    for(int i=0;i<maxbytes;i++)
    	spidata[i]=(byte)0;
    //put our device data into the array
    spidata[offset+1]=opcode;
    spidata[offset]=data;
    //enable the line 
    digitalWrite(SPI_CS,LOW);
    //Now shift out the data 
    for(int i=maxbytes;i>0;i--)
     	shiftOut(SPI_MOSI,SPI_CLK,MSBFIRST,spidata[i-1]);
    //latch the data onto the display
    digitalWrite(SPI_CS,HIGH);
}    


