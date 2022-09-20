*************
Usage 
*************

.. _getting-started:

Getting Started
===============

As an introductory example, consider the following function
that adds two ``double``\s and is a bit talkative about it.

.. code-block:: cpp
   
   double add(double const& l, double const r)
   {
       std::cout << "Adding " << l << " and " << r << std::endl;
       return l + r;
   };

grunk let's you delay the evaluation of the function until the result is queried.

.. code-block:: cpp

   #include <grunk/grunk.h>

   auto o = grunk::eval("o", &add, 1.2, 40.8)->get();
   
   std::cout << "Until here, nothing has happened" << std::endl;

   std::cout << o.value() << std::endl;
   std::cout << o.value() << std::endl;

.. code-block:: console

   Until here, nothing has happened
   Adding 1.2 and 40.8
   42
   42

In the first line no computation takes place, the function ``add`` is not evaluated.
Instead, 
a new ``Algorithm`` instance ``o`` is created using ``grunk::eval``.
The arguments are the label ``"o"``, a function pointer ``&add`` and two arguments
that shall be passed into the function. With ``->get()`` we retrieve a handle
to the first (and in this case only) output of the algorithm, which is of
type ``Feature<double>``. 

Internally, ``o`` depends on the algorithm created
by ``grunk::eval``, which in turn depends on two ``Feature<double>`` instances, 
one holding the value ``1.2`` and the other the value ``40.8``. With this dependency 
information, grunk can delay the computation to the point when ``o.value()`` 
is called. At this time, the function must be evaluated and the result, ``42``, is 
stored in ``o``. The next time the value is queried, the compuation is not repeated,
the cached result is returned. This is called **lazy evaluation**, because computations 
are delayed until the last point possible and only necessary calculations are performed.

Let's modify the above code example a bit. 

.. code-block:: cpp
   
   grunk::Feature x("x", 1.2);
   grunk::Feature y("y", 15.2);
   grunk::Feature z("z", 25.6);

   auto a = grunk::eval("a", &add, x, y)->get();
   auto b = grunk::eval("b", &add, a, z)->get();

We have created three independent features ``x,y,z``. The feature
``a`` is the result of adding ``x`` and ``y`` and the feature ``b`` is the 
result of adding ``a`` and ``z``.

If we would now query the value of ``a``, ``b`` would 
not be computed, because ``a`` does not depend on ``b``. If instead,
we were to query the value of ``b``, ``a`` would have to be computed first. 
Let's try this:

.. code-block:: cpp
   
   std::cout << b.value() << std::endl;

.. code-block:: console
   
   Adding 1.2 and 15.2
   Adding 16.4 and 25.6
   42

If we now were to change ``z``, ``b`` would have to be recomputed the 
next time its value is queried. ``a`` which does not depend on ``z``
does not need to be recomputed.

.. code-block:: cpp
   
   z.access_value() = 24.2;
   
   std::cout << "No calculations done until this point" << std::endl;
   std::cout  << b.value() << std::endl;

.. code-block:: console 
   
   No calculations done until this point
   Adding 16.4 and 24.2
   40.6

If we were to change ``x``, both ``a`` and ``b`` have to be re-computed and 
grunk knows this:

.. code-block:: cpp 
   
   x.access_value() = 2.6;

   std::cout << "No calculations done until this point" << std::endl;
   std::cout  << b.value() << std::endl;

.. code-block:: console
   
   No calculations done until this point
   Adding 2.6 and 15.2
   Adding 17.8 and 24.2
   42

This functionality of only invalidating those features, that depend 
on a changed feature is called **automatic invalidation**. 

Lazy evaluation and automatic invalidation come with the trade-off of having to 
store dependency information, but it pays of
for big workflows with many computationally expensive functions. This becomes especially 
apparent in explorative design or automated optimization workflows, where input parameters 
can be expected to be altered frequently.

Conceptually, the ``Feature<double>`` correspond to a node in a *directed acyclic graph* 
(DAG). This graph is often called a **feature tree**. grunk retains these feature trees 
by retaining parent-child relations in the ``Feature<T>`` instances.


.. _using-plugins:

Using Plugins
=============

What kind of types can I store in a ``Feature``? What kind of functions can I 
use with grunk?

(Almost) anything goes: Any kind of type can be stored in a ``Feature``. Functions have
to be **referentially transparent**, which means they may not alter their inputs.
Internally, grunk checks if a function passed to ``grunk::eval`` can be invoked given only
`const` references. 

