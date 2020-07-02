#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include "systems/cart_pole_obs.hpp"
#include "systems/two_link_acrobot_obs.hpp"
#include "systems/quadrotor_obs.hpp"

#include "trajectory_optimizers/cem.hpp"

#include "networks/mpnet.hpp"
#include "networks/mpnet_cost.hpp"

#include "motion_planners/deep_smp_mpc_sst.hpp"

namespace pybind11 {
    template <typename T>
    using safe_array = typename pybind11::array_t<T, pybind11::array::c_style>;
}

namespace py = pybind11;
using namespace pybind11::literals;

class __attribute__ ((visibility ("hidden"))) DSSTMPCWrapper{
    
public:

	/**
	 * @brief Python wrapper of DSSTMPC planner Constructor
	 * @details Python wrapper of DSSTMPC planner Constructor
	 *
	 */
    DSSTMPCWrapper(
            std::string system_string,
            const py::safe_array<double> &start_state_array,
            const py::safe_array<double> &goal_state_array,
            double goal_radius,
            unsigned int random_seed,
            double sst_delta_near,
            double sst_delta_drain,
            const py::safe_array<double> &obs_list_array,
            double width,
            bool verbose,
            std::string mpnet_weight_path, std::string cost_predictor_weight_path,
            std::string cost_to_go_predictor_weight_path,
            int num_sample,
            int ns, int nt, int ne, int max_it,
            double converge_r, double mu_u, double std_u, double mu_t, double std_t, double t_max, double step_size, double integration_step,
            std::string device_id, float refine_lr, bool normalize,
            py::safe_array<double>& weights_array,
            py::safe_array<double>& obs_voxel_array
    ){
        if(system_string.size()==0){
            system_string="cartpole_obs";
        }
        system_type = system_string;
        auto start_state = start_state_array.unchecked<1>();
        auto goal_state = goal_state_array.unchecked<1>();

        if (obs_list_array.shape()[0] == 0) {
             throw std::runtime_error("Should contain at least one obstacles.");
         }
         if (obs_list_array.shape()[1] != 2) {
             throw std::runtime_error("Shape of the obstacle input should be (N,2).");
         }
         if (width <= 0.) {
             throw std::runtime_error("obstacle width should be non-negative.");
        }
        auto py_obs_list = obs_list_array.unchecked<2>();
        // initialize the array
        obs_list = std::vector<std::vector<double>>(obs_list_array.shape()[0], std::vector<double>(2, 0.0));
        for (unsigned int i = 0; i < obs_list.size(); i++) {
             obs_list[i][0] = py_obs_list(i, 0);
             obs_list[i][1] = py_obs_list(i, 1);
        }


        auto obs_voxel_data = obs_voxel_array.unchecked<1>();
        std::vector<float> obs_vec;
        for (unsigned i=0; i < obs_voxel_data.shape(0); i++)
        {
            obs_vec.push_back(float(obs_voxel_data(i)));
        }
        obs_tensor = torch::from_blob(obs_vec.data(), {1, 1, 32, 32}).to(at::kCUDA);

        auto py_weight_array = weights_array.unchecked<1>();
        for (unsigned int i = 0; i < weights_array.shape()[0]; i++) {
             loss_weights[i] = py_weight_array(i);
        }

        if (system_type == "acrobot_obs"){
            system = new two_link_acrobot_obs_t(obs_list, width);
            distance_computer = two_link_acrobot_obs_t::distance;

        } else if (system_type == "cartpole_obs"){
            system = new cart_pole_obs_t(obs_list, width);
            distance_computer = cart_pole_obs_t::distance;
        } else{
            std::cout<<"undefined system"<<std::endl;
            exit(-1);
        }
        std::cout <<system_type<<std::endl;
        dt = integration_step;
        
        mpnet.reset(
            new networks::mpnet_cost_t(mpnet_weight_path, 
            cost_predictor_weight_path, 
            cost_to_go_predictor_weight_path,
            num_sample, device_id, refine_lr, normalize)
            //  new networks::mpnet_t(mpnet_weight_path)
        );
        std::cout <<""<<std::endl;

        cem.reset(
            new trajectory_optimizers::CEM(
                system, ns, nt,               
                ne, converge_r, 
                mu_u, std_u, 
                mu_t, std_t, t_max, 
                dt, loss_weights, max_it, verbose, step_size)
        );
       
        planner.reset(
            new deep_smp_mpc_sst_t(
                    &start_state(0), &goal_state(0), goal_radius,
                    system->get_state_bounds(),
                    system->get_control_bounds(),
                    distance_computer,
                    random_seed,
                    sst_delta_near, sst_delta_drain, 
                    cem.get(),
                    mpnet.get()
                    )
        );
    }

