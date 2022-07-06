#include "RuntimeObject.h"
#include <stdexcept>

namespace grunk {

RuntimeObject RuntimeObject::get(std::string const& memberName) const
{
    auto* member = type_info->GetDataMember(memberName);
    if (!member) {
        throw std::invalid_argument("RuntimeObject::Get: No member \"" + memberName + "\" found for Type \"" + type_info->GetName() + "\"");
    }
    return RuntimeObject(member->GetType(), member->Get(object));
}

void RuntimeObject::set(std::string const& memberName, RuntimeObject const& obj)
{
    auto* member = type_info->GetDataMember(memberName);
    if (!member) {
        throw std::invalid_argument("RuntimeObject::Set: No member \"" + memberName + "\" found for Type \"" + type_info->GetName() + "\"");
    }
    if (member->GetType()->GetName() != obj.get_type_info()->GetName()) {
        throw std::invalid_argument("RuntimeObject::Set: Argument type \""
                                    + obj.get_type_info()->GetName()
                                    + "\" and member type \""
                                    + member->GetType()->GetName()
                                    + "\" are incompatible.");
    }
    member->Set(object, obj.object);
}

Reflect::TypeDescriptor const* RuntimeObject::get_type_info() const
{
    return type_info;
}

}