The power of grunk comes with its plugin system. A grunk plugin supplies types and 
functions that can be used as building blocks of a feature tree. Imagine that instead of 
using our ``add`` function from above, we now want to build a feature tree with the functions
``SomePluginA::add`` and ``SomePluginB::multiply``, both taking instances of 
``SomePluginA::MyDouble`` as arguments. 

Let's assume that we have both plugins ``libSomePluginA.so`` and ``libSomePluginB.so`` in the 
directory ``/home/jan/grunk_plugins/`` *(Note that on Windows the file extension would be .dll)*. 
We can load the plugins using the ``PluginRegistry``. 

.. code-block:: cpp
   
   auto& plugins = grunk::get_plugin_registry();
   plugins.prepend_path("/home/jan/grunk_plugins/");
   plugins.load_all();

   grunk::Feature x("x", "SomePluginA::MyDouble", 4.3);
   grunk::Feature y("y", "SomePluginA::MyDouble", 3.3);
   grunk::Feature z("z", "SomePluginA::MyDouble", 2.0);

   auto a = grunk::eval("a", "SomePluginA::add", x, y)->get();
   auto b = grunk::eval("b", "SomePluginB::multiply", a, z)->get();

When working with plugins, I 
have to use grunk's :ref:`dynamic mode<dynamic-mode>`, while the :ref:`first example<getting-started>` used grunk's 
:ref:`static mode<static-mode>`. In essence, this means that all features of the above feature tree are now instances of ``Feature<Reflect::DynamicObject>``, 
se also :ref:`design principles<design-dynamic-sublanguage>`. Because the plugins are loaded
at runtime, the calling code does not know about the type ``SomePluginA::MyDouble`` and the 
functions ``SomePluginA::add`` and ``SomePluginB::multiply`` directly. Instead, it relies on 
a runtime reflection system used by grunk's plugin system. 

If I evaluate the tree by querying `b.value()`, I will retrieve an instance of ``Reflect::DynamicObject``.
Luckily, the type ``SomePluginA::MyDouble`` has a public data member called `value` which is of type
`double`, see also the section on :ref:`writing plugins<writing-plugins>`. I can use ``Reflect::DynamicObject::get`` to retrieve this data member and then cast it to a 
type that I can deal with:

.. code-block:: cpp
  
   auto b_result = Reflect::cast<double>(b.value().get("value"));
   std::cout << b_result << std::endl;

.. code-block:: console

   15.2
   

.. _reading-and-writing-to-file:

Reading and writing to file
===========================

We can write the feature tree from the :ref:`previous section<using-plugins>` to a file 
with the command

.. code-block:: cpp
   
   grunk::write("/home/jan/my_grunk_files/simple.gk", b);

The file will then have the following contents:

.. code-block:: yaml 
   
   uses:
     grunk: 0.1.0
     SomePluginA: 2.4.19
     SomePluginB: 1.3.0
   parameters:
     x:
       type: "SomePluginA::MyDouble"
       value: 4.3
     y:
       type: "SomePluginA::MyDouble"
       value: 3.3
     z:
       type: "SomePluginA::MyDouble"
       value: 2.0
    steps:
    - function: "SomePluginA::add"
      inputs:
      - x
      - y
      outputs:
      - a
    - function: "SomePluginB::multiply"
      inputs:
      - a
      - z
      outputs:
      - b 

All information needed to reproduce the output of ``b`` gets written into the file in 
yaml format. The steps are sorted in topological order, which means they can be performed
in the same order. The function ``grunk::write`` accepts any number of ``Feature`` 
instances or an iterable collection of ``Feature`` instances. The following commands all 
yield the same file *(except for the order of independent parameters)*:

.. code-block:: cpp 
   
   grunk::write("/home/jan/my_grunk_files/simple.gk", b);
   grunk::write("/home/jan/my_grunk_files/simple.gk", b, a, x, y, z);
   grunk::write("/home/jan/my_grunk_files/simple.gk", z, b);

   std::vector<RuntimeFeature> vec{a,b,x};
   grunk::write("/home/jan/my_grunk_files/simple.gk", vec);

The file can then be read by grunk and the feature tree can be 
reconstructed, as long as the two plugins have been loaded:

.. code-block:: cpp 
   
   auto& plugins = grunk::get_plugin_registry();
   plugins.prepend_path("/home/jan/grun_plugins/");
   plugins.load_all();

   auto features = grunk::read("/home/jan/my_grunk_files/simple.gk")
   auto b = features["b"];
   auto b_result = Reflect::cast<double>(b.value().get("value"));
   std::cout << b_result << std::endl;

.. code-block:: console

   15.2

The function ``grunk::read`` returns a map of ``Feature`` instances, where the 
keys are the the string ids of the features. 

