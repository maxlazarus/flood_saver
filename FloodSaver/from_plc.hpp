#pragma once

namespace flood_saver {

	struct StateData {

		uint32_t t1_set_time_ms;
		uint32_t timer_t1_ms;
		bool timer_t1_initiated;
		bool timer_t1_alarm_on;

		uint8_t counter_ct1;

		uint16_t five_second_timer_t2_ms;
		bool five_second_timer_t2_initiated;

		bool leak_detected;
		bool leak_alarm_on;
		bool valve_104_open;
		bool audio_alarm_on;
		bool water_supply_alarm_on;

		int32_t current_pressure_mpsi;
		int32_t m1_mpsi;
		int32_t m2_mpsi;

		bool x1; // Start switch pressed, latch on
		bool x2; // Stop/reset switch pressed, latch on
		bool x3; // Mute switch pressed, momentary
		bool x4; // Home/away switch, latch for 'Away' mode
	};

	void subroutine1(StateData& data);
	void subroutine2(StateData& data);
	void main_routine(StateData& data);

	void subroutine1(StateData& data) {
		if (data.x2) {
			if (data.valve_104_open) {
				data.valve_104_open = false;
			}
			if (data.timer_t1_alarm_on) {
				data.timer_t1_alarm_on = false;
			}
			subroutine2(data);
		}
		else {
			if (data.x4) {
				data.t1_set_time_ms = 30000;
			}
			else {
				data.t1_set_time_ms = 1200000;
			}
			if (data.timer_t1_initiated) {
				if (data.timer_t1_ms > data.t1_set_time_ms) {
					data.timer_t1_alarm_on = true;
				}
			}
		}
	}

	void subroutine2(StateData& data) {
		if (data.x2) {
			if (data.leak_alarm_on) {
				data.leak_alarm_on = false;
			}
			data.counter_ct1 = 0;
			data.x1 = false;
			data.x2 = false;
		}
		else {
			if (data.leak_alarm_on) {
				return;
			}
			else {
				if (data.five_second_timer_t2_initiated) {
					if (data.five_second_timer_t2_ms > 5000) {
						data.m2_mpsi = data.current_pressure_mpsi;
						int32_t delta_P_mpsi = (data.m2_mpsi - data.m1_mpsi) / 5;
						if (delta_P_mpsi > -3000 and delta_P_mpsi < -300) {
							// Actual system leak detected
							data.counter_ct1 += 1;
						} 
						else {
							if (delta_P_mpsi > 300) {
								// Inherent or quiescent state detected 
								data.counter_ct1 = 0;
							}
						}
						if (data.counter_ct1 == 10) {
							data.leak_alarm_on = true;
						}
						else {
							data.five_second_timer_t2_ms = 0;
						}
					}
				}
			}
		}
	}

	void main_routine(StateData& data) {
		if (data.x1) {
			if (data.current_pressure_mpsi < 45000) {
				if (data.current_pressure_mpsi < 35000) {
					if (data.valve_104_open) {
						data.water_supply_alarm_on = true;
					}
				}
				else {
					if (data.water_supply_alarm_on) {
						data.water_supply_alarm_on = false;
					}
				}
				if (data.timer_t1_ms > data.t1_set_time_ms) {
					data.leak_alarm_on = true;
					data.x1 = false;
					data.valve_104_open = false;
				} 
				else {
					if (data.leak_detected) {
						data.leak_alarm_on = true;
						data.x1 = false;
						data.valve_104_open = false;
					}
					else {
						data.valve_104_open = true;
						subroutine1(data);
					}
				}
			}
			else {
				if (data.valve_104_open) {
					subroutine1(data);
				}
				else {
					subroutine2(data);
					return;
				}
			}
		}
		else {
			if (data.x2) {
				subroutine1(data);
			}
			else {
				if (data.audio_alarm_on) {
					if (data.x3) {
						data.audio_alarm_on = false;
					}
				}
				return;
			}
		}
		while (data.x2) {
			if (not (data.leak_alarm_on or data.timer_t1_alarm_on)) {
				break;
			}
			subroutine1(data);
		}
		if (not (data.timer_t1_ms > data.t1_set_time_ms)) {
			if (not data.leak_detected) {
				if (not data.current_pressure_mpsi > 65000) {
					return;
				}
			}
		}
		data.valve_104_open = false;
		subroutine2(data);
		return;
	}
}
