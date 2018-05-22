#pragma once
#include "Arduino.h"
/*
	Name:		state.h
	Created:	8/16/2017 9:06:19 PM
	Author:	Maxim Prokopenko

	Written for Kirk Dobbs and Len Shankland.
	State machine controlling inputs and outputs for the patent "Smart Building
	Water Supply Management System With Leak Detection And Flood Prevention".
*/

namespace flood_saver {

	struct Inputs {
		int32_t P;			// milli-psi
		uint32_t delta_t;	// milliseconds
		bool reset_button;
		bool away_switch_on;
	};

	struct Outputs {
    int32_t delta_P; // milli-psi/second
		char message[8];
		bool valve_open;
		bool alarm_audio_on;
		bool leak_alarm_on;
		bool water_source_alarm_on;
	};

  	class Accumulator {
  	protected:
    	uint32_t i;
    	uint32_t timeout;
  	public:
    	Accumulator(uint32_t timeout) : timeout(timeout) {}

    	void update(uint32_t elapsed) {
    		i += elapsed;
    	}

		void reset() {
    		i = 0;
		}

		void reset_keep_remainder() {
    		i = i % timeout;
    	}

		uint32_t count() {
			return i;
		}

		uint32_t overflows() {
			return i / timeout;
		}
	};

	class PressureAccumulator : public Accumulator {
		int32_t last_P;
	public:
		PressureAccumulator(uint32_t timeout) : Accumulator(timeout) {}
    
    	bool update(uint32_t elapsed, int32_t P, int32_t &flow_rate_result) {
			// Returns true if accumulator has overflowed
			i += elapsed;
			if (last_P == 0) {
				last_P = P;
			} else if (last_P - P > 4000) {
				flow_rate_result = (1000 * (P - last_P)) / static_cast<int32_t>(i);
				last_P = P;
				i = 0;
				return true;
			} else if ((last_P - P < 0) && (overflows() > 0)) {
                flow_rate_result = (1000 * (P - last_P)) / static_cast<int32_t>(i);
                last_P = P;
                i = 0;
			}
			return false;
		}

		void clear() {
			i = 0;
			last_P = 0;
		}
	};
  
	class StateMachine {

	private:
		void (StateMachine::*current_state)(const Inputs&, Outputs&) = NULL;

		uint32_t timer_1; // milliseconds
		uint32_t timer_2; // milliseconds
		int32_t last_P; // milli-psi
		static PressureAccumulator pressure_accumulator; // milli-psi?
		static Accumulator count_reset_accumulator; // milliseconds

		// Constants, defined at bottom of this file
		static const int32_t DELTA_P_QUIESCENT_MAX;	// milli-psi/second
		static const int32_t DELTA_P_USE_MIN;		// milli-psi/second
		static const int32_t P_VALVE_OPEN_MAX;		// milli-psi
		static const int32_t P_VALVE_CLOSED_MIN;	// milli-psi
		static const int32_t P_VALVE_SOURCE_MIN;	// milli-psi

		static const uint32_t T_VALVE_OPEN_AWAY;	// milliseconds
		static const uint32_t T_VALVE_OPEN_HOME;	// milliseconds
		static const uint32_t T_LEAK_TIMEOUT;		// milliseconds

		// State prototypes
		inline void valve_closed_reset		(const Inputs& in, Outputs& out);
		inline void valve_closed_idle		(const Inputs& in, Outputs& out);
		inline void valve_closed_counting	(const Inputs& in, Outputs& out);
		inline void valve_open_counting		(const Inputs& in, Outputs& out);
		inline void water_source_fault		(const Inputs& in, Outputs& out);
		inline void valve_closed_alarmed	(const Inputs& in, Outputs& out);
		inline void valve_closed_muted		(const Inputs& in, Outputs& out);
		
		// Convenience
		void service_count_reset(const uint32_t delta_t) {
			count_reset_accumulator.update(delta_t);
			if (count_reset_accumulator.overflows()) {
				count_reset_accumulator.reset();
				timer_1 = 0;
			}
		}

	public:
		StateMachine() {
			current_state = &StateMachine::valve_closed_idle;
		}
		void run(const Inputs& in, Outputs& out) { (this->*current_state)(in, out); }
	};

	void StateMachine::valve_closed_reset(const Inputs& in, Outputs& out) {

    	pressure_accumulator.update(in.delta_t, in.P, out.delta_P);
		count_reset_accumulator.reset();
		
		memcpy(out.message, "Reset   ", 8);

		timer_1 = 0;
		timer_2 = 0;

		out.alarm_audio_on = false;
		out.leak_alarm_on = false;
		out.water_source_alarm_on = false;

		current_state = &StateMachine::valve_closed_idle;
	}

	void StateMachine::valve_closed_idle(const Inputs& in, Outputs& out) {
		
		memcpy(out.message, "Off     ", 8);
		out.valve_open = false;
		
		StateMachine::service_count_reset(in.delta_t);
		
		count_reset_accumulator.update(in.delta_t);
		if (count_reset_accumulator.overflows()) {
			count_reset_accumulator.reset();
			timer_1 = 0;
		}

    	if (pressure_accumulator.update(in.delta_t, in.P, out.delta_P)) {
			if ((out.delta_P > DELTA_P_USE_MIN) && (out.delta_P < DELTA_P_QUIESCENT_MAX)) {
				current_state = &StateMachine::valve_closed_counting;
			}
    	}
		else if (in.P < P_VALVE_CLOSED_MIN) {
			current_state = &StateMachine::valve_open_counting;
		}
	}

