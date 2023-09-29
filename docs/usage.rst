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

grunk lets you delay the evaluation of the function until the result is queried.

.. code-block:: cpp

   #include <grunk/grunk.h>

   auto o = grunk::action("o", &add, 1.2, 40.8).output();
   
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
a new ``Action`` instance ``o`` is created using ``grunk::action``.
The arguments are the label ``"o"``, a function pointer ``&add`` and two arguments
that shall be passed into the function. With ``.output()`` we retrieve a handle
to the first (and in this case only) output of the action, which is of
type ``Feature<double>``. 

Internally, ``o`` depends on the action created
by ``grunk::action``, which in turn depends on two ``Feature<double>`` instances, 
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

   auto a = grunk::action("a", &add, x, y).output();
   auto b = grunk::action("b", &add, a, z).output();

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
store dependency information, but it pays off
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
Internally, grunk checks if a function passed to ``grunk::action`` can be invoked given only
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

   auto a = grunk::action("a", "SomePluginA::add", x, y).output();
   auto b = grunk::action("b", "SomePluginB::multiply", a, z).output();

When working with plugins, I 
have to use grunk's :ref:`dynamic mode<dynamic-mode>`, while the :ref:`first example<getting-started>` used grunk's 
:ref:`static mode<static-mode>`. In essence, this means that all features of the above feature tree are now instances of ``Feature<reflect::DynamicObject>``, 
see also :ref:`design principles<design-dynamic-sublanguage>`. Because the plugins are loaded
at runtime, the calling code does not know about the type ``SomePluginA::MyDouble`` and the 
functions ``SomePluginA::add`` and ``SomePluginB::multiply`` directly. Instead, it relies on 
a runtime reflection system used by grunk's plugin system. 

If I evaluate the tree by querying `b.value()`, I will retrieve an instance of ``reflect::DynamicObject``.
Luckily, the type ``SomePluginA::MyDouble`` has a public data member called `value` which is of type
`double`, see also the section on :ref:`writing plugins<writing-plugins>`. I can use ``reflect::DynamicObject::output`` to retrieve this data member and then cast it to a 
type that I can deal with:

.. code-block:: cpp
  
   auto b_result = reflect::cast<double>(b.value().output("value"));
   std::cout << b_result << std::endl;

.. code-block:: console

   15.2
   

.. _reading-and-writing-to-file:

Reading and writing to file
===========================

We can write the feature tree from the :ref:`previous section<using-plugins>` to a file, 
the *grunk recipe*, with the command

.. code-block:: cpp
   
   grunk::write("/home/jan/my_grunk_files/simple.grr", b);

The grunk recipe will have the following contents:

.. code-block:: yaml 
   
   uses:
     grunk: 0.1.0
     SomePluginA: 2.4.19
     SomePluginB: 1.3.0
   parameters:
     x: !<SomePluginA::MyDouble> 4.3
     y: !<SomePluginA::MyDouble> 3.3
     z: !<SomePluginA::MyDouble> 2.0
    steps:
    - !<SomePluginA::add> [ [a], [x, y] ]
    - !<SomePluginB::multiply> [ [b], [a, z] ]

All information needed to reproduce the output of ``b`` gets written into the file in 
yaml format. The steps are sorted in topological order, that is in the order in which an 
evaluation is possible. The function ``grunk::write`` accepts any number of ``Feature`` 
instances or an iterable collection of ``Feature`` instances. The following commands all 
yield the same file *(except for the order of independent parameters)*:

.. code-block:: cpp 
   
   grunk::write("/home/jan/my_grunk_files/simple.grr", b);
   grunk::write("/home/jan/my_grunk_files/simple.grr", b, a, x, y, z);
   grunk::write("/home/jan/my_grunk_files/simple.grr", z, b);

The file can then be read by grunk and the feature tree can be 
reconstructed, as long as the two plugins have been loaded:

.. code-block:: cpp 
   
   auto& plugins = grunk::get_plugin_registry();
   plugins.prepend_path("/home/jan/grun_plugins/");
   plugins.load_all();

   auto features = grunk::read("/home/jan/my_grunk_files/simple.grr")
   auto b = features.at("b");
   auto b_result = reflect::cast<double>(b.value().output("value"));
   std::cout << b_result << std::endl;

.. code-block:: console

   15.2

The function ``grunk::read`` returns a ``Recipe`` instances, which stores the re-constructed
features, see :ref:`the next section<grunk-recipes>`. Retrieval is based on the string ids of the features. 

Plugins enable experts to create and use domain specific building blocks to model 
complex systems. grunk files enable experts to share workflows in a collaborative and 
multidisciplinary environment.

