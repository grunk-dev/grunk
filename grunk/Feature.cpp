#include "Feature.h"

namespace grunk {

// Feature Feature::Get(std::string const& memberName) const
// {
//     class MemberGetter : public  parametric::ComputeNode
//     {
//     public:
//         MemberGetter(parametric::param<RuntimeObject> const& the_struct, std::string const& member ) 
//             : in(the_struct)
//             , member_str(member)
//         {
//             this->depends_on(in);
//             computes(out, parametric::param<RuntimeObject>(member_str));
//         }

//         void eval() const override
//         {
//             if (!out.expired())

//                 if ( auto const& member = in.value().Get(member_str); member.GetTypeInfo()->GetName() == "Feature" ) {
//                     // the member already is a feature. unwrap the contained parameter
//                     //TODO: Expensive copy! Would be better to use references or pointers here
//                     out.set_value(member.cast<Feature>().param.value());
//                 }
//                 else {
//                     //TODO: Expensive copy! Would be better to use references or pointers here
//                     out.set_value(member);
//                 }
//         }

//         parametric::param<RuntimeObject> result()
//         {
//             return out.param();
//         }

//     private:
//         std::string member_str;
//         parametric::param<RuntimeObject> in;
//         mutable parametric::OutputParam<RuntimeObject> out;
//     };

//     auto computeNode = parametric::new_node<MemberGetter>(param, memberName);
//     return Feature(computeNode->result());
// }

} //namespace grunk