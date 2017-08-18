/*
 Name:		FloodSaver.ino
 Created:	8/16/2017 9:06:19 PM
 Author:	Maxim
*/

#include <LiquidCrystal.h>
#include "from_plc.hpp"
#include <avr/wdt.h>
#include <avr/delay.h>

const byte rs = 12, en = 11, d4 = 10, d5 = 9, d6 = 8, d7 = 7;
const byte 
	button_0_pin = 2, 
	button_1_pin = 3, 
	led_red_pin = 4, 
	led_green_pin = 5, 
	buzzer_pin = 6,
	pressure_pin = A0;
byte button_0, button_1;
uint16_t pressure_reading_deciP;
bool button_0_pressed, button_1_pressed;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

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
	WDTCSR = (1 << WDCE);
	WDTCSR = (1 << WDIE);
}

void loop() {
	if (button_0_pressed == true) {
		lcd.clear();
		lcd.print("Button 0 last");
		button_0_pressed = false;
		lcd.setCursor(0, 1);
		lcd.print(millis());
	} 
	else if (button_1_pressed == true) {
		lcd.clear();
		lcd.print("Button 1 last");
		button_1_pressed = false;
		lcd.setCursor(0, 1);
		lcd.print(millis());
	}
	else {
		pressure_reading_deciP = (41 * (uint16_t)analogRead(pressure_pin) + 57) / 42;
		char analog_reading_str[4];
		sprintf(analog_reading_str, "%4d", pressure_reading_deciP);
		if (analog_reading_str[2] == ' ') analog_reading_str[2] = '0';
		lcd.setCursor(11, 1);
		lcd.print(analog_reading_str[0]);
		lcd.print(analog_reading_str[1]);
		lcd.print(analog_reading_str[2]);
		lcd.print('.');
		lcd.print(analog_reading_str[3]);
	}

	if (digitalRead(button_0_pin) == 0 and digitalRead(button_1_pin) == 0) {
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
