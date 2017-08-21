/*
 Name:		FloodSaver.ino
 Created:	8/16/2017 9:06:19 PM
 Author:	Maxim
*/

#include <LiquidCrystal.h>
#include "from_plc.hpp"
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
int32_t pressure_reading_mpsi;
bool button_0_pressed, button_1_pressed;
uint32_t last_time, current_time, time_elapsed_ms;

flood_saver::StateData globals = {};

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
	
	//
	byte watchdog_register_value = (1 << WDIE) | (1 << WDP3) | (1 << WDP0);
	cli();
	WDTCSR = (1 << WDCE) | (1 << WDE);
	WDTCSR = watchdog_register_value;
	sei();

	//wdt_enable(WDTO_2S);

	last_time = millis();
	read_pressure_and_time(pressure_reading_mpsi, time_elapsed_ms);
	
	// Audio alarm shutoff test
	/*
	globals.x1 = false;
	globals.x2 = false;
	globals.audio_alarm_on = true;
	globals.valve_104_open = false;
	*/

	// test1
	globals.x1 = true;
	// globals.water_supply_alarm_on = true;
}

struct P_t_record {
	int32_t P_ms;
	int32_t delta_t_ms;
};

P_t_record test1[] = {
	{ 60000, 5001 },
	{ 59000, 5001 },
	{ 58000, 5001 },
	{ 57000, 5001 },
	{ 57000, 5001 },
	{ 56000, 5001 },
	{ 55000, 5001 },
	{ 54000, 5001 },
	{ 53000, 5001 },
	{ 52000, 5001 },
	{ 51000, 5001 },
	{ 50000, 5001 },
	{ 49000, 5001 },
	{ 48000, 5001 },
	{ 47000, 5001 },
	{ 46000, 5001 },
	{ 45000, 5001 }
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

	globals.x2 = button_1_pressed;
	globals.x3 = button_0_pressed;
	
	read_pressure_and_time(pressure_reading_mpsi, time_elapsed_ms);
	globals.current_pressure_mpsi = pressure_reading_mpsi;

	if (globals.timer_t1_initiated) 
		globals.timer_t1_ms += static_cast<uint32_t>(time_elapsed_ms);

	if (globals.five_second_timer_t2_initiated) 
		globals.five_second_timer_t2_ms += static_cast<uint32_t>(time_elapsed_ms);

	_delay_ms(1000); //DEBUG
	flood_saver::main_routine(globals);
	
	lcd.setCursor(0, 0);
	lcd.print(time_elapsed_ms);
	lcd.print("    ");
	lcd.setCursor(8, 0);
	lcd.print("S");
	if (globals.subroutine1_called) lcd.print("1");
	if (globals.subroutine2_called) lcd.print("2");
	lcd.print(" ");
	if (globals.water_supply_alarm_on) lcd.print("WS ");
	if (globals.x4)
		lcd.print("A");
	else
		lcd.print("H");
	lcd.setCursor(0, 1);
	lcd.print(pressure_reading_mpsi);
	lcd.print("    ");
	lcd.setCursor(8, 1);
	lcd.print("T");
	if (globals.timer_t1_initiated) lcd.print('1');
	if (globals.five_second_timer_t2_initiated) {
		lcd.print('2');
		lcd.print('-');
		lcd.print(globals.counter_ct1);
	}
	if (globals.leak_detected) lcd.print("!");
	if (globals.TRACE) lcd.print('*');
	lcd.print("    ");

	if (globals.valve_104_open) {
		digitalWrite(led_green_pin, HIGH);
	}
	else {
		digitalWrite(led_green_pin, LOW);
	}

	if (globals.leak_alarm_on) {
		digitalWrite(led_red_pin, HIGH);
	}
	else {
		digitalWrite(led_red_pin, LOW);
	}

	if (globals.audio_alarm_on) {
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

/*
char analog_reading_str[4];
sprintf(analog_reading_str, "%4d", pressure_reading_deciP);
if (analog_reading_str[2] == ' ') analog_reading_str[2] = '0';
lcd.setCursor(11, 1);
lcd.print(analog_reading_str[0]);
lcd.print(analog_reading_str[1]);
lcd.print(analog_reading_str[2]);
lcd.print('.');
lcd.print(analog_reading_str[3]);
*/