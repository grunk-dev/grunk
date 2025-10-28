#include "RecipeAction.hpp"

namespace grunk {

    decltype(auto) RecipeAction::result() const {
        return this->template res<object>(0);
    }

    decltype(auto) RecipeAction::argument(int i) const {
        return this->template arg<object>(i);
    }

} // namespace grunk