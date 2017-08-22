
/*
	Name:		FloodSaver.ino
	Created:	8/16/2017 9:06:19 PM
	Author:	Maxim Prokopenko

	Written for Kirk Dobbs and Len Shankland.
	Arduino implementation of interface between state machine and physical I/O.
	Requirements from patent "Smart Building Water Supply Management System With
	Leak Detection And Flood Prevention".

	Wiring instructions:

		LCD VSS -- Ground
		LCD VDD -- 5V
		LCD V0	-- Middle terminal of 10k variable resistor 1 (VR1)
		LCD RS	-- Arduino D12
		LCD RW	-- Ground
		LCD E	-- Arduino D11
		LCD D0	-- NC
		LCD D1	-- NC
		LCD D2	-- NC
		LCD D3	-- NC
		LCD D4	-- Arduino D10
		LCD D5	-- Arduino D9
		LCD D6	-- Arduino D8
		LCD D7	-- Arduino D7
		Arduino D6	-- [+]Active Buzzer[-] -- Ground
		Arduino D5	-- [A]Green LED[K] -- [2k resistor] -- Ground
		OR use Arduino D5 for active high valve output
		Arduino D4	-- [A]Red Led[K] -- [2k resistor] -- Ground
		Arduino D3	-- [Normally open momentary switch Button 1] -- Ground
		Arduino D2	-- [Normally open momentary switch Button 0] -- Ground
		Arduino A0	-- Middle terminal of 10k variable resistor 2 (VR2)
		OR use Arduino A0 for pressure voltage in (0-5V)
		Left terminal of VR1 to ground, right terminal to 5V
		Left terminal of VR2 to ground, right terminal to 5V

	Usage instructions:

		Button 1 connected to Arduino D3 mutes audio alarms and resets alarms
		Button 0 switches between Home and Away modes
		Sampling time is set to 1 second, so 50 measurements are needed to set the leak alarm
			* This number can be altered in the 'state.h' file, T_LEAK_TIMEOUT at bottom of file
*/

#include <LiquidCrystal.h>
#include "state.h"
#include <avr/wdt.h>
#include <avr/delay.h>

const byte // LCD pins
	rs	= 12, 
	en	= 11, 
	d4	= 10, 
	d5	= 9, 
	d6	= 8, 
	d7	= 7;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

byte delta_char[8] = { // LCD special char
	B00000,
	B00000,
	B00100,
	B01010,
	B10001,
	B11111,
	B00000
};

const byte // System IO pins
	button_0_pin	= 2, 
	button_1_pin	= 3, 
	led_red_pin		= 4, 
	valve_pin		= 5, 
	buzzer_pin		= 6,
	pressure_pin	= A0;

// I/O state variables
byte button_0, button_1;
int32_t last_P, pressure_reading_mpsi;
volatile bool button_0_pressed, button_1_pressed;
uint32_t last_time, current_time, time_elapsed_ms;

// State machine components and interfacing structs
flood_saver::StateMachine state_machine;
flood_saver::Inputs inputs;
flood_saver::Outputs outputs;

void setup() {

	// LCD display, input and outputs
	lcd.begin(16, 2);
	lcd.createChar(0, delta_char);
	pinMode(pressure_pin, INPUT);
	pinMode(button_0_pin, INPUT_PULLUP);
	pinMode(button_1_pin, INPUT_PULLUP);
	pinMode(led_red_pin, OUTPUT);
	pinMode(valve_pin, OUTPUT);
	pinMode(buzzer_pin, OUTPUT);

	// Create interrupts for momentary switches
	attachInterrupt(digitalPinToInterrupt(button_0_pin), button_0_interrupt, FALLING);
	attachInterrupt(digitalPinToInterrupt(button_1_pin), button_1_interrupt, FALLING);
	
	// Set watchdog timer values
	// This will call an emergency alarm function that closes valve and flashes
	// alarm LED. 
	byte watchdog_register_value = (1 << WDIE) | (1 << WDP3) | (1 << WDP0);
	cli();
	WDTCSR = (1 << WDCE) | (1 << WDE);
	WDTCSR = watchdog_register_value;
	sei();

	// Initialize input values
	last_time = millis();
	read_pressure_and_time(pressure_reading_mpsi, time_elapsed_ms);
	last_P = last_P;

	// Initialize output values
	digitalWrite(led_red_pin, LOW);
	digitalWrite(valve_pin, LOW);
	digitalWrite(buzzer_pin, LOW);
}

