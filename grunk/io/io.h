#pragma once 

#include <grunk/core/Feature.h>

#include <json/json.h>
#include <initializer_list>

namespace parametric {

std::string serialize(double v) {
    Json::Value j = v;
    Json::FastWriter writer;
    return writer.write(j);
}

std::string serialize(int v) {
    Json::Value j = v;
    Json::FastWriter writer;
    return writer.write(j);
}

template <>
std::string serialize(std::string const& v) {
    Json::Value j = v;
    Json::FastWriter writer;
    return writer.write(j);
}

template <>
std::string serialize(Reflect::DynamicObject const& v)
{
    return Reflect::cast<std::string>(v.invoke("serialize"));
}

} // namespace parametric

namespace grunk {

namespace details {

template <typename Arg>
void parse_feature(Feature<Arg> const& arg, Json::Value& json_root)
{

    class ToStringVisitor
    {
    public:
        ToStringVisitor(Json::Value& root)
         : json(root)
        {}

        void visit(parametric::DAGNode const& n, size_t depth)
        {

            // check if the node has already been parsed
            for( Json::Value::const_iterator itr = json["parameters"].begin() ; itr != json["parameters"].end() ; itr++ ) {
                if (itr.key() == n.id() ) {
                    return;
                }
            }
            for( Json::Value::const_iterator itr = json["steps"].begin() ; itr != json["steps"].end() ; itr++ ) {
                for (auto const& output : (*itr)["outputs"]) {
                    if (output.asString() == n.id() ) {
                        return;
                    }
                }
            }

            if (std::string json_string = n.serialize(); !json_string.empty()){
                
                JSONCPP_STRING err;
                Json::Value json_node;
                Json::CharReaderBuilder builder;
                std::unique_ptr<Json::CharReader> const reader(builder.newCharReader());
                if (!reader->parse(
                        json_string.c_str(), 
                        json_string.c_str() + (int)json_string.length(),
                        &json_node,
                        &err
                     )
                   ) {
                    //TODO: throw exception
                }

                bool is_parameter = ((depth % 2) == 0);
                if (is_parameter) {
                    json["parameters"][n.id()] = json_node;
                }
                else {
                    // is algorithm
                    json["steps"].insert(0, json_node);
                }
            }
        }

    private:
        Json::Value& json;
    };
    
    ToStringVisitor visitor(json_root);
    auto const& node = *(arg.param().node_pointer());
    node.accept(
        visitor,
        0,
        parametric::DAGNode::Direction::up
    );
}

template <typename... Args>
Json::Value parse_feature_tree(Feature<Args> const&... args)
{
    Json::Value json_root;
    
    ([&](auto const& node){
        parse_feature(node, json_root);
    }(args), ...);
    
    //TODO: topological sort for steps

    //TODO: Required plugins
    json_root["requires"]["grunk"] = "0.2.16";
    json_root["requires"]["pluginA"] = "0.1.1";
    json_root["requires"]["pluginB"] = "2.5";

    return json_root;
}

} //namespace details

template <typename... Args>
std::string serialize(Feature<Args> const&... args)
{
    Json::Value root = details::parse_feature_tree(args...);

    Json::StreamWriterBuilder wbuilder;
    wbuilder["indentation"] = "    ";
    return Json::writeString(wbuilder, root);
}

} //namespace grunk