Plugins enable experts to create and use domain specific building blocks to model 
complex systems. grunk files enable experts to share workflows in a collaborative and 
multidisciplinary environment.

.. _writing-plugins:

Writing Plugins
===============

Let us assume we are the authors of the plugin ``SomePluginA`` from the 
:ref:`previous example<using-plugins>`, so our code looks like this:

.. code-block:: cpp
   
   struct MyDouble {
       MyDouble(double v) : value(v) {}
       double value;
   };

   MyDouble add(MyDouble const& l, MyDouble const& r)
   {
       return {l.value + r.value};
   }

We can make the type ``MyDouble`` and the function ``add`` available for 
use in a feature tree by creating a grunk plugin. We do so, by including the 
grunk header ``grunk/grunk.h`` and inheriting from ``grunk::IPlugin``. We 
have to overwrite the virtual methods ``name``, ``version`` and ``init``. 
In the ``init`` function we can register all types and functions we want to 
make available in our grunk interface.

.. code-block:: cpp 

   class SomePluginA: public grunk::IPlugin
   {
   public:
   
       virtual std::string name() const override final
       {
           return "SomePluginA";
       }
   
       virtual std::string version() const override final
       {
           return "2.4.19";
       }
   
       virtual void init() const override final 
       {
           // register types
   
           grunk::register_type<MyDouble>("MyDouble")
           .AddConstructor<double>()
           .AddDataMember(&MyDouble::value, "value")
           .AddMemberFunction(
               [](MyDouble const& d){
                   YAML::Node out(d.value);
                   return out;
               },
               "serialize"
           )
           .AddMemberFunction(
               [](YAML::Node const& y){
                   return MyDouble(y.as<double>());
               },
               "deserialize"
           );
   
           // register functions
   
           grunk::register_function(&add, "add", "adds two MyDouble instances");
       }
   
   };
   GRUNK_REGISTER_PLUGIN(SomePluginA)

After creating the derived class ``SomePluginA``, we need to register the plugin using 
the C macro ``GRUNK_REGISTER_PLUGIN``. If the compilation unit containing this code 
is compiled to a shared library, the plugin can be used in grunk.

Let us take a closer look at the body of the ``init`` function. 

First, the type 
``MyDouble`` is registered with the call to ``grunk::register_type``. It is given a 
name to look up the type in grunk's type registry. 

Though this is not necessary 
for grunk's plugin system, we are letting the type registry know about the 
constructor taking a ``double`` with ``AddConstructor``. This allows users of grunk to 
create instances of ``MyDouble``, even if the plugin is loaded at runtime and the calling 
program does not know about the existence of ``MyDouble`` at compile time. 

Next, the public data member ``value`` is added to the grunk interface. It can be queried
with the string identifier "value", and this was already used in the example 
:ref:`"Using Plugins"<using-plugins>`.

If ``MyDouble`` had any public member functions, we could register them using 
``AddMemberFunction``. But ``AddMemberFunction`` is more powerful: We can use it to 
add free functions as methods, even if they don't exist in the definition of the type. 
If this free function takes a reference to ``MyDouble`` as first argument, it behaves like a normal
member function. If it does not, it behaves like a static member function. 

In the above code block, we are adding the free function ``serialize`` as a method to 
``MyDouble`` using ``AddMemberFunction``. The free function 
creates a ``YAML::Node`` (see `yaml-cpp <https://github.com/jbeder/yaml-cpp>`_) from an 
instance of ``MyDouble``. In this example, the ``YAML::Node`` is very simple: It only holds the 
``MyDouble::value`` as a ``double``. This information is enough to uniquely transform an instance 
of ``MyDouble`` to yaml and back again.

In addition, a "static" member function is added called ``deserialize``. This method takes 
a ``YAML::Node`` and creates an instance of ``MyDouble``. 

Adding the functions

.. code-block:: cpp
   
   YAML::Node serialize(Type const&);
   Type deserialize(YAML::Node const&);

as member functions to a type ``Type`` is mandatory, if 

 * it should be possible to use ``Type`` instances as a root parameter of a grunk feature tree **and**
 * it should be possible to write and read feature trees with ``Type`` instances as root parameters to/from a grunk file.

Finally, in the last line of the ``init`` function, the function ``add`` is registered by a 
call to ``register_function``. It is given a string identifier for lookup in grunk's function
registry and (optionally) a short string that serves as a documentation for that function. 

grunk is designed so that it should be easy to add a grunk interface to an existing C++ 
code base.

.. _sharing-plugins:

Sharing Plugins 
===============

To Do