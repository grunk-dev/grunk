#include <grunk/script.hpp>

namespace grunk {

ResultHolder<DynamicAction> script(
    std::vector<Script::Step> const& steps,
    std::vector<std::string> const& returns
)
{
    auto ptr = std::shared_ptr<Script>(new Script(steps, returns));
    return ResultHolder<DynamicAction>(
        parametric::compute(ptr, steps),
        ptr
    );
}

} // namespace grunk