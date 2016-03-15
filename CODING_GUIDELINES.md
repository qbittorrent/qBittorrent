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
    //code
}

void myFunction() {} // empty body

MyClass::MyClass(int *parent)
    : m_parent(parent)
{
    //initialize
}

int MyClass::myMethod(int a)
{
    //code
}

class MyOtherClass
{
public:
    //code
protected:
    //code
private:
    //code
};

namespace Name
{
    //code
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
    //code
}

for (int a = 0; a < b; ++b) {
    //code
}

switch (a) {
case 1:
    //blah
case 2:
    //blah
default:
    //blah
}
```

#### c. Blocks in switch's case labels ####
```c++
switch (var) {
case 1: {
        //declare local variables
        //code
    }
    break;
case 2: {
        //declare local variables
        //code
    }
    break;
default:
    //code
}
```

#### d. single-line blocks (lambdas, initializer lists etc.) ####
```c++
 {} // empty - space before {
 { body } // spaces around { and before }
```

### 2. If blocks ###
#### a. Multiple tests ####
```c++
if (condition) {
    //code
}
else if (condition) {
    //code
}
else {
    //code
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

### 3. Indentation###
4 spaces.

### 4. File encoding and line endings.###

UTF-8 and Unix-like line ending (LF). Unless some platform specific files need other encodings/line endings.

### 5. Initialization lists.###
Initialization lists should be vertical. This will allow for more easily readable diffs. The initialization colon should be indented and in its own line along with first argument. The rest of the arguments should be indented too and have the comma prepended.
```c++
myClass::myClass(int a, int b, int c, int d)
    : m_a(a)
    , m_b(b)
    , m_c(c)
    , m_d(d)
{
    //code
}
```

### 6. Enums.###
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

### 7. Names.###
All names should be camelCased.

#### a. Type names and namespaces ####
Type names and namespaces start with Upper case letter (except POD types).
```c++
class ClassName {}

struct StructName {}

enum EnumName {}

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

### 8. Misc.###

* Line breaks for long lines with operation:

```c++
a += "b"
  + "c"
  + "d";
```

* Initializers

We allow brace enclosed initializers only for aggregates and arrays/containers.<br />
Brace enclosed initializer MUST be used with equality sign if it follows the variable declaration.<br />
Brace enclosed initializer MUST be additionally enclosed in parentheses if it is used in constructor initialization list.<br />
Some valid use cases:
```c++
// aggregate
Person john = { "John", "Smith", 21 };
Person *john = new Person { "John", "Smith", 21 };

// array
int array[] = { 1, 2, 3, 4 };

// container
QHash<QString, QString> map = {
    { "key1", "value1" },
    { "key2", "value2" }
);

// member array
SomeClass::SomeClass(BaseClass *parent)
    : BaseClass(parent)
    , m_someArrayMember({ 1, 2, 3, 4 })
{
}

// return from function
Person getPersonByName(const QString &name)
{
    // do something
    return { name, surname, age };
}

// function argument
doSomething({ name, surname, age }, someOtherArg);
```

* **auto** keyword

We allow the use of the **auto** keyword only where it doesn't break the readability of the code (i.e. either we can gather enough information about the type from the right part of the expression, or we do not need to know the exact type), or where it is strictly necessary (for example, to compute the type of a lambda, etc.).<br />
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

* Space around operations eg `a = b + c` or `a=b+c`:

Before and after the assignment there should be a space. One exception could be: for loops.
```c++
for (int a=0; a<b; ++b) {
}
```

* private/public/protected must not be indented

* Preprocessor commands must go at line start

* Method definitions aren't allowed in header files

### 9. Not covered above###
If something isn't covered above, just follow the same style the file you are editing has. If that particular detail isn't present in the file you are editing, then use whatever the rest of the project uses.