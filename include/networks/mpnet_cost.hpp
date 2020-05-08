#ifndef MPNET_COST_HPP
#define MPNET_COST_HPP

#include "networks/mpnet.hpp"
#include "systems/enhanced_system.hpp"

namespace networks{
    class mpnet_cost_t : public mpnet_t
    {
    public:
        mpnet_cost_t(std::string network_weights_path, std::string cost_predictor_weights_path, int num_sample);
        at::Tensor forward(std::vector<torch::jit::IValue> input_container);
        at::Tensor forward_cost(std::vector<torch::jit::IValue> input_container);
        virtual void mpnet_sample(enhanced_system_t* system, torch::Tensor env_vox_tensor, 
            const double* state,  double* goal_state, double* neural_sample_state) override;
        int num_sample;
        ~mpnet_cost_t();
    protected:
        std::shared_ptr<torch::jit::script::Module> network_torch_module_ptr;
        std::shared_ptr<torch::jit::script::Module> cost_predictor_torch_module_ptr;

    };
}
#endif
