/*
 Name:		FloodSaver.ino
 Created:	8/16/2017 9:06:19 PM
 Author:	Maxim
*/

#include <LiquidCrystal.h>
#include "state.hpp"
#include <avr/wdt.h>
#include <avr/delay.h>

const byte
	// LCD pins
	rs	= 12, 
	en	= 11, 
	d4	= 10, 
	d5	= 9, 
	d6	= 8, 
	d7	= 7;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const byte 
	// System IO pins
	button_0_pin	= 2, 
	button_1_pin	= 3, 
	led_red_pin		= 4, 
	led_green_pin	= 5, 
	buzzer_pin		= 6,
	pressure_pin	= A0;

byte button_0, button_1;
int32_t last_pressure_reading_mpsi, pressure_reading_mpsi;
volatile bool button_0_pressed, button_1_pressed;
uint32_t last_time, current_time, time_elapsed_ms;

flood_saver::StateMachine state_machine;
flood_saver::Inputs inputs;
flood_saver::Outputs outputs;

void setup() {
	lcd.begin(16, 2);
	pinMode(pressure_pin, INPUT);
	pinMode(button_0_pin, INPUT_PULLUP);
	pinMode(button_1_pin, INPUT_PULLUP);
	pinMode(led_red_pin, OUTPUT);
	pinMode(led_green_pin, OUTPUT);
	pinMode(buzzer_pin, OUTPUT);
	attachInterrupt(digitalPinToInterrupt(button_0_pin), button_0_interrupt, FALLING);
	attachInterrupt(digitalPinToInterrupt(button_1_pin), button_1_interrupt, FALLING);
	
	byte watchdog_register_value = (1 << WDIE) | (1 << WDP3) | (1 << WDP0);
	cli();
	WDTCSR = (1 << WDCE) | (1 << WDE);
	WDTCSR = watchdog_register_value;
	sei();

	last_pressure_reading_mpsi = 0;
	last_time = millis();
	read_pressure_and_time(pressure_reading_mpsi, time_elapsed_ms);
}

struct P_t_record {
	int32_t P_ms;
	int32_t delta_t_ms;
};

P_t_record test1[] = {
	{ 78000, 5001 },
	{ 76000, 5001 },
	{ 74000, 5001 },
	{ 72000, 5001 },
	{ 70000, 5001 },
	{ 68000, 5001 },
	{ 66000, 5001 },
	{ 64000, 5001 },
	{ 62000, 5001 },
	{ 60000, 5001 },
	{ 58000, 5001 },
	{ 56000, 5001 },
	{ 54000, 5001 },
	{ 52000, 5001 },
	{ 50000, 5001 },
	{ 48000, 5001 },
	{ 46000, 5001 }
};

void read_pressure_and_time(int32_t& P_mpsi, uint32_t& delta_t_ms) {
	if (false) {
		// This reading is not accurate right now
		P_mpsi = 99 * static_cast<int32_t>(analogRead(pressure_pin));
		current_time = millis();
		delta_t_ms = current_time - last_time;
		last_time = current_time;
	}
	else {
		static uint32_t index;
		P_t_record* test = test1;
		index = (index + 1) % 17;
		P_mpsi = test[index].P_ms;
		delta_t_ms = test[index].delta_t_ms;
	}
}

void loop() {

	last_pressure_reading_mpsi = pressure_reading_mpsi;
	read_pressure_and_time(pressure_reading_mpsi, time_elapsed_ms);

	_delay_ms(1000); //DEBUG

	inputs.P = pressure_reading_mpsi;
	inputs.delta_P = (1000 * (pressure_reading_mpsi - last_pressure_reading_mpsi)) / static_cast<int32_t>(time_elapsed_ms);
	inputs.delta_t = time_elapsed_ms;
	inputs.reset_button = button_1_pressed;
	button_1_pressed = false;
	inputs.away_switch_on = button_0_pressed;
	button_0_pressed = false;

	state_machine.run(inputs, outputs);
	
	lcd.setCursor(0, 0);
	lcd.print(inputs.delta_t);
	lcd.print("    ");

	lcd.setCursor(8, 0);
	lcd.print(outputs.message[0]);
	lcd.print(outputs.message[1]);
	lcd.print(outputs.message[2]);
	lcd.print(outputs.message[3]);
	lcd.print(outputs.message[4]);
	lcd.print(outputs.message[5]);
	lcd.print(outputs.message[6]);
	lcd.print(outputs.message[7]);

	lcd.setCursor(0, 1);
	lcd.print(inputs.P);
	lcd.print("    ");

	lcd.setCursor(8, 1);
	lcd.print(inputs.delta_P);
	lcd.print("    ");

	if (outputs.valve_open) {
		digitalWrite(led_green_pin, HIGH);
	}
	else {
		digitalWrite(led_green_pin, LOW);
	}

	if (outputs.leak_alarm_on) {
		digitalWrite(led_red_pin, HIGH);
	}
	else {
		digitalWrite(led_red_pin, LOW);
	}

	if (outputs.alarm_audio_on) {
		digitalWrite(buzzer_pin, HIGH);
	}
	else {
		digitalWrite(buzzer_pin, LOW);
	}

	wdt_reset();
}

ISR(WDT_vect) {
	cli();
	while (true) {
		digitalWrite(buzzer_pin, HIGH);
		digitalWrite(led_red_pin, HIGH);
		digitalWrite(led_green_pin, LOW);
		_delay_ms(100);
		digitalWrite(buzzer_pin, LOW);
		digitalWrite(led_red_pin, LOW);
		digitalWrite(led_green_pin, HIGH);
		_delay_ms(100);
	}
}

void button_0_interrupt() {
	button_0_pressed = true;
}

void button_1_interrupt() {
	button_1_pressed = true;
}