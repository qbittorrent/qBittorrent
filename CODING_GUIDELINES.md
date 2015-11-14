All new code must follow the following coding guidelines.  
If you make changes in a file that still uses another coding style, make sure that you follow these guidelines for your changes instead.  
**Note:** I will now take your head if you forget and use another style. However, most probably the request will be delayed until you fix your coding style.

### 1. Curly braces ###
#### a. Function blocks, class/struct definitions, namespaces ####
```c++
int myFunction(int a)
{
    //code
}

myClass::myClass(int *parent)
    : m_parent(parent)
{
    //initialize
}

int myClass::myMethod(int a)
{
    //code
}

class myOtherClass
{
public:
    //code
protected:
    //code
private:
    //code
};

namespace id
{
    //code
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
    : priv_a(a)
    , priv_b(b)
    , priv_c(c)
    , priv_d(d)
{
    //code
}
```

### 6. Enums.###
Enums should be vertical. This will allow for more easily readable diffs. The members should be indented.
```c++
enum days
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

### 7. Misc.###

* Line breaks for long lines with operation:

```c++
a += "b"
  + "c"
  + "d";
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

###8. Not covered above###
If something isn't covered above, just follow the same style the file you are editing has. If that particular detail isn't present in the file you are editing, then use whatever the rest of the project uses.