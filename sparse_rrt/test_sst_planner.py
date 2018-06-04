
import _sst_module
from sparse_rrt.systems import standard_cpp_systems
import numpy as np
import time

from sparse_rrt.systems.acrobot import Acrobot, AcrobotDistance
from sparse_rrt.systems.point import Point


def test_point_sst():
    system = standard_cpp_systems.Point()

    planner = _sst_module.SSTWrapper(
        state_bounds=system.get_state_bounds(),
        control_bounds=system.get_control_bounds(),
        distance=system.distance_computer(),
        start_state=np.array([0., 0.]),
        goal_state=np.array([9., 9.]),
        goal_radius=0.5,
        random_seed=0,
        sst_delta_near=0.4,
        sst_delta_drain=0.2
    )

    number_of_iterations = 410000

    min_time_steps = 20
    max_time_steps = 200
    integration_step = 0.002

    print("Starting the planner.")

    start_time = time.time()

    expected_results = {
        0: (1, None),
        100000: (4900, 2.486),
        200000: (5291, 2.072),
        300000: (5436, 1.996),
        400000: (5611, 1.988),
        'final': (5629, 1.988)
    }

    for iteration in range(number_of_iterations):
        planner.step(system, min_time_steps, max_time_steps, integration_step)
        if iteration % 100000 == 0:
            solution = planner.get_solution()

            expected_number_of_nodes, expected_solution_cost = expected_results[iteration]
            assert(expected_number_of_nodes == planner.get_number_of_nodes())

            if solution is None:
                solution_cost = None
                assert(expected_solution_cost is None)
            else:
                solution_cost = np.sum(solution[2])
                assert(abs(solution_cost - expected_solution_cost) < 1e-9)

            print("Time: %.2fs, Iterations: %d, Nodes: %d, Solution Quality: %s" %
                  (time.time() - start_time, iteration, planner.get_number_of_nodes(), solution_cost))

    path, controls, costs = planner.get_solution()
    solution_cost = np.sum(costs)

    print("Time: %.2fs, Iterations: %d, Nodes: %d, Solution Quality: %f" %
          (time.time() - start_time, number_of_iterations, planner.get_number_of_nodes(), solution_cost))

    expected_number_of_nodes, expected_solution_cost = expected_results['final']
    assert(planner.get_number_of_nodes() == expected_number_of_nodes)
    assert(abs(solution_cost - expected_solution_cost) < 1e-9)


def test_create_multiple_times():
    '''
    There used to be a crash during construction
    '''
    system = standard_cpp_systems.CartPole()
    planners = []
    for i in range(100):
        planner = _sst_module.SSTWrapper(
            state_bounds=system.get_state_bounds(),
            control_bounds=system.get_control_bounds(),
            distance=system.distance_computer(),
            start_state=np.array([-20, 0, 3.14, 0]),
            goal_state=np.array([20, 0, 3.14, 0]),
            goal_radius=1.5,
            random_seed=0,
            sst_delta_near=2.,
            sst_delta_drain=1.2
        )
        min_time_steps = 10
        max_time_steps = 50
        integration_step = 0.02

        for iteration in range(100):
            planner.step(system, min_time_steps, max_time_steps, integration_step)
        planners.append(planner)


def test_py_system_sst():

    system = Point()

    planner = _sst_module.SSTWrapper(
        state_bounds=system.get_state_bounds(),
        control_bounds=system.get_control_bounds(),
        distance=system.distance_computer(),
        start_state=np.array([0.2, 0.1]),
        goal_state=np.array([5., 5.]),
        goal_radius=1.5,
        random_seed=0,
        sst_delta_near=0.6,
        sst_delta_drain=0.4
    )

    min_time_steps = 10
    max_time_steps = 50
    integration_step = 0.02

    for iteration in range(1000):
        planner.step(system, min_time_steps, max_time_steps, integration_step)
        im = planner.visualize_tree(system)


def test_py_system_sst_custom_distance():
    '''
    Check that distance overriding in python works
    '''

    system = Acrobot()

    planner = _sst_module.SSTWrapper(
        state_bounds=system.get_state_bounds(),
        control_bounds=system.get_control_bounds(),
        # use custom distance computer
        distance=AcrobotDistance(),
        start_state=np.array([0., 0., 0., 0.]),
        goal_state=np.array([np.pi, 0., 0., 0.]),
        goal_radius=2.,
        random_seed=0,
        sst_delta_near=0.6,
        sst_delta_drain=0.4
    )

    min_time_steps = 10
    max_time_steps = 50
    integration_step = 0.02

    for iteration in range(100):
        planner.step(system, min_time_steps, max_time_steps, integration_step)
        im = planner.visualize_tree(system)


if __name__ == '__main__':
    st = time.time()
    test_point_sst()
    print("Current test time: %fs (baseline: %fs)" % (time.time() - st, 21.4076721668))
    test_create_multiple_times()
    test_py_system_sst()
    test_py_system_sst_custom_distance()
    print('Passed all tests!')