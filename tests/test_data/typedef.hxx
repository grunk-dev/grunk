#pragma once 

template <typename T>
struct TemplateClass
{
    TemplateClass(T const& t) : value(t) {}
    T const& get() const 
    {
        return value;
    }
    T value;
};

struct NonTemplateClass
{
    void foo() {}
};

typedef TemplateClass<int> TemplateClass_int;
using AliasNonTemplateClass = NonTemplateClass;
