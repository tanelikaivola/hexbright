/*
Copyright (c) 2012, "David Hilton" <dhiltonp@gmail.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met: 

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer. 
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies, 
either expressed or implied, of the FreeBSD Project.
*/

#include <Wire.h>
#include <Arduino.h>

/// Some space-saving options
#define LED // comment out save 786 bytes if you don't use the rear LEDs
#define PRINT_NUMBER // comment out to save 626 bytes if you don't need to print numbers (but need the LEDs)
#define ACCELEROMETER //comment out to save 3500 bytes (in development, it will shrink a lot once it's finished)

// In development, api will change.
#ifdef ACCELEROMETER 
//#define DEBUG 6
#define DPIN_ACC_INT 3

#define ACC_ADDRESS             0x4C
#define ACC_REG_XOUT            0
#define ACC_REG_YOUT            1
#define ACC_REG_ZOUT            2
#define ACC_REG_TILT            3
#define ACC_REG_INTS            6
#define ACC_REG_MODE            7
#endif


// debugging related definitions
#define DEBUG 0
// Some debug modes set the light.  Your control code may reset it, causing weird flashes at startup.
#define DEBUG_OFF 0 // no extra code is compiled in
#define DEBUG_ON 1 // initialize printing
#define DEBUG_LOOP 2 // main loop
#define DEBUG_LIGHT 3 // Light control
#define DEBUG_TEMP 4  // temperature safety
#define DEBUG_BUTTON 5 // button presses - you may experience some flickering LEDs if enabled
#define DEBUG_LED 6 // rear LEDs - you may get flickering LEDs with this enabled
#define DEBUG_ACCEL 7 // accelerometer
#define DEBUG_NUMBER 8 // number printing utility
#define DEBUG_CHARGE 9 // charge state



#if (DEBUG==DEBUG_TEMP)
#define OVERHEAT_TEMPERATURE 265 // something lower, to more easily verify algorithms
#else
#define OVERHEAT_TEMPERATURE 320 // 340 in original code, 320 = 130* fahrenheit/55* celsius (with calibration)
#endif


///////////////////////////////////
// key points on the light scale //
///////////////////////////////////
#define MAX_LEVEL 1000
#define MAX_LOW_LEVEL 500
#define CURRENT_LEVEL -1

#define NOW 1


// led constants
#define RLED 0
#define GLED 1

#define LED_OFF 0
#define LED_WAIT 1
#define LED_ON 2

// charging constants
#define CHARGING 1
#define BATTERY 7
#define CHARGED 3

class hexbright {
  public: 
    // This is the constructor, it is called when you create a hexbright object.
    hexbright();
  
    // init hardware.
    // put this in your setup().
    static void init_hardware();
    
    // Put update in your loop().  It will block until update_delay has passed.
    static void update();

    // When plugged in: turn off the light immediately, 
    //   leave the cpu running (as it cannot be stopped)
    // When on battery power: turn off the light immediately, 
    //   turn off the cpu in about .5 seconds.
    // Loop will run a few more times, and if your code turns 
    //  on the light, shutoff will be canceled. As a result, 
    //  if you do not reset your variables you may get weird 
    //  behavior after turning the light off and on again in 
    // less than .5 seconds.
    static void shutdown();

    
    // go from start_level to end_level over time (in milliseconds)
    // level is from 0-1000. 
    // 0 = no light (but still on), 500 = MAX_LOW_LEVEL, MAX_LEVEL=1000.
    // start_level can be CURRENT_LEVEL
    static void set_light(int start_level, int end_level, int time);
    // get light level (before overheat protection adjustment)
    static int get_light_level();
    // get light level (after overheat protection adjustment)
    static int get_safe_light_level();

    // Returns the duration the button has been in updates.  Keeps its value 
    //  immediately after being released, allowing for use as follows:
    // if(button_released() && button_held()>500)
    static int button_held();
    // button has been released
    static boolean button_released();
    
    // led = GLED or RLED,
    // on_time (0-MAXINT) = time in milliseconds before led goes to LED_WAIT state
    // wait_time (0-MAXINT) = time in ms before LED_WAIT state decays to LED_OFF state.
    //   Defaults to 100 ms.
    // brightness (0-255) = brightness of rear led
    //   Defaults to 255 (full brightness)
    // Takes up 16 bytes.
    static void set_led(byte led, int on_time, int wait_time=100, byte brightness=255);
    // led = GLED or RLED 
    // returns LED_OFF, LED_WAIT, or LED_ON
    // Takes up 54 bytes.
    static byte get_led_state(byte led);
    // returns the opposite color than the one passed in
    // Takes up 12 bytes.
    static byte flip_color(byte color);


    // Get the raw thermal sensor reading. Takes up 18 bytes.
    static int get_thermal_sensor();
    // Get the degrees in celsius. I suggest calibrating your sensor, as described
    //  in programs/temperature_calibration. Takes up 60 bytes
    static int get_celsius();
    // Get the degrees in fahrenheit. After calibrating your sensor, you'll need to
    //  modify this as well. Takes up 60 bytes
    static int get_fahrenheit();