	void StateMachine::valve_closed_counting(const Inputs& in, Outputs& out) {
		
		// Put current timer count into message in a super primitive way
		memcpy(out.message, "Count   ", 8);
		char first_digit  = static_cast<char>(48 + (timer_1 / 10));
		char second_digit = static_cast<char>(48 + (timer_1 % 10));
		out.message[6] = first_digit == '0'? ' ' : first_digit;
		out.message[7] = second_digit;
		
		StateMachine::service_count_reset(in.delta_t);

    	if (in.P < P_VALVE_CLOSED_MIN) {
            current_state = &StateMachine::valve_open_counting;
        }
        else if (pressure_accumulator.update(in.delta_t, in.P, out.delta_P)) {
    		// Pressure accumulator has overflowed
    		timer_1 += 1;  
			count_reset_accumulator.reset();
		  	if (out.delta_P < DELTA_P_USE_MIN) {
				current_state = &StateMachine::valve_closed_idle;
		  	} 
		  	else if (out.delta_P > DELTA_P_QUIESCENT_MAX) {
				current_state = &StateMachine::valve_closed_reset;
			}
    	}
    	if (timer_1 >= T_LEAK_TIMEOUT) {
			current_state = &StateMachine::valve_closed_alarmed;
    	}
	}

	void StateMachine::valve_open_counting(const Inputs& in, Outputs& out) {
		
		memcpy(out.message, "On      ", 8);
		
		StateMachine::service_count_reset(in.delta_t);

		timer_2 += in.delta_t;
		out.valve_open = true;
    	pressure_accumulator.update(in.delta_t, in.P, out.delta_P);
    
		if (in.P > P_VALVE_OPEN_MAX) {
			timer_2 = 0;
      		pressure_accumulator.clear();
			current_state = &StateMachine::valve_closed_idle;
		} else if (in.P < P_VALVE_SOURCE_MIN) {
      		timer_2 = 0;
			current_state = &StateMachine::water_source_fault;
		} else {
			if (in.away_switch_on) {
				if (timer_2 > T_VALVE_OPEN_AWAY) {
					current_state = &StateMachine::valve_closed_alarmed;
				}
			}
			else {
				if (timer_2 > T_VALVE_OPEN_HOME) {
					current_state = &StateMachine::valve_closed_alarmed;
				}
			}
		}
	}

	void StateMachine::water_source_fault(const Inputs& in, Outputs& out) {

    	pressure_accumulator.update(in.delta_t, in.P, out.delta_P);
		memcpy(out.message, "WaterSrc", 8);
		out.water_source_alarm_on = true;
    	out.alarm_audio_on = true;
    	timer_2 += in.delta_t;  
		
		StateMachine::service_count_reset(in.delta_t);

    	if (in.away_switch_on) {
    		if (timer_2 > T_VALVE_OPEN_AWAY) {
    			out.valve_open = false;
    		}
    		else {
    			out.valve_open = true;
    		}
    	}
    	else {
    		if (timer_2 > T_VALVE_OPEN_HOME) {
        		out.valve_open = false;
        		current_state = &StateMachine::valve_closed_alarmed;
    		}
    		else { 
        		out.valve_open = true;
    		}
    	}

    	if (in.P > P_VALVE_SOURCE_MIN) {
    		out.water_source_alarm_on = false;
    		out.alarm_audio_on = false;
    		timer_2 = 0;
    		current_state = &StateMachine::valve_closed_idle;
    	} else if (in.reset_button) {
			current_state = &StateMachine::valve_closed_muted;
    	}
	}

	void StateMachine::valve_closed_alarmed(const Inputs& in, Outputs& out) {

		pressure_accumulator.update(in.delta_t, in.P, out.delta_P);
		memcpy(out.message, "Alarm   ", 8);
		out.valve_open = false;
		out.leak_alarm_on = true;
		out.alarm_audio_on = true;

		if (in.reset_button)
			current_state = &StateMachine::valve_closed_muted;
	}

	void StateMachine::valve_closed_muted(const Inputs& in, Outputs& out) {

    	pressure_accumulator.update(in.delta_t, in.P, out.delta_P);
		memcpy(out.message, "Muted   ", 8);
		out.alarm_audio_on = false;

		if (in.reset_button)
			current_state = &StateMachine::valve_closed_reset;
	}

	const int32_t StateMachine::DELTA_P_QUIESCENT_MAX	(-56);
	const int32_t StateMachine::DELTA_P_USE_MIN			(-560);
	const int32_t StateMachine::P_VALVE_OPEN_MAX		(65000);
	const int32_t StateMachine::P_VALVE_CLOSED_MIN		(45000);
	const int32_t StateMachine::P_VALVE_SOURCE_MIN		(35000);

	const uint32_t StateMachine::T_VALVE_OPEN_AWAY		(30000);
	const uint32_t StateMachine::T_VALVE_OPEN_HOME		(1200000);
	const uint32_t StateMachine::T_LEAK_TIMEOUT			(30);
  	
	PressureAccumulator StateMachine::pressure_accumulator(2000);
	Accumulator StateMachine::count_reset_accumulator(300000);
}
