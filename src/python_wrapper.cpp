#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <iostream>

#include "systems/point.hpp"
#include "systems/car.hpp"
#include "systems/cart_pole.hpp"
#include "systems/pendulum.hpp"
#include "systems/rally_car.hpp"
#include "systems/two_link_acrobot.hpp"

#include "motion_planners/sst.hpp"
#include "motion_planners/rrt.hpp"

#include "image_creation/planner_visualization.hpp"
#include "systems/distance_functions.h"


//namespace pybind11 {
//    template<typename T, uint8_t n_dim>
//    class safe_array: public array_t<T, pybind11::array::c_style> {
//    private:
//        typedef array_t<T, pybind11::array::c_style> ParentT;
//    public:
//        template <typename... Args>
//        safe_array(Args&&... args)
//            : ParentT(std::forward<Args>(args)...)
//        {
//            // Check if array is not empty and then check dimensions
//            if (ParentT::size() > 0 && ParentT::ndim() != n_dim) {
//                throw std::domain_error("Required array with " + std::to_string(n_dim) + " dimensions. Got " + std::to_string(ParentT::ndim()));
//            }
//        }
//    };
//};


namespace pybind11 {
    template <typename T>
    using safe_array = typename pybind11::array_t<T, pybind11::array::c_style>;
}

namespace py = pybind11;
using namespace pybind11::literals;


py::safe_array<double> get_data(py::safe_array<double>& in_data) {
    //if (in_data.ndim() != 2) throw std::domain_error("error: ndim != 2");
    //in_data.mutable_data(0, 1);
//    double *p = in_data.mutable_data(0);
//    for (ssize_t i = 0; i < in_data.shape(0); i++) {
//        *p += 1;
//        ++p;
//    }

    throw std::domain_error("a");
    return in_data;
}


class PlannerWrapper
{
public:

    void step(system_interface& system, int min_time_steps, int max_time_steps, double integration_step) {
        planner->step(&system, min_time_steps, max_time_steps, integration_step);
    }

    std::string visualize_tree_wrapper(system_t& system) {

        int image_width = 500;
        int image_height = 500;
        double solution_node_diameter = 4;
        double solution_line_width = 3;
        double tree_line_width = 0.5;

        std::vector<std::vector<double>> solution_path;
        std::vector<std::vector<double>> controls;
        std::vector<double> costs;
        planner->get_solution(solution_path, controls, costs);

        using namespace std::placeholders;
        return visualize_tree(
            planner->get_root(), solution_path,
            std::bind(&system_t::visualize_point, &system, _1),
            &system,
            planner->get_start_state(), planner->get_goal_state(),
            image_width, image_height, solution_node_diameter, solution_line_width, tree_line_width);
    }

    std::string visualize_nodes_wrapper(system_t& system) {
        int image_width = 500;
        int image_height = 500;
        double node_diameter = 5;
        double solution_node_diameter = 4;

        std::vector<std::vector<double>> solution_path;
        std::vector<std::vector<double>> controls;
        std::vector<double> costs;
        planner->get_solution(solution_path, controls, costs);

        using namespace std::placeholders;
        return visualize_nodes(
            planner->get_root(), solution_path,
            std::bind(&system_t::visualize_point, &system, _1),
            &system,
            planner->get_start_state(),
            planner->get_goal_state(),
            image_width, image_height, node_diameter, solution_node_diameter);
    }

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
        for (int i = 0; i < controls.size(); ++i) {
            for (int j = 0; j < controls[0].size(); ++j) {
                controls_ref(i, j) = controls[i][j];
            }
            costs_ref(i) = costs[i];
        }

        py::safe_array<double> state_array({solution_path.size(), solution_path[0].size()});
        auto state_ref = state_array.mutable_unchecked<2>();
        for (int i = 0; i < solution_path.size(); ++i) {
            for (int j = 0; j < solution_path[0].size(); ++j) {
                state_ref(i, j) = solution_path[i][j];
            }
        }
        return py::cast(std::tuple<py::safe_array<double>, py::safe_array<double>, py::safe_array<double>>
            (state_array, controls_array, costs_array));
    }

    int get_number_of_nodes() {
        return this->planner->number_of_nodes;
    }

protected:
    std::unique_ptr<planner_t> planner;
};



