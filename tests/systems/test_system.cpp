#include "systems/quadrotor_obs.hpp"
#include <iostream>
#include <string>
#include <utilities/debug.hpp>

using namespace std;


int main(){
    double width = 1;
    std::vector<std::vector<double>> obs_list;
    obs_list.push_back(std::vector<double> {0., 0., 2.});
    enhanced_system_t* model = new quadrotor_obs_t(obs_list, width);
    
    // initialize cem
    double loss_weights[13] = {1, 1, 1, 
                               0.3, 0.3, 0.3, 0.3,
                               0.3, 0.3, 0.3,
                               0.3, 0.3, 0.3};
    int ns = 1024,
        nt = 5,
        ne = 32,
        max_it = 20;
    double converge_r = 0.1,
           mu_u = 0,
           std_u = 4,
           mu_t = 0.1,
           std_t = 0.2,
           t_max = 0.5,
           dt = 2e-2,
           step_size = 0.5;
   
    const double in_start[13] = {0, 0, 0, 
                          0, 0, 0, 1,
                          0, 0, 0,
                          0, 0, 0,
                          };
    double in_goal[13] = {1, 1, 1, 
                          0, 0, 0, 1,
                          0, 0, 0,
                          0, 0, 0,
                          };;
    double in_radius = 3; 
    
    // Test system propagation
    double const control[4] = {-15, 0, 0, 0};
    double state[13] = {0, 0, 0, 
                        0, 0, 0, 1,
                        0, 0, 0,
                        0, 0, 0};

    check_state_validity(model, state);
    for(unsigned int step = 0; step < 10; step++){
        std::cout << model->propagate(state, model->get_state_dimension(), 
                                      control, model->get_control_dimension(), 
                                      10, state, dt) << std::endl;
        // print_state(model, state);
        // check_state_validity(model, state);

    }
    return 0;
}