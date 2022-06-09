#include "RuntimeObject.h"

namespace grunk {

RuntimeObject RuntimeObject::Get(std::string const& memberName) const
{
    auto* member = type_info->GetDataMember(memberName);
    return RuntimeObject(member->GetType(), member->Get(object));
}

void RuntimeObject::Set(std::string const& memberName, RuntimeObject const& obj)
{
    auto* member = type_info->GetDataMember(memberName);
    member->Set(object, obj.object);
}

Reflect::TypeDescriptor const* RuntimeObject::GetTypeInfo() const
{
    return type_info;
}

}