class SSTWrapper : public PlannerWrapper{
public:
    SSTWrapper(
            const py::safe_array<double> &state_bounds_array,
            const py::safe_array<double> &control_bounds_array,
            const py::safe_array<bool> &is_circular_topology_array,
            const py::safe_array<double> &start_state_array,
            const py::safe_array<double> &goal_state_array,
            double goal_radius,
            unsigned int random_seed,
            double sst_delta_near,
            double sst_delta_drain
    ) {
        if (state_bounds_array.shape()[0] != is_circular_topology_array.shape()[0]) {
            throw std::domain_error("State and topology arrays have to be equal size");
        }

        if (state_bounds_array.shape()[0] != start_state_array.shape()[0]) {
            throw std::domain_error("State bounds and start state arrays have to be equal size");
        }

        if (state_bounds_array.shape()[0] != goal_state_array.shape()[0]) {
            throw std::domain_error("State bounds and goal state arrays have to be equal size");
        }
        auto state_bounds = state_bounds_array.unchecked<2>();
        auto control_bounds = control_bounds_array.unchecked<2>();
        auto is_circular_topology = is_circular_topology_array.unchecked<1>();
        auto start_state = start_state_array.unchecked<1>();
        auto goal_state = goal_state_array.unchecked<1>();

        typedef std::pair<double, double> bounds_t;
        std::vector<bounds_t> state_bounds_v;
        std::vector<bool> is_circular_topology_v;

        for (int i = 0; i < state_bounds_array.shape()[0]; i++) {
            state_bounds_v.push_back(bounds_t(state_bounds(i, 0), state_bounds(i, 1)));
            is_circular_topology_v.push_back(is_circular_topology[i]);
        }

        std::vector<bounds_t> control_bounds_v;
        for (int i = 0; i < control_bounds_array.shape()[0]; i++) {
            control_bounds_v.push_back(bounds_t(control_bounds(i, 0), control_bounds(i, 1)));
        }

        planner.reset(
                new sst_t(
                        &start_state(0), &goal_state(0), goal_radius,
                        state_bounds_v, control_bounds_v,
                        euclidian_distance(is_circular_topology_v),
                        random_seed,
                        sst_delta_near, sst_delta_drain)
        );
    }
};


class RRTWrapper : public PlannerWrapper{
public:
    RRTWrapper(
            const py::safe_array<double> &state_bounds_array,
            const py::safe_array<double> &control_bounds_array,
            const py::safe_array<bool> &is_circular_topology_array,
            const py::safe_array<double> &start_state_array,
            const py::safe_array<double> &goal_state_array,
            double goal_radius,
            unsigned int random_seed
    ) {
        if (state_bounds_array.shape()[0] != is_circular_topology_array.shape()[0]) {
            throw std::runtime_error("State and topology arrays have to be equal size");
        }

        if (state_bounds_array.shape()[0] != start_state_array.shape()[0]) {
            throw std::runtime_error("State bounds and start state arrays have to be equal size");
        }

        if (state_bounds_array.shape()[0] != goal_state_array.shape()[0]) {
            throw std::runtime_error("State bounds and goal state arrays have to be equal size");
        }

        auto state_bounds = state_bounds_array.unchecked<2>();
        auto control_bounds = control_bounds_array.unchecked<2>();
        auto is_circular_topology = is_circular_topology_array.unchecked<1>();
        auto start_state = start_state_array.unchecked<1>();
        auto goal_state = goal_state_array.unchecked<1>();

        typedef std::pair<double, double> bounds_t;
        std::vector<bounds_t> state_bounds_v;
        std::vector<bool> is_circular_topology_v;
        for (int i = 0; i < state_bounds_array.shape()[0]; i++) {
            state_bounds_v.push_back(bounds_t(state_bounds(i, 0), state_bounds(i, 1)));
            is_circular_topology_v.push_back(is_circular_topology(i));
        }

        std::vector<bounds_t> control_bounds_v;
        for (int i = 0; i < control_bounds_array.shape()[0]; i++) {
            control_bounds_v.push_back(bounds_t(control_bounds(i, 0), control_bounds(i, 1)));
        }

        planner.reset(
                new rrt_t(
                        &start_state(0), &goal_state(0), goal_radius,
                        state_bounds_v, control_bounds_v,
                        euclidian_distance(is_circular_topology_v),
                        random_seed)
        );
    }
};


