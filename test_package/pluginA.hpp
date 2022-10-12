#pragma once 

struct MyDouble {

    MyDouble(double v);
    double value;
};

MyDouble add(MyDouble const& l, MyDouble const& r);