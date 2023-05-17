#pragma once 

template <typename T>
struct opencascade_handle {
    T* operator*();
    T* t;
};

struct Standard_Transient {};

struct Foo : public Standard_Transient {};

struct Bar {};