.. _grunk-recipes:

Grunk Recipes 
=============

You have seen in :ref:`the previous section<reading-and-writing-to-file>` how individual features or 
a set of features can be written to a *grunk recipe*. In grunk, there exists a class to model 
such a recipe in :ref:`dynamic mode<dynamic-mode>`, namely ``::grunk::Recipe``. 

In a certain sense, a ``::grunk::Recipe`` is just a container of features. You can create a recipe and 
add features to it, or you can create the features directly within the recipe:

.. code-block:: cpp 

   auto a = grunk::Feature("a", "PluginA::Scalar", 17.);
   auto b = grunk::Feature("b", "PluginA::Scalar", 15.);
   auto c = grunk::action("c", "PluginA::add", a, b).output();
   Recipe recipe1(a, b, c);

   Recipe recipe2;
   recipe2.feature("x", "PluginA::Scalar", 2.);
   recipe2.feature("y", "PluginA::Scalar", 5.);
   recipe2.insert_feature(
      grunk::action("z", "PluginB::multiply", recipe2["x"], recipe2["y"]).output()
   );

In addition to storing features, recipes can store recipes. Think of them as building-blocks for your model. 
For instance, a recipe for an aircraft may have recipes for modeling wings, fuselages or a landing gear.
Continuing our above example, we can insert ``recipe2`` as a subrecipe of ``recipe1`` and assign a label to it:

.. code-block:: cpp

   recipe1.insert_recipe("multiplication", std::move(recipe2));

``::grunk::Recipe``\s can be used like functions in the sense, that we can interpret independent 
features as arguments and any other feature as an output. These functions can in turn be treated like a new 
compute node in a feature tree. This helps us with encapsulation: We can build complex recipes using a set of 
smaller recipes.

Let us invoke the new subrecipe ``multiplication`` of ``recipe1`` on ``a`` and ``b``. 

.. code-block:: cpp 

   recipe.recipe("multiplication", {{"d", "z"}}, {{"x", a}, {"y", b}});

The arguments are not intuitive to parse. It is best to read them from right to left:

 * Take feature ``b`` as input for the inner independent feature with id ``y``. 
 * Take feature ``a`` as input for the inner independent feature with id ``x``.
 * Map the inner output feature ``z`` to a newly created feature in the outer recipe and assign 
   the new feature with the id ``d``.

The inner recipe will be uneffected by this action. Since all inner features have a default value, 
it is not necessary to assign all input features with a feature from the outer recipe. As an example, 
it would also have been okay, to just replace ``x`` with say ``c`` and keep ``y`` as is.

Exporting the recipe will result in the following yaml-representation:

.. code-block:: yaml 
   
   uses:
     grunk: 0.2.1
     PluginA: 0.1.0
   parameters:
     a: !<PluginA::Scalar> 17
     b: !<PluginA::Scalar> 15
   steps:
     - !<PluginA::add> [[c], [a, b]]
     - !<recipes::multiplication> [{d: z}, {x: a, y: b}]
   recipes:
     multiplication:
       uses:
         PluginB: 0.1.0
         PluginA: 0.1.0
       parameters:
         x: !<PluginA::Scalar> 2
         y: !<PluginA::Scalar> 5
       steps:
         - !<PluginB::multiply> [[z], [x, y]]

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
   
           register_type<MyDouble>("MyDouble")
           .add_constructor<double>()
           .add_data_member(&MyDouble::value, "value")
           .add_member_function(
               [](MyDouble const& d){
                   YAML::Node out(d.value);
                   return out;
               },
               "serialize"
           )
           .add_member_function(
               [](YAML::Node const& y){
                   return MyDouble(y.as<double>());
               },
               "deserialize"
           );
   
           // register functions
   
           register_function(&add, "add", "adds two MyDouble instances");
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
constructor taking a ``double`` with ``add_constructor``. This allows users of grunk to 
create instances of ``MyDouble``, even if the plugin is loaded at runtime and the calling 
program does not know about the existence of ``MyDouble`` at compile time. 

Next, the public data member ``value`` is added to the grunk interface. It can be queried
with the string identifier "value", and this was already used in the example 
:ref:`"Using Plugins"<using-plugins>`.

If ``MyDouble`` had any public member functions, we could register them using 
``add_member_function``. But ``add_member_function`` is more powerful: We can use it to 
add free functions as methods, even if they don't exist in the definition of the type. 
If this free function takes a reference to ``MyDouble`` as first argument, it behaves like a normal
member function. If it does not, it behaves like a static member function. 

In the above code block, we are adding the free function ``serialize`` as a method to 
``MyDouble`` using ``add_member_function``. The free function 
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
