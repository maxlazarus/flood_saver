#pragma once
#include "Arduino.h"
/*
	Name:		state.hpp
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

	class StateMachine {

	private:
		void (StateMachine::*current_state)(const Inputs&, Outputs&) = NULL;

		uint32_t timer_1; // milliseconds
		uint32_t timer_2; // milliseconds
    int32_t last_P; // milli-psi
    uint32_t timer_accumulator; // milliseconds

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

    // Computation functions
    inline int32_t compute_delta_P(int32_t this_P, int32_t last_P, uint32_t delta_t) {
      return (1000 * (this_P - last_P)) / static_cast<int32_t>(delta_t);
    }

	public:
		StateMachine() {
			current_state = &StateMachine::valve_closed_idle;
		}
		void run(const Inputs& in, Outputs& out) { (this->*current_state)(in, out); }
	};

	void StateMachine::valve_closed_reset(const Inputs& in, Outputs& out) {
		
		memcpy(out.message, "Reset   ", 8);

		timer_1 = 0;
		timer_2 = 0;

		out.alarm_audio_on = false;
		out.leak_alarm_on = false;
		out.water_source_alarm_on = false;

		current_state = &StateMachine::valve_closed_idle;
	}

	void StateMachine::valve_closed_idle(const Inputs& in, Outputs& out) {
		
		memcpy(out.message, "Idle    ", 8);
		out.valve_open = false;

    bool delta_P_calculated = false;

    if (timer_accumulator == 0) {
      last_P = in.P;
    }
    if (timer_accumulator > 5000) {
      out.delta_P = compute_delta_P(in.P, last_P, timer_accumulator); 
      delta_P_calculated = true;
      timer_accumulator = 0;
    } 
    else {
      timer_accumulator += in.delta_t;
    }

    if (delta_P_calculated) {
		  if ((out.delta_P > DELTA_P_USE_MIN) && (out.delta_P < DELTA_P_QUIESCENT_MAX)) {
			  current_state = &StateMachine::valve_closed_counting;
		  }
    }
		else if (in.P < P_VALVE_CLOSED_MIN)
			current_state = &StateMachine::valve_open_counting;
	}

	void StateMachine::valve_closed_counting(const Inputs& in, Outputs& out) {
		
		// Put current timer count into message in a super primitive way
		memcpy(out.message, "Count   ", 8);
		char first_digit  = static_cast<char>(48 + (timer_1 / 10));
		char second_digit = static_cast<char>(48 + (timer_1 % 10));
		out.message[6] = first_digit == '0'? ' ' : first_digit;
		out.message[7] = second_digit;

    bool delta_P_calculated = false;

    if (timer_accumulator == 0) {
      last_P = in.P;
    }
    if (timer_accumulator > 5000) {
      out.delta_P = compute_delta_P(in.P, last_P, timer_accumulator); 
      delta_P_calculated = true;
      timer_1 += 1;
      timer_accumulator = 0;
    } 
    else {
      timer_accumulator += in.delta_t;
    }

    if (delta_P_calculated == true) {
		  if (out.delta_P < DELTA_P_USE_MIN) {
        timer_accumulator = 0; 
			  current_state = &StateMachine::valve_closed_idle;
		  } 
		  else if (out.delta_P > DELTA_P_QUIESCENT_MAX) {
		    timer_accumulator = 0; 
		  	current_state = &StateMachine::valve_closed_reset;
		  }
    }
    else if (in.P < P_VALVE_CLOSED_MIN) {
			timer_accumulator = 0; 
			current_state = &StateMachine::valve_open_counting;
    } else if (timer_1 >= T_LEAK_TIMEOUT) {
      timer_accumulator = 0; 
			current_state = &StateMachine::valve_closed_alarmed;
    }
	}

	void StateMachine::valve_open_counting(const Inputs& in, Outputs& out) {
		
		memcpy(out.message, "Opened  ", 8);

		timer_2 += in.delta_t;
		out.valve_open = true;
		
		if (in.P > P_VALVE_OPEN_MAX) {
			timer_2 = 0;
			current_state = &StateMachine::valve_closed_idle;
		} else if (in.P < P_VALVE_SOURCE_MIN)
			current_state = &StateMachine::water_source_fault;
		else {
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

		memcpy(out.message, "WaterSrc", 8);
		out.water_source_alarm_on = true;
		out.valve_open = false;

		if (in.reset_button)
			current_state = &StateMachine::valve_closed_reset;
	}

	void StateMachine::valve_closed_alarmed(const Inputs& in, Outputs& out) {

		memcpy(out.message, "Alarm   ", 8);
		out.valve_open = false;
		out.leak_alarm_on = true;
		out.alarm_audio_on = true;

		if (in.reset_button)
			current_state = &StateMachine::valve_closed_muted;
	}

	void StateMachine::valve_closed_muted(const Inputs& in, Outputs& out) {

		memcpy(out.message, "Muted   ", 8);
		out.alarm_audio_on = false;

		if (in.reset_button)
			current_state = &StateMachine::valve_closed_reset;
	}

	const int32_t StateMachine::DELTA_P_QUIESCENT_MAX	(-300);
	const int32_t StateMachine::DELTA_P_USE_MIN			(-3000);
	const int32_t StateMachine::P_VALVE_OPEN_MAX		(65000);
	const int32_t StateMachine::P_VALVE_CLOSED_MIN		(45000);
	const int32_t StateMachine::P_VALVE_SOURCE_MIN		(35000);

	const uint32_t StateMachine::T_VALVE_OPEN_AWAY		(30000);
	const uint32_t StateMachine::T_VALVE_OPEN_HOME		(1200000);
	const uint32_t StateMachine::T_LEAK_TIMEOUT			(10);
}
