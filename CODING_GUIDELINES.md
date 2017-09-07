All new code must follow the following coding guidelines.  
If you make changes in a file that still uses another coding style, make sure that you follow these guidelines for your changes instead.  
**Note 1:** I will not take your head if you forget and use another style. However, most probably the request will be delayed until you fix your coding style.  
**Note 2:** You can use the `uncrustify` program/tool to clean up any source file. Use it with the `uncrustify.cfg` configuration file found in the root folder.  
**Note 3:** There is also a style for QtCreator but it doesn't cover all cases. In QtCreator `Tools->Options...->C++->Code Style->Import...` and choose the `codingStyleQtCreator.xml` file found in the root folder.  

### 1. Curly braces ###
#### a. Function blocks, class/struct definitions, namespaces ####
```c++
int myFunction(int a)
{
    // code
}

void myFunction() {} // empty body

MyClass::MyClass(int *parent)
    : m_parent(parent)
{
    // initialize
}

int MyClass::myMethod(int a)
{
    // code
}

class MyOtherClass
{
public:
    // code

protected:
    // code

private:
    // code
};

namespace Name
{
    // code
}

// Lambdas
[](int arg1, int arg2) -> bool { return arg1 < arg2; }

[this](int arg)
{
    this->acc += arg;
}
```

#### b. Other code blocks ####
```c++
if (condition) {
    // code
}

for (int a = 0; a < b; ++b) {
    // code
}

switch (a) {
case 1:
    // blah
case 2:
    // blah
default:
    // blah
}
```

#### c. Blocks in switch's case labels ####
```c++
switch (var) {
case 1: {
        // declare local variables
        // code
    }
    break;
case 2: {
        // declare local variables
        // code
    }
    break;
default:
    // code
}
```

#### d. Brace enclosed initializers ####
Unlike single-line functions, you must not insert spaces between the brackets and concluded expressions.<br/>
But you must insert a space between the variable name and initializer.
```c++
Class obj {}; // empty
Class obj {expr};
Class obj {expr1, /*...,*/ exprN};
QVariantMap map {{"key1", 5}, {"key2", 10}};
```

### 2. If blocks ###
#### a. Multiple tests ####
```c++
if (condition) {
    // code
}
else if (condition) {
    // code
}
else {
    // code
}
```
The `else if`/`else` must be on their own lines.

#### b. Single statement if blocks ####
**Most** single statement if blocks should look like this:
```c++
if (condition)
    a = a + b;
```

One acceptable exception to this **can be** `return`, `break` or `continue` statements, provided that the test condition isn't very long. However you can choose to use the first rule instead.
```c++
a = myFunction();
b = a * 1500;

if (b > 0) return;
c = 100 / b;
```

#### c. Using curly braces for single statement if blocks ####

However, there are cases where curly braces for single statement if blocks **should** be used.
* If some branch needs braces then all others should use them. Unless you have multiple `else if` in a row and the one needing the braces is only for a very small sub-block of code.
* Another exception would be when we have nested if blocks or generally multiple levels of code that affect code readability.

Generally it will depend on the particular piece of code and would be determined on how readable that piece of code is. **If in doubt** always use braces if one of the above exceptions applies.

### 3. Indentation ###
4 spaces.

### 4. File encoding and line endings. ###

UTF-8 and Unix-like line ending (LF). Unless some platform specific files need other encodings/line endings.

### 5. Initialization lists. ###
Initialization lists should be vertical. This will allow for more easily readable diffs. The initialization colon should be indented and in its own line along with first argument. The rest of the arguments should be indented too and have the comma prepended.
```c++
myClass::myClass(int a, int b, int c, int d)
    : m_a(a)
    , m_b(b)
    , m_c(c)
    , m_d(d)
{
    // code
}
```

### 6. Enums. ###
Enums should be vertical. This will allow for more easily readable diffs. The members should be indented.
```c++
enum Days
{
    Monday,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
    Sunday
};
```

### 7. Names. ###
All names should be camelCased.

#### a. Type names and namespaces ####
Type names and namespaces start with Upper case letter (except POD types).
```c++
class ClassName {};

struct StructName {};

enum EnumName {};

typedef QList<ClassName> SomeList;

namespace NamespaceName
{
}
```

#### b. Variable names ####
Variable names start with lower case letter.
```c++
int myVar;
```

#### c. Private member variable names ####
Private member variable names start with lower case letter and should have ```m_``` prefix.
```c++
class MyClass
{
    int m_myVar;
}
```

### 8. Header inclusion order. ###
The headers should be placed in the following order:
 1. Module header (in .cpp)
 2. System/Qt/Boost etc. headers (splitted in subcategories if you have many).
 3. Application headers, starting from *Base* headers.

The headers should be ordered alphabetically within each group (subgroup).<br/>
<br/>
Example:
```c++
// examplewidget.cpp

#include "examplewidget.h"

#include <cmath>
#include <cstdio>

#include <QDateTime>
#include <QList>
#include <QString>
#include <QUrl>

#include <libtorrent/version.hpp>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "ui_examplewidget.h"

```

### 9. Misc. ###

* Line breaks for long lines with operation:

```c++
a += "b"
  + "c"
  + "d";
```

* **auto** keyword

We allow the use of the **auto** keyword only where it is strictly necessary 
(for example, to declare a lambda object, etc.), or where it **enhances** the readability of the code.
Declarations for which one can gather enough information about the object interface (type) from its name 
or the usage pattern (an iterator or a loop variable are good examples of clear patterns) 
or the right part of the expression nicely fit here.<br/>
<br/>
When weighing whether to use an auto-typed variable please think about potential reviewers of your code, 
who will read it as a plain diff (on github.com, for instance). Please make sure that such reviewers can 
understand the code completely and without excessive effort.<br/>
<br/>
Some valid use cases:
```c++
template <typename List>
void doSomethingWithList(const List &list)
{
    foreach (const auto &item, list) {
        // we don't know item type here so we use 'auto' keyword
        // do something with item
    }
}

for (auto it = container.begin(), end = container.end(); it != end; ++it) {
    // we don't need to know the exact iterator type,
    // because all iterators have the same interface
}

auto spinBox = static_cast<QSpinBox*>(sender());
// we know the variable type based on the right-hand expression
```

* Notice the spaces in the following specific situations:
```c++
// Before and after the assignment and other binary (and ternary) operators there should be a space
// There should not be a space between increment/decrement and its operand
a += 20;
a = (b <= MAX_B ? b : MAX_B);
++a;
--b;

for (int a = 0; a < b; ++b) {
}

// Range-based for loop, spaces before and after the colon
for (auto i : container) {
}

// Derived class, spaces before and after the colon
class Derived : public Base
{
};
```

* Prefer pre-increment, pre-decrement operators
```c++
++i, --j;  // Yes
i++, j--;  // No
```

* private/public/protected must not be indented

* Preprocessor commands must go at line start

* Method definitions aren't allowed in header files

### 10. Not covered above ###
If something isn't covered above, just follow the same style the file you are editing has. If that particular detail isn't present in the file you are editing, then use whatever the rest of the project uses.