void read_pressure_and_time(int32_t& P_mpsi,  uint32_t& delta_t_ms) {

	// This reading is not accurate right now, adjust for actual sensor later
	P_mpsi = 99 * static_cast<int32_t>(analogRead(pressure_pin));
	current_time = millis();
	delta_t_ms = current_time - last_time;
	last_time = current_time;
}

void loop() {

	// Read pressure and time, swap variables for pressure difference calc
	last_P = inputs.P;
	read_pressure_and_time(inputs.P, inputs.delta_t);
	inputs.delta_P = (1000 * (inputs.P - last_P)) / static_cast<int32_t>(inputs.delta_t);

	// Input switch data, button 1 is momentary on, button 0 toggles
	inputs.reset_button = button_1_pressed;
	if (button_0_pressed) inputs.away_switch_on = !inputs.away_switch_on;
	button_0_pressed = button_1_pressed = false; // Reset momentary switch inputs
	

	///////////////////////////////////
	//        Logic in state.h       //
	///////////////////////////////////
	state_machine.run(inputs, outputs);
	///////////////////////////////////
	//     See state machine pdf     //
	///////////////////////////////////
	

	// Print the message returned by the current state
	lcd.setCursor(0, 0);	
	lcd.print(outputs.message[0]);
	lcd.print(outputs.message[1]);
	lcd.print(outputs.message[2]);
	lcd.print(outputs.message[3]);
	lcd.print(outputs.message[4]);
	lcd.print(outputs.message[5]);
	lcd.print(outputs.message[6]);
	lcd.print(outputs.message[7]);
	lcd.print("    ");

	// Print 'Away' or 'Home' on LCD
	lcd.setCursor(12, 0);
	if (inputs.away_switch_on)
		lcd.print("Away");
	else
		lcd.print("Home");

	// Print current pressure
	lcd.setCursor(0, 1);
	lcd.print("P:");
	lcd.print(inputs.P / 1000);
	lcd.print(".");
	lcd.print((inputs.P / 100) % 10);
	lcd.print("  ");

	// Print current pressure differential
	lcd.setCursor(9, 1);
	lcd.write(byte(0));
	lcd.print(":");
	if (inputs.delta_P < 0) lcd.print("-");
	lcd.print(abs(inputs.delta_P / 1000));
	lcd.print(".");
	lcd.print(abs(inputs.delta_P / 100) % 10);
	lcd.print("  ");

	// Translate outputs from state machine into real world
	digitalWrite(valve_pin, outputs.valve_open);
	digitalWrite(led_red_pin, outputs.leak_alarm_on || outputs.water_source_alarm_on);
	digitalWrite(buzzer_pin, outputs.alarm_audio_on);

	wdt_reset();

	_delay_ms(1000); // Samples every second right now
}

ISR(WDT_vect) {
	/*
		If both indicators are flashing and buzzer is periodic the program
		has hung. This represents a critical failure of the logic and should
		never ever happen.
	*/
	cli();
	digitalWrite(valve_pin, LOW);
	while (true) {
		digitalWrite(buzzer_pin, HIGH);
		digitalWrite(led_red_pin, HIGH);
		digitalWrite(valve_pin, LOW);
		_delay_ms(100);
		digitalWrite(buzzer_pin, LOW);
		digitalWrite(led_red_pin, LOW);
		digitalWrite(valve_pin, HIGH);
		_delay_ms(100);
	}
}

// Trivial interrupts so we don't miss a button press during the main loop
void button_0_interrupt() { button_0_pressed = true; }
void button_1_interrupt() { button_1_pressed = true; }
