#pragma once 

#include <grunk/core/Feature.h>

#include <json/json.h>
#include <initializer_list>

namespace grunk {

namespace details {

template <typename Arg>
Json::Value parse_feature(Feature<Arg> const& arg)
{
    Json::Value node;

    return node;
}

template <typename... Args>
Json::Value parse_feature_tree(Feature<Args> const&... args)
{
    Json::Value nodes;
    
    [&](auto const& node){
        if (!nodes[node.name()]) {
            nodes[node.name()] = parse_feature(node);
        }
    }(args...);

    return nodes;
}

} //namespace details

template <typename... Args>
void write(std::string const& filename, Feature<Args> const&... args)
{
    Json::Value root;
    root["nodes"] = details::parse_feature_tree(args...);

    Json::StreamWriterBuilder wbuilder;
    wbuilder["indentation"] = "    ";
    std::string out = Json::writeString(wbuilder, root);
}

} //namespace grunk