    py::object neural_step(bool refine, float refine_threshold, bool using_one_step_cost,
        bool cost_reselection, double goal_bias) {
        
        //std::cout << "vector to torch obs vector.." << std::endl;
        double* return_states = new double[system->get_state_dimension()*2]();
        planner -> neural_step(system, dt, obs_tensor, refine, refine_threshold, using_one_step_cost, cost_reselection, return_states, goal_bias);
        py::safe_array<double> terminal_array({system->get_state_dimension()*2}, return_states);
        return terminal_array;
    }

    // /**
	//  * @copydoc planner_t::step()
	//  */
    void step(int min_time_steps, int max_time_steps, double integration_step) {
        planner->step(system, min_time_steps, max_time_steps, integration_step);
    }

    void mpc_step(double integration_step) {
        planner->mpc_step(system, integration_step);
    }

    py::object steer(const py::safe_array<double> &start_array,
                     const py::safe_array<double> &sample_array){

        auto start_state_ref = start_array.unchecked<1>();
        auto sample_array_ref = sample_array.unchecked<1>();

        double* start = new double[system->get_state_dimension()]();
        double* sample = new double[system->get_state_dimension()]();

        for(int si = 0; si < system->get_state_dimension(); si++){
            start[si] = start_state_ref[si];
            sample[si] = sample_array_ref[si];
        }

        double *terminal_state = new double[system->get_state_dimension()]();

        planner->steer(system, start, sample, 
                       terminal_state, dt);
        py::safe_array<double> terminal_array({start_array.shape()[0]}, terminal_state);
        return terminal_array;

    }

    py::object neural_sample(const py::safe_array<double> &state_array,
                             bool refine, float refine_threshold, 
                             bool using_one_step_cost, bool cost_reselection){

                                 auto state_ref = state_array.unchecked<1>();
                                 double * state = new double[system->get_state_dimension()]();
                                 for(int si = 0; si < system->get_state_dimension(); si++){
                                    state[si] = state_ref[si];
                                }

                                 double* neural_sample_state = new double[system->get_state_dimension()]();
                                 planner->neural_sample(system, 
                                                        state, 
                                                        neural_sample_state, 
                                                        obs_tensor, 
                                                        refine, 
                                                        refine_threshold, 
                                                        using_one_step_cost, 
                                                        cost_reselection);
                                py::safe_array<double> neural_sample_state_array({system->get_state_dimension()}, neural_sample_state);
                                return neural_sample_state_array;
                             }

    /**
    * @copydoc sst_backend_t::nearest_vertex()
    */
    py::object nearest_vertex(const py::safe_array<double> &sample_state_array){
        auto sample_state = sample_state_array.unchecked<1>();
        nearest = this -> planner -> nearest_vertex(&sample_state(0));
        const double* nearest_point = nearest -> get_point();
        
        py::safe_array<double> nearest_array({sample_state_array.shape()[0]});
        auto state_ref = nearest_array.mutable_unchecked<1>();
        for (unsigned int i = 0; i < sample_state_array.shape()[0]; i++){
            state_ref(i) = nearest_point[i];
        }
        return nearest_array;
    }

    void add_to_tree(const py::safe_array<double> &sample_state_array,
        double duration
    ){
        auto sample_state = sample_state_array.unchecked<1>();
        const double* sample_control = new double[planner -> get_control_dimension()];
        planner -> add_to_tree(&sample_state(0), sample_control, nearest, duration);       

    }
    /**
	 * @copydoc planner_t::get_solution()
	 */
    py::object get_solution() {
        std::vector<std::vector<double>> solution_path;
        std::vector<std::vector<double>> controls;
        std::vector<double> costs;
        planner->get_solution(solution_path, controls, costs);
        

        if (controls.size() == 0) {
            return py::none();
        }

        py::safe_array<double> controls_array({controls.size(), controls[0].size()});
        py::safe_array<double> costs_array({costs.size()});
        auto controls_ref = controls_array.mutable_unchecked<2>();
        auto costs_ref = costs_array.mutable_unchecked<1>();
        for (unsigned int i = 0; i < controls.size(); ++i) {
            for (unsigned int j = 0; j < controls[0].size(); ++j) {
                controls_ref(i, j) = controls[i][j];
            }
            costs_ref(i) = costs[i];
        }

        py::safe_array<double> state_array({solution_path.size(), solution_path[0].size()});
        auto state_ref = state_array.mutable_unchecked<2>();
        for (unsigned int i = 0; i < solution_path.size(); ++i) {
            for (unsigned int j = 0; j < solution_path[0].size(); ++j) {
                state_ref(i, j) = solution_path[i][j];
            }
        }
        return py::cast(std::tuple<py::safe_array<double>, py::safe_array<double>, py::safe_array<double>>
            (state_array, controls_array, costs_array));
    }

    /**
	 * @copydoc planner_t::get_number_of_nodes()
	 */
    unsigned int get_number_of_nodes() {
        return this->planner->get_number_of_nodes();
    }

    std::string system_type;
protected:
    enhanced_system_t *system;
    std::function<double(const double*, const double*, unsigned int)> distance_computer;
    std::unique_ptr<trajectory_optimizers::CEM> cem;
    std::unique_ptr<networks::mpnet_cost_t> mpnet;
    // std::unique_ptr<networks::mpnet_cost_t> mpnet;

    std::unique_ptr<deep_smp_mpc_sst_t> planner;
    sst_node_t* nearest;
    std::vector<std::vector<double>> obs_list;
    double dt;
    double* loss_weights = new double[4]();
    torch::Tensor obs_tensor;
private:

	/**
	 * @brief Captured distance computer python object to prevent its premature death
	 */
    py::object  _distance_computer_py;
};

PYBIND11_MODULE(_deep_smp_module, m) {
    m.doc() = "Python wrapper for deep smp planners";
    py::class_<DSSTMPCWrapper>(m, "DSSTMPCWrapper")
        .def(py::init<
            std::string,
            const py::safe_array<double>&,
            const py::safe_array<double>&,
            double,
            unsigned int,
            double,
            double,
            const py::safe_array<double>&,
            double,
            bool,
            std::string, std::string, 
            std::string, 
            int,
            int, int, int, int,
            double, double, double, double, double, double, double, double,
            std::string, float, bool,
            py::safe_array<double>&,
            py::safe_array<double>&>(),
            "system_type"_a,
            "start_state"_a,
            "goal_state"_a,
            "goal_radius"_a,
            "random_seed"_a,
            "sst_delta_near"_a,
            "sst_delta_drain"_a,
            "obs_list"_a,
            "width"_a,
            "verbose"_a,
            "mpnet_weight_path"_a, "cost_predictor_weight_path"_a,
            "cost_to_go_predictor_weight_path"_a,
            "num_sample"_a,
            "ns"_a, "nt"_a, "ne"_a, "max_it"_a,
            "converge_r"_a, "mu_u"_a, "std_u"_a, "mu_t"_a, "std_t"_a, "t_max"_a, "step_size"_a, "integration_step"_a,
            "device_id"_a, "refine_lr"_a=0.2, "normalize"_a=true,
            "weights_array"_a,
            "obs_voxel_array"_a
        )
        .def("steer", &DSSTMPCWrapper::steer,
            "start_array"_a,
             "sample_array"_a
        )
        .def("neural_sample", &DSSTMPCWrapper::neural_sample,
            "state_array"_a,
            "refine"_a, 
            "refine_threshold"_a, 
            "using_one_step_cost"_a, 
            "cost_reselection"_a
        )
        .def("step", &DSSTMPCWrapper::step,
            "min_time_steps"_a,
            "max_time_steps"_a,
            "integration_step"_a
        )
        .def("mpc_step", &DSSTMPCWrapper::mpc_step,
            "integration_step"_a
        )
        .def("nearest_vertex", &DSSTMPCWrapper::nearest_vertex,
            "sample_state_array"_a
        )
        .def("add_to_tree", &DSSTMPCWrapper::add_to_tree,
            "sample_state_array"_a,
            "duration"_a
        )
        .def("neural_step", &DSSTMPCWrapper::neural_step,
            "refine"_a=false,
            "refine_threshold"_a=0,
            "using_one_step_cost"_a=false,
            "cost_reselection"_a=false,
            "goal_bias"_a=0
        )
        .def("get_solution", &DSSTMPCWrapper::get_solution)
        .def("get_number_of_nodes", &DSSTMPCWrapper::get_number_of_nodes)
        ;
}