    // returns CHARGING, CHARGED, or BATTERY
    // This reads the charge state twice with a small delay, then returns 
    //  the actual charge state.  BATTERY will never be returned if we are 
    //  plugged in.
    // Use this if you take actions based on the charge state (example: you
    //  turn on when you stop charging).  Takes up 56 bytes (34+22).
    static byte get_definite_charge_state();
    // returns CHARGING, CHARGED, or BATTERY
    // This reads and returns the charge state, without any verification.  
    //  As a result, it may report BATTERY when switching between CHARGED 
    //  and CHARGING.
    // Use this if you don't care if the value is sometimes wrong (charging 
    //  notification).  Takes up 34 bytes.
    static byte get_charge_state();

    
    // prints a number through the rear leds
    // 120 = 1 red flashes, 2 green flashes, one long red flash (0), 2 second delay.
    // the largest printable value is +/-999,999,999, as the left-most digit is reserved.
    // negative numbers begin with a leading long flash.
    static void print_number(long number);
    // currently printing a number
    static boolean printing_number();

#ifdef ACCELEROMETER
    // accepts things like ACC_REG_TILT
    static byte read_accelerometer(byte acc_reg);

    // Most units are in 1/100ths of Gs.
	//  so 100 = 1G.
    // last two readings have had minor acceleration
	static boolean stationary(int tolerance=10);
    // last reading had non-gravitational acceleration
    static boolean moved(int tolerance=50);
	
	// returns a value from 100 to -100. 0 is no movement.
	//  This returns lots of noise if we're pointing up or down.
	//  I've found it works well rotating it one-handed.
	static char get_spin();
    //returns the angle between straight down and 
    // returns 0 to 1. 0 == down, 1 == straight up.  Multiply by 1.8 to get degrees.
    // expect noise of about 10.
    static double difference_from_down();
    // lots of noise < 5*.  Most noise is <10*
    // noise varies partially based on sample rate.  120, noise <10*.  64, ~8?
    static double angle_change();
	
	// returns how much acceleration is occurring on a vector - down.
	//  If no acceleration is occurring, the vector should be close to {0,0,0}.
	static void absolute_vector(int* out_vector, int* in_vector);
	
	   
	// Returns the nth vector back from our position.  Currently we only store the last 4 vectors.
	//  0 = most recent reading,
	//  3 = most distant reading.
	// Do not modify the return vector.
	static int* vector(byte back);
	// Returns our best guess at which way is down.
	// Do not modify the return vector.
	static int* down();
	
    
	/// vector operations, most operate with 100 = 1G, mathematically.
	// returns a value roughly corresponding to how similar two vectors are.
	static int dot_product(int* vector1, int* vector2);
	// this will give a vector that has experienced no movement, only rotation relative to the two inputs
    static void cross_product(int * out_vector, int* in_vector1, int* in_vector2, double angle_difference);
	// magnitude of a non-normalized vector corresponds to how many Gs we're sensing
	//  The only normalized vector is down.
	static double magnitude(int* vector);
	static void sum_vectors(int* out_vector, int* in_vector1, int* in_vector2);
    static void sub_vectors(int* out_vector, int* in_vector1, int* in_vector2);
	static void copy_vector(int* out_vector, int* in_vector);
	// normalize scales our current vector to 100
	static void normalize(int* out_vector, int* in_vector, double magnitude);
    // angle difference is unusual in that it returns a value from 0-1 
	//  (0 = same angle, 1 = opposite).
    static double angle_difference(int dot_product, double magnitude1, double magnitude2);
	
	static void print_vector(int* vector, char* label);
    
  private: // internal to the library
    // good documentation:
    // http://cache.freescale.com/files/sensors/doc/app_note/AN3461.pdf
    // http://cache.freescale.com/files/sensors/doc/data_sheet/MMA7660FC.pdf
    // 1 ~= .05 Gs (page 28 of the data sheet).
    static void read_accelerometer_vector();
    
    // advances the current vector to the next (a place for more data)
    static void next_vector();
	// Recalculate down.  If there has been lots of movement, this could easily
    //  be off. But not recalculating down in such cases costs more work, and 
	//  even then, we're just guessing.  Overall, a windowed average works fairly 
	//  well.
	static void find_down();
    
	static void enable_accelerometer();
#endif

  private: 
    static void adjust_light();
    static void set_light_level(unsigned long level);
    static void overheat_protection();

    static void update_number();

    // controls actual led hardware set.  
    //  As such, state = HIGH or LOW
    static void _set_led(byte led, byte state);
    static void _led_on(byte led);
    static void _led_off(byte led);
    static void adjust_leds();

    static void read_thermal_sensor();
    
    static void read_button();
};