class py_system_interface : public system_interface {
public:

    bool propagate(
        const double* start_state, unsigned int state_dimension,
        const double* control, unsigned int control_dimension,
        int num_steps,
        double* result_state, double integration_step) override
    {
        py::safe_array<double> start_state_array{{state_dimension}};
        std::copy(start_state, start_state + state_dimension, start_state_array.mutable_data(0));

        py::safe_array<double> control_array{{control_dimension}};
        std::copy(control, control + control_dimension, control_array.mutable_data(0));

        py::gil_scoped_acquire gil;
        py::function overload = py::get_overload(static_cast<const system_interface *>(this), "propagate");
        if (!overload) {
            pybind11::pybind11_fail("Tried to call pure virtual function propagate");
            return false;
        }

        auto result = overload(start_state_array, control_array, num_steps, integration_step);
        if (py::isinstance<py::none>(result)) {
            return false;
        } else {
            auto result_state_array = py::detail::cast_safe<py::safe_array<double>>(std::move(result));
            std::copy(result_state_array.data(0), result_state_array.data(0) + state_dimension, result_state);
            return true;
        }
//        //py::safe_array<double> result_state_array{{state_dimension}};
//
//        //return py::detail::cast_safe<bool>(std::move(result));
//        return false;

    }
};


PYBIND11_MODULE(_sst_module, m) {
   m.doc() = "Python wrapper for SST planners";

   m.def("get_data", &get_data, py::arg("in_data").noconvert());

   py::class_<system_interface, py_system_interface> system_interface_var(m, "ISystem");
   system_interface_var
        .def(py::init<>())
        .def("propagate", &system_interface::propagate);
//   system_interface_var
//        .def("get_state_bounds", &system_t::get_state_bounds)
//        .def("get_control_bounds", &system_t::get_control_bounds)
//        .def("is_circular_topology", &system_t::is_circular_topology)
//   ;

   py::class_<system_t> system(m, "System", system_interface_var);
   system
        .def("get_state_bounds", &system_t::get_state_bounds)
        .def("get_control_bounds", &system_t::get_control_bounds)
        .def("is_circular_topology", &system_t::is_circular_topology)
   ;

   py::class_<car_t>(m, "Car", system).def(py::init<>());
   py::class_<cart_pole_t>(m, "CartPole", system).def(py::init<>());
   py::class_<pendulum_t>(m, "Pendulum", system).def(py::init<>());
   py::class_<point_t>(m, "Point", system).def(py::init<>());
   py::class_<rally_car_t>(m, "RallyCar", system).def(py::init<>());
   py::class_<two_link_acrobot_t>(m, "TwoLinkAcrobot", system).def(py::init<>());


   py::class_<PlannerWrapper> planner(m, "PlannerWrapper");
   planner
        .def("step", &PlannerWrapper::step)
        .def("visualize_tree", &PlannerWrapper::visualize_tree_wrapper)
        .def("visualize_nodes", &PlannerWrapper::visualize_nodes_wrapper)
        .def("get_solution", &PlannerWrapper::get_solution)
        .def("get_number_of_nodes", &PlannerWrapper::get_number_of_nodes)
   ;

   py::class_<RRTWrapper>(m, "RRTWrapper", planner)
        .def(py::init<const py::safe_array<double>&,
                      const py::safe_array<double>&,
                      const py::safe_array<bool>&,
                      const py::safe_array<double>&,
                      const py::safe_array<double>&,
                      double,
                      unsigned int>(),
            "state_bounds"_a,
            "control_bounds"_a,
            "is_circular_topology"_a,
            "start_state"_a,
            "goal_state"_a,
            "goal_radius"_a,
            "random_seed"_a
        )
    ;

   py::class_<SSTWrapper>(m, "SSTWrapper", planner)
        .def(py::init<const py::safe_array<double>&,
                      const py::safe_array<double>&,
                      const py::safe_array<bool>&,
                      const py::safe_array<double>&,
                      const py::safe_array<double>&,
                      double,
                      unsigned int,
                      double,
                      double>(),
            "state_bounds"_a,
            "control_bounds"_a,
            "is_circular_topology"_a,
            "start_state"_a,
            "goal_state"_a,
            "goal_radius"_a,
            "random_seed"_a,
            "sst_delta_near"_a,
            "sst_delta_drain"_a
        )
   ;

}
