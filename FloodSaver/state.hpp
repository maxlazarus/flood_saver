#pragma once
namespace flood_saver {

	struct Inputs {
		
		int32_t P;			// milli-psi
		int32_t delta_P;	// milli-psi/second
		uint32_t delta_t;	// milliseconds
		bool reset_button;
		bool away_switch_on;
	};

	struct Outputs {
		
		char message[8];
		bool valve_open;
		bool alarm_audio_on;
		bool leak_alarm_on;
		bool water_source_alarm_on;
	};

	class StateMachine {

	private:
		void (StateMachine::*current_state)(Inputs&, Outputs&) = NULL;

		uint32_t timer_1;
		uint32_t timer_2;

		static const int32_t DELTA_P_QUIESCENT_MAX;	// milli-psi/second
		static const int32_t DELTA_P_USE_MIN;		// milli-psi/second
		static const int32_t P_VALVE_OPEN_MAX;		// milli-psi
		static const int32_t P_VALVE_CLOSED_MIN;		// milli-psi
		static const int32_t P_VALVE_SOURCE_MIN;	// milli-psi

		static const uint32_t T_VALVE_OPEN_AWAY;	// milliseconds
		static const uint32_t T_VALVE_OPEN_HOME;	// milliseconds
		static const uint32_t T_LEAK_TIMEOUT;		// milliseconds

		// State prototypes
		inline void valve_closed_reset(Inputs& in, Outputs& out);
		inline void valve_closed_idle(Inputs& in, Outputs& out);
		inline void valve_closed_counting(Inputs& in, Outputs& out);
		inline void valve_open_counting(Inputs& in, Outputs& out);
		inline void water_source_fault(Inputs& in, Outputs& out);
		inline void valve_closed_alarmed(Inputs& in, Outputs& out);
		inline void valve_closed_muted(Inputs& in, Outputs& out);

	public:
		StateMachine() {
			current_state = &StateMachine::valve_closed_idle;
		}
		void run(Inputs& in, Outputs& out) { (this->*current_state)(in, out); }
	};

	void StateMachine::valve_closed_reset(Inputs& in, Outputs& out) {
		
		memcpy(out.message, "Reset   ", 8);
		timer_1 = 0;
		timer_2 = 0;

		out.alarm_audio_on = false;
		out.leak_alarm_on = false;
		out.water_source_alarm_on = false;

		current_state = &StateMachine::valve_closed_idle;
	}

	void StateMachine::valve_closed_idle(Inputs & in, Outputs & out) {
		
		memcpy(out.message, "Idle    ", 8);
		out.valve_open = false;

		if ((in.delta_P > DELTA_P_USE_MIN) && (in.delta_P < DELTA_P_QUIESCENT_MAX))		
			current_state = &StateMachine::valve_closed_counting;
		else if (in.P < P_VALVE_CLOSED_MIN)
			current_state = &StateMachine::valve_open_counting;
	}

	void StateMachine::valve_closed_counting(Inputs& in, Outputs& out) {
		
		memcpy(out.message, "Counting", 8);
		timer_1 += in.delta_t;

		if (in.delta_P < DELTA_P_USE_MIN)
			current_state = &StateMachine::valve_closed_idle;
		else if (in.delta_P > DELTA_P_QUIESCENT_MAX)
			current_state = &StateMachine::valve_closed_reset;
		else if (in.P < P_VALVE_CLOSED_MIN)
			current_state = &StateMachine::valve_open_counting;
		else if (timer_1 > T_LEAK_TIMEOUT)
			current_state = &StateMachine::valve_closed_alarmed;
	}

	void StateMachine::valve_open_counting(Inputs& in, Outputs& out) {
		
		memcpy(out.message, "Counting", 8);
		timer_2 += in.delta_t;
		out.valve_open = true;
		
		if (in.P > P_VALVE_OPEN_MAX)
			current_state = &StateMachine::valve_closed_idle;
		else if (in.P < P_VALVE_SOURCE_MIN)
			current_state = &StateMachine::water_source_fault;
		else {
			if (in.away_switch_on) {
				if (timer_2 > T_VALVE_OPEN_AWAY)
					current_state = &StateMachine::valve_closed_alarmed;
			}
			else {
				if (timer_2 < T_VALVE_OPEN_HOME)
					current_state = &StateMachine::valve_closed_alarmed;
			}
		}
	}

	void StateMachine::water_source_fault(Inputs& in, Outputs& out) {

		memcpy(out.message, "WaterSrc", 8);
		out.water_source_alarm_on = true;

		if (in.reset_button)
			current_state = &StateMachine::valve_closed_reset;
	}

	void StateMachine::valve_closed_alarmed(Inputs& in, Outputs& out) {

		memcpy(out.message, "Alarm   ", 8);
		out.valve_open = false;
		out.leak_alarm_on = true;
		out.alarm_audio_on = true;

		if (in.reset_button)
			current_state = &StateMachine::valve_closed_muted;
	}

	void StateMachine::valve_closed_muted(Inputs& in, Outputs& out) {

		memcpy(out.message, "Muted   ", 8);
		out.alarm_audio_on = false;

		if (in.reset_button)
			current_state = &StateMachine::valve_closed_reset;
	}

	const int32_t StateMachine::DELTA_P_QUIESCENT_MAX(-300);
	const int32_t StateMachine::DELTA_P_USE_MIN(-3000);
	const int32_t StateMachine::P_VALVE_OPEN_MAX(65);
	const int32_t StateMachine::P_VALVE_CLOSED_MIN(45);
	const int32_t StateMachine::P_VALVE_SOURCE_MIN(35);

	const uint32_t StateMachine::T_VALVE_OPEN_AWAY(30000);
	const uint32_t StateMachine::T_VALVE_OPEN_HOME(1200000);
	const uint32_t StateMachine::T_LEAK_TIMEOUT(50000);
}