#pragma once
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <string>
// #include <ompl/base/StateValidityChecker.h>
#include "systems/enhanced_system.hpp"

class quadrotor_t : public enhanced_system_t
{
public:
	quadrotor_t(unsigned env_id){
		state_dimension = 13;
		control_dimension = 4;
		temp_state = new double[state_dimension];
		deriv = new double[control_dimension]();
	}
	virtual ~quadrotor_t(){
		delete temp_state;
	}
	virtual bool propagate(
		const double* start_state, unsigned int state_dimension,
        const double* control, unsigned int control_dimension,
	    int num_steps, double* result_state, double integration_step);
	void enforce_bounds();
	void enforce_bounds_SO3(double* qstate);
	double distance(double* point1, double* point2);
	virtual bool valid_state();

	protected:
		double* deriv;
		void update_derivative(double* control);
		// ompl::base::StateValidityCheckerPtr svc;
};