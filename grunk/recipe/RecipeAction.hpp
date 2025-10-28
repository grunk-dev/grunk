#pragma once 

namespace grunk {

class RecipeAction : public parametric::ComputeNode<
                                RecipeAction,
                                parametric::Results<object>, /* Results are ignored*/
                                parametric::Arguments<std::vector<object>> /* Arguments are ignored by derived class */
                            >
{
    friend class RecipeCaller;

private:

    RecipeAction(); //TODO

    inline decltype(auto) result() const {
        return this->template res<object>(0);
    }

    inline decltype(auto) argument(int i) const {
        return this->template arg<object>(i);
    }

public:


    void connect_inputs(std::unordered_map<std::string, DynamicFeature> const& args)
    {
        for (auto const& arg : args) {
            depends_on(arg.second);
            evaluators.push_back(make_evaluator(arg));
        }
    }

    DynamicFeature initialize_results() const
    {
        return feature<object>({});
    }

    void connect_results(DynamicFeature const& res)
    {
        computes(res);
    }

    void eval() const override
    {
        // // tranform input nodes to vector of DynamicObjects
        // std::vector<object> inputs_vec;
        // inputs_vec.reserve(this->num_parents());
        // for (size_t i = 0; i < this->num_parents(); ++i) {
        //     auto const& parent = this->get_parents()[i];
        //     inputs_vec.push_back(evaluators[i](*parent));
        // }

        // // call the wrapped function
        // sol::protected_function_result res = function.call(sol::as_args(inputs_vec));
        // if (!res.valid()) {
        //     sol::error err = res;
        //     throw std::runtime_error(std::string("Error evaluting dynamic action: ") + err.what());
        // }

        // transform to output
        if (auto output = result(); output) {
            output->set_value(res[0]);
        }
    }

};

} // namespace 