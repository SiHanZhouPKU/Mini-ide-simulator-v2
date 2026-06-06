# Mini C++ IDE Simulator V2 — 支持的 C++ 操作完整清单

> 本文档基于源码审计（lexer / parser / AST / executor / variant_value），逐一列出该解释器能够识别和执行的**所有 C++ 语法与运行时操作**。

---

## 目录

1. [数据类型](#1-数据类型)
2. [字面量](#2-字面量)
3. [变量声明](#3-变量声明)
4. [运算符](#4-运算符)
5. [控制流](#5-控制流)
6. [输入 / 输出](#6-输入--输出)
7. [STL 容器](#7-stl-容器)
8. [容器成员方法](#8-容器成员方法)
9. [字符串操作](#9-字符串操作)
10. [内置函数](#10-内置函数)
11. [异常处理](#11-异常处理)
12. [类型转换](#12-类型转换)
13. [类与结构体](#13-类与结构体)
14. [模板](#14-模板)
15. [指针与动态内存](#15-指针与动态内存)
16. [Lambda 表达式](#16-lambda-表达式)
17. [注释](#17-注释)
18. [预处理器指令](#18-预处理器指令)
19. [附录：已解析但未完全执行的关键字](#19-附录已解析但未完全执行的关键字)

---

## 1. 数据类型

### 1.1 基本类型（声明 + 运算 + 显示 全支持）

| 类型 | Token | 内部存储 | 模拟字节数 | 示例 |
|------|-------|----------|-----------|------|
| `int` | `INT` | `std::variant<int>` | 4 | `int x = 42;` |
| `float` | `FLOAT` | `std::variant<float>` | 4 | `float f = 3.14f;` |
| `double` | `DOUBLE` | `std::variant<double>` | 8 | `double d = 2.718;` |
| `char` | `CHAR` | `std::variant<char>` | 1 | `char c = 'X';` |
| `bool` | `BOOL` | `std::variant<bool>` | 1 | `bool b = true;` |
| `string` | `STRING` | `std::variant<std::string>` | 字符串长度+1 | `string s = "hello";` |
| `void` | `VOID` | — | 0 | 函数返回类型 |

### 1.2 类型推导

| 特性 | 支持情况 |
|------|---------|
| `auto` 自动类型推导 | ✅ `auto x = 42;` 根据初始化表达式推导 |
| `auto` 无初始化 | ❌ 报错：`auto 变量必须有初始化表达式` |

### 1.3 组合声明

| 语法 | 示例 |
|------|------|
| 同类型多变量 | `int x = 10, y = 3;` |

### 1.4 默认值（无初始化时）

| 类型 | 默认值 |
|------|--------|
| `int` | `0` |
| `float` | `0.0f` |
| `double` | `0.0` |
| `char` | `'\0'` |
| `bool` | `false` |
| `string` | `""` |
| 容器类型 | 空容器 |

---

## 2. 字面量

### 2.1 整数字面量

```
42, 0, -1, 12345
```

- Token 类型：`INT_LITERAL`
- 内部存储：`int`
- Lexer 方法：`readNumber()`

### 2.2 浮点数字面量

```
3.14, 3.14f, 2.718, 0.5
```

- Token 类型：`FLOAT_LITERAL` / `DOUBLE_LITERAL`
- 带 `f` 后缀识别为 `float`，否则为 `double`

### 2.3 字符字面量

```cpp
'X', '\0', 'A'
```

- Token 类型：`CHAR_LITERAL`
- Lexer 方法：`readChar()`
- 支持转义序列识别（`\n`, `\t`, `\\`, `\'`, `\0` 等）

### 2.4 字符串字面量

```cpp
"Hello World", "C++", ""
```

- Token 类型：`STRING_LITERAL`
- Lexer 方法：`readString()`

### 2.5 布尔字面量

```cpp
true, false
```

- Token 类型：`BOOL_LITERAL`

---

## 3. 变量声明

### 3.1 基本声明语法

```cpp
int x;                  // 默认初始化
int x = 42;             // 带初始化表达式
int a = 1, b = 2, c = 3; // 多变量声明
auto x = 3.14;          // 自动推导为 double
```

### 3.2 容器声明

```cpp
vector<int> vi;                        // 空 vector
vector<int> vi = {1, 2, 3};           // 初始化列表
vector<string> vs;                     // 字符串 vector
vector<double> vd;                     // 浮点 vector
list<int> li;                          // int 链表
list<string> ls;                       // 字符串链表
set<int> numbers = {5, 3, 8};          // 初始化列表 → set（自动去重排序）
map<string, int> ages;                 // 字符串到 int 的映射
```

### 3.3 对象声明

```cpp
Student stu;             // 默认构造
Student stu2;            // 第二个独立实例
```

### 3.4 指针声明

```cpp
int* p = nullptr;
```

---

## 4. 运算符

### 4.1 算术运算符

| 运算符 | 语法 | 支持类型 | 示例 |
|--------|------|---------|------|
| `+` | `a + b` | int, float, double | `x + y` |
| `-` | `a - b` | int, float, double | `x - y` |
| `*` | `a * b` | int, float, double | `x * y` |
| `/` | `a / b` | int, float, double | `x / y` |
| `%` | `a % b` | int | `x % y` |
| `-` (一元取负) | `-a` | int, float, double | `-x` |

- **隐式类型提升**：混合运算时 int→float→double 自动提升
- **除零检查**：`%` 运算检测除零

### 4.2 比较运算符

| 运算符 | 语法 | 返回值 |
|--------|------|--------|
| `==` | `a == b` | bool |
| `!=` | `a != b` | bool |
| `<` | `a < b` | bool |
| `>` | `a > b` | bool |
| `<=` | `a <= b` | bool |
| `>=` | `a >= b` | bool |

### 4.3 逻辑运算符

| 运算符 | 语法 | 特性 |
|--------|------|------|
| `&&` | `a && b` | **短路求值**（左为假不计算右） |
| `\|\|` | `a \|\| b` | **短路求值**（左为真不计算右） |
| `!` | `!a` | 逻辑非 |

- 条件判断支持 int（非0为真）、bool、string（非空为真）、char（非0为真）

### 4.4 自增自减运算符

| 运算符 | 语法 | 支持类型 | 语义 |
|--------|------|---------|------|
| `++x` | 前置自增 | int, float, double | 先加后用 |
| `x++` | 后置自增 | int, float, double | 先用后加 |
| `--x` | 前置自减 | int, float, double | 先减后用 |
| `x--` | 后置自减 | int, float, double | 先用后减 |

### 4.5 复合赋值运算符

| 运算符 | 语法 |
|--------|------|
| `=` | `v = expr` |
| `+=` | `v += 5` |
| `-=` | `v -= 3` |
| `*=` | `v *= 2` |
| `/=` | 已解析，支持 |
| `%=` | 已解析，支持 |

- `+=` 支持数值类型和字符串拼接

### 4.6 位运算符

| 运算符 | 语法 | 支持类型 |
|--------|------|---------|
| `<<` | `a << b` | int 左移 |
| `>>` | `a >> b` | int 右移 |
| `~` | `~a` | 已解析 (Token: TILDE) |

### 4.7 指针 / 成员运算符

| 运算符 | 语法 | 用途 |
|--------|------|------|
| `.` | `obj.member` | 成员访问 |
| `->` | `ptr->member` | 指针成员访问 |
| `&` | `&var` | 取地址 |
| `*` | `*ptr` | 解引用 |

### 4.8 运算符优先级（完整 Precedence Climbing）

```
表达式入口
  └── 赋值表达式 (=, +=, -=, *=, /=, %=)
        └── 逻辑或 (||)
              └── 逻辑与 (&&)
                    └── 相等性 (==, !=)
                          └── 关系 (<, >, <=, >=)
                                └── 加减 (+, -)
                                      └── 乘除 (*, /, %)
                                            └── 一元 (-, !, *, &, ++, --)
                                                  └── 后缀 ([]、.、->、()、++、--)
                                                        └── 基本表达式
```

### 4.9 字符串拼接（`+` 运算符重载）

```cpp
string hello = "Hello, ";
string world = "World!";
cout << (hello + world) << endl;  // 输出: Hello, World!
```

---

## 5. 控制流

### 5.1 条件语句

| 语句 | 语法 | 支持 |
|------|------|------|
| `if` | `if (cond) { ... }` | ✅ |
| `else if` | `else if (cond) { ... }` | ✅ 可级联 |
| `else` | `else { ... }` | ✅ |

**条件表达式**支持的 truthy 判断：

| 类型 | true 条件 |
|------|-----------|
| `bool` | `== true` |
| `int` | `!= 0` |
| `string` | 非空（`!value.empty()`） |

### 5.2 循环语句

| 语句 | 语法 | 支持 |
|------|------|------|
| `for` | `for (init; cond; update) { ... }` | ✅ |
| `while` | `while (cond) { ... }` | ✅ |
| `do-while` | `do { ... } while (cond);` | ✅ |
| range-based for | `for (type var : container) { ... }` | ✅ |

**range-based for 支持的容器：**

| 容器类型 | 迭代元素类型 |
|---------|------------|
| `vector<int>` | int |
| `vector<float>` | float |
| `vector<double>` | double |
| `vector<string>` | string |
| `vector<char>` | char |
| `vector<object>` | pointer 语义 |
| `list<int>` | int |
| `list<string>` | string |
| `set<int>` | int |
| `map<string,int>` | pair 对象（`.first` / `.second`） |
| `string` | char（逐字符遍历） |

### 5.3 跳转语句

| 语句 | 用途 | 支持 |
|------|------|------|
| `break` | 跳出当前循环 | ✅ |
| `continue` | 跳过本次迭代 | ✅ |
| `return` | 返回（结束 main 函数） | ✅ |
| `return expr` | 带返回值 | ✅ |

### 5.4 switch 语句

| 特性 | 状态 |
|------|------|
| `switch` / `case` / `default` | ⚠️ 已完整解析（Parser: `parseSwitch()`），Lexer 支持 `SWITCH` `CASE` `DEFAULT` Token |

---

## 6. 输入 / 输出

### 6.1 cout 输出

```cpp
cout << expr;
cout << expr1 << expr2 << expr3;          // 链式输出
cout << "x = " << x << endl;              // 混合类型
cout << endl;                              // 换行
```

- **链式输出**：`<<` 被解析为 `SHL` 运算符，在 cout 上下文中作为流输出
- **支持的类型**：所有 VariantValue 类型（通过 `toString()` 转换）
- **endl**：对应 `CoutItem::ENDL`，输出 `\n`

### 6.2 cin 输入

```cpp
cin >> x;
cin >> name >> age;    // 多变量输入
```

- 支持自动类型转换（根据目标变量类型自动调用 `stoi`/`stof`/`stod` 等）：

| 变量类型 | 转换方式 |
|---------|---------|
| `int` | `std::stoi(input)` |
| `float` | `std::stof(input)` |
| `double` | `std::stod(input)` |
| `string` | 原样保存 |
| `char` | 取首字符 |
| `bool` | `"1"` 或 `"true"` → true |

---

## 7. STL 容器

### 7.1 vector\<T\>

| 元素类型 | Token 关键字 | 内部存储 | 支持操作 |
|---------|------------|---------|---------|
| `vector<int>` | VECTOR | `std::vector<int>` | 声明、push_back、size、empty、下标读写、初始化列表、范围 for |
| `vector<float>` | VECTOR | `std::vector<float>` | 同上 |
| `vector<double>` | VECTOR | `std::vector<double>` | 同上 |
| `vector<char>` | VECTOR | `std::vector<char>` | 同上 |
| `vector<bool>` | VECTOR | `std::vector<bool>` | 同上 |
| `vector<string>` | VECTOR | `std::vector<std::string>` | 同上 |
| `vector<自定义类>` | VECTOR | `std::vector<shared_ptr<RuntimeObject>>` | 同上 + 对象存储 |

### 7.2 list\<T\>

| 元素类型 | 内部存储 | 支持操作 |
|---------|---------|---------|
| `list<int>` | `std::list<int>` | push_back、size、范围 for |
| `list<float>` | `std::list<float>` | 同上 |
| `list<double>` | `std::list<double>` | 同上 |
| `list<char>` | `std::list<char>` | 同上 |
| `list<bool>` | `std::list<bool>` | 同上 |
| `list<string>` | `std::list<std::string>` | 同上 |

### 7.3 set\<int\>

```cpp
set<int> numbers = {5, 3, 8, 1, 3, 6};
```

- 内部存储：`std::set<int>`
- **自动去重 + 排序**
- 支持范围 for 遍历

### 7.4 map\<string, int\>

```cpp
map<string, int> ages;
ages["小明"] = 18;
ages["小红"] = 20;
```

- 内部存储：`std::map<std::string, int>`
- 支持 `[]` 下标写入（key 为 string，value 为 int）
- 支持 `[]` 下标读取（key 不存在时报错）
- 支持范围 for 遍历（每次迭代返回 pair 对象，有 `.first` 和 `.second` 成员）

### 7.5 stack 模拟（基于 vector）

```cpp
vector<int> stk;
stk.push(10);   // 入栈
stk.top();      // 查看栈顶
stk.pop();      // 出栈
stk.size();     // 栈大小
```

### 7.6 queue 模拟（基于 vector）

```cpp
vector<int> qu;
qu.push(1);     // 入队
qu.front();     // 查看队首
qu.pop();       // 出队
qu.size();      // 队列大小
```

---

## 8. 容器成员方法

### 8.1 push_back（追加元素）

| 容器 | 参数类型 |
|------|---------|
| `vector<int>` | int（支持 float/double 隐式转换） |
| `vector<float>` | float（支持 int/double 隐式） |
| `vector<double>` | double（支持 int/float 隐式） |
| `vector<string>` | string（自动 toString） |
| `vector<对象>` | 对象指针 |
| `list<int>` | int（支持 float/double 隐式） |
| `list<string>` | string（自动 toString） |

### 8.2 size（获取大小）

| 容器 | 返回值 |
|------|--------|
| `vector<T>`（全部子类型） | int（元素个数） |
| `list<int>`, `list<string>` | int |
| `string` | int（字符数） |

### 8.3 empty（判空）

| 容器 | 返回值 |
|------|--------|
| `vector<T>`（全部子类型） | bool |
| `list<int>`, `list<string>` | bool |
| `string` | bool |

### 8.4 下标访问 `[]`

| 类型 | 下标类型 | 支持操作 |
|------|---------|---------|
| `vector<int>` | int | 读 / 写 |
| `vector<float>` | int | 读 / 写 |
| `vector<double>` | int | 读 / 写 |
| `vector<string>` | int | 读 / 写 |
| `string` | int | 读 / 写（返回单个 char） |
| `map<string,int>` | string | 读 / 写 |

- **下标越界检查**：vector 和 string 有越界检测

### 8.5 push / pop / top / front（栈和队列模拟）

| 方法 | 适用类型 | 行为 |
|------|---------|------|
| `push(val)` | `vector<int>`, `vector<string>` | 同 push_back |
| `pop()` | `vector<int>`, `vector<string>` | 同 pop_back |
| `top()` | `vector<int>`, `vector<string>` | 返回最后一个元素 |
| `front()` | `vector<int>`, `vector<string>` | 返回第一个元素 |

### 8.6 其他方法

| 方法 | 适用类型 | 行为 |
|------|---------|------|
| `c_str()` | `string` | 返回自身（string 类型本身就是字符串） |
| `begin()` | 容器 | 返回占位值 0（用于算法兼容） |
| `end()` | 容器 | 返回占位值 0 |
| `is_open()` | fstream 对象 | 返回 false |
| `close()` | fstream 对象 | 返回 0 |

---

## 9. 字符串操作

### 9.1 字符串声明

```cpp
string s = "Hello";
string empty = "";
```

### 9.2 成员操作

```cpp
s.size()        // → int: 字符串长度
s.empty()       // → bool: 是否为空
s[i]            // → char: 第 i 个字符（支持读写）
s.c_str()       // → string: 返回自身
```

### 9.3 字符串条件

```cpp
if (s)     { ... }   // 非空为 true
if (!s)    { ... }   // 空字符串为 false
```

### 9.4 字符串拼接

```cpp
string result = hello + world;   // "+" 运算符
s += "追加";                      // "+=" 复合赋值
```

### 9.5 字符串遍历

```cpp
for (char ch : message) { ... }   // range-based for 逐字符
```

### 9.6 字符串拼接（cout 链式）

```cpp
cout << "字符串: " << s << endl;
```

---

## 10. 内置函数

### 10.1 类型转换函数

| 函数 | 签名 | 示例 | 行为 |
|------|------|------|------|
| `to_string` | `to_string(expr) → string` | `to_string(12345)` → `"12345"` | 任意类型 → 字符串（调用 toString()） |
| `stoi` | `stoi(str) → int` | `stoi("6789")` → `6789` | 字符串 → int（调用 `std::stoi`） |
| `stod` | `stod(str) → double` | `stod("3.14")` → `3.14` | 字符串 → double（调用 `std::stod`） |

### 10.2 算法占位函数

以下函数名被识别但仅在模板函数调用时被跳过（返回 0，不报错）：

| 函数名 | 状态 |
|--------|------|
| `sort` | 占位（识别但不执行） |
| `find` | 占位 |
| `find_if` | 占位 |
| `count` | 占位 |
| `copy` | 占位 |
| `transform` | 占位 |
| `getline` | 占位 |

### 10.3 文件流占位

| 类名 | 状态 |
|------|------|
| `ofstream` | 创建空对象，is_open 返回 false |
| `ifstream` | 同上 |
| `fstream` | 同上 |

---

## 11. 异常处理

### 11.1 try / catch / throw

```cpp
try {
    throw "异常消息";
} catch (string e) {
    cout << "捕获: " << e << endl;
}
```

| 特性 | 支持情况 |
|------|---------|
| `throw` 表达式 | ✅ 抛出异常值 |
| `try { ... } catch (type name) { ... }` | ✅ 支持嵌套 try/catch |
| 异常类型匹配 | ✅ 自动推断异常类型（string / int / 自定义类） |
| `catch` 参数名 | ⚠️ 固定使用变量名 `e` |
| 异常展开（unwinding） | ✅ 支持 |
| `catch(...)` | ❌ 暂不支持省略号捕获 |

### 11.2 throw 支持的类型

```cpp
throw "string 异常";        // → 异常类型: std::string
throw 404;                  // → 异常类型: int
throw MyException();        // → 异常类型: 自定义类名
```

---

## 12. 类型转换

### 12.1 static_cast

```cpp
static_cast<int>(3.14159)     // double → int
static_cast<char>(65)          // int → char
static_cast<double>(42)        // int → double
static_cast<float>(3)          // int → float
```

- **cast 类型**：`static` `dynamic` `const` `reinterpret`（均解析，实际运行时仅 static 有实现）

### 12.2 隐式类型转换规则

| 源类型 | 目标类型 | 转换方式 |
|--------|---------|---------|
| 任意数值 | `int` | `static_cast<int>` |
| 任意数值 | `float` | `static_cast<float>` |
| 任意数值 | `double` | `static_cast<double>` |
| 任意类型 | `string` | `.toString()` |
| `int` | `char` | `static_cast<char>` |
| 任意类型 | `bool` | int: `!= 0`; bool: 原值 |

### 12.3 算术混合类型提升

```
int + float   →  float
int + double  →  double
float + double → double
```

---

## 13. 类与结构体

### 13.1 定义语法

```cpp
// struct 定义（默认 public）
struct Student {
    string name;
    int score;
};

// class 定义
class Counter {
    int value;
public:
    Counter() { value = 0; }
    void increment() { value++; }
};
```

### 13.2 成员访问

```cpp
Student stu;
stu.name = "张三";        // . 成员访问
stu.score = 95;
cout << stu.name << endl; // 成员读取
```

### 13.3 多实例

```cpp
Student stu1, stu2;     // 两个独立的对象
stu1.name = "张三";
stu2.name = "李四";
```

### 13.4 OOP 关键字（完整解析）

| 关键字 | Token | 解析状态 |
|--------|-------|---------|
| `class` | CLASS | ✅ |
| `struct` | STRUCT | ✅ |
| `public` | PUBLIC | ✅ |
| `private` | PRIVATE | ✅ |
| `protected` | PROTECTED | ✅ |
| `virtual` | VIRTUAL | ✅ |
| `override` | OVERRIDE | ✅ |
| `final` | FINAL | ✅ |
| `const` | CONST | ✅ |
| `static` | STATIC | ✅ 静态成员变量支持 |
| `friend` | FRIEND | ✅ |
| `this` | THIS | ✅ `ThisExpr` AST 节点 |
| `explicit` | EXPLICIT | ✅ |
| `operator` | OPERATOR | ✅ 运算符重载声明 |

### 13.5 类构造 / 析构 / 方法

```cpp
struct Foo {
    Foo() { ... }              // 构造函数
    ~Foo() { ... }             // 析构函数
    void method() { ... }      // 普通方法
    virtual void vfunc() { }   // 虚函数
    virtual void pure() = 0;   // 纯虚函数
    void constMethod() const;  // const 成员函数
};
```

### 13.6 继承

```cpp
class Derived : public Base { ... };
class Multi : public A, protected B { ... };
```

- 支持多继承解析
- 支持 public / protected / private 继承访问修饰符

### 13.7 类方法调用（运行时）

```cpp
obj.method()         // 普通方法调用
obj->method()        // 指针方法调用
```

- 参数传递到方法作用域
- 方法执行结束后恢复变量作用域
- this 指针支持
- 虚函数表（vtable）支持

---

## 14. 模板

### 14.1 模板类

```cpp
template <typename T>
class MyVector {
    T data;
public:
    void set(T val) { data = val; }
};
```

- 完整解析 `TemplateClassDef` AST 节点
- 运行时模板实例化：首次使用时根据模板参数生成 `RuntimeClass`
- 支持多个模板参数

### 14.2 模板函数

```cpp
template <typename T>
T max(T a, T b) {
    if (a > b) return a;
    else return b;
}
```

- 完整解析 `TemplateFuncDef` AST 节点
- 运行时类型推导（根据调用参数自动推断 T）

### 14.3 模板参数类型

```
typename T       // 类型参数
class T          // 等效于 typename
```

---

## 15. 指针与动态内存

### 15.1 nullptr

```cpp
int* p = nullptr;
```

- Token：`NULLPTR`
- AST：`NullptrExpr`
- 运行时类型：`VariantValue::NULL_PTR`，模拟大小 8 字节

### 15.2 new（动态分配）

```cpp
int* p = new int;            // 标量 new
new Student();               // 对象 new（可带构造函数参数）
new int[10];                 // 数组 new
new int[5]{1, 2, 3, 4, 5};  // 数组 new 带初始化列表
```

- Token：`NEW`
- AST：`NewExpr`（支持 isArray / arraySize / constructorArgs / braceInit）
- 返回类型：`POINTER`

### 15.3 delete（释放内存）

```cpp
delete p;       // 标量 delete
delete[] arr;   // 数组 delete
```

- Token：`DELETE`
- AST：`DeleteExpr`（支持 isArray）
- 运行时无实际操作（返回 0）

### 15.4 指针解引用

```cpp
*ptr    // 解引用 → 返回 OBJECT 类型
```

### 15.5 取地址

```cpp
&var    // 取地址 → 返回 POINTER 类型
```

---

## 16. Lambda 表达式

```cpp
[](int x) { return x * 2; }
```

- Token：`LBRACKET`, `RBRACKET`
- AST：`LambdaExpr`（参数列表 + 函数体）
- **状态**：完整解析，运行时支持待完善

---

## 17. 注释

### 17.1 单行注释

```cpp
// 这是单行注释
```

### 17.2 多行注释

```cpp
/* 这是
   多行注释 */
```

- 在 Lexer 的 `skipWhitespaceAndComments()` 中处理
- 注释内的内容完全跳过，不生成任何 Token

---

## 18. 预处理器指令

| 指令 | 处理方式 |
|------|---------|
| `#include <...>` / `#include "..."` | 在 Parser 的 `skipPreprocessing()` 中跳过整行 |
| `#define` 等 | 跳过 |

- 注意：本解释器中无需 `std::` 前缀，`cout` / `cin` / `string` / `vector` / `endl` 等直接使用

---

## 19. 附录：已解析但未完全执行的关键字

以下 Token 已被 Lexer 识别且 Parser 有相应解析代码，但 Executor 尚未完全实现（运行时可能返回默认值或不执行操作）：

| 关键字 / Token | 解析状态 | 运行时状态 |
|---------------|---------|-----------|
| `switch` / `case` / `default` | ✅ `parseSwitch()` | ⚠️ 待实现 |
| `long` / `short` / `unsigned` | ✅ Token 已定义 | ⚠️ 未完全支持 |
| `deque` | ✅ Token 已定义 | ⚠️ 未完全支持 |
| `namespace` / `using` / `std` | ✅ 已解析（跳过） | — |
| `const` 成员变量 | ✅ 解析 | ⚠️ 运行时未强制不可变 |
| `explicit` 构造函数 | ✅ 解析 | ⚠️ 运行时未强制 |
| `friend` | ✅ 解析 | ⚠️ 运行时未强制访问权限 |
| 纯虚函数 (`= 0`) | ✅ 解析 + 注册 | ✅ 实例化抽象类时阻止 |
| 运算符重载 `operator+` 等 | ✅ 解析 | ✅ 运行时通过 `callOperator` 支持 |
| `virtual` / `override` / `final` | ✅ 解析 | ⚠️ 虚函数表已支持，最终重写检测未完全 |
| 文件流 `ofstream` / `ifstream` | ✅ 解析 + 创建空对象 | ⚠️ 未实际读写文件 |

---

## 统计总览

| 类别 | 数量 |
|------|------|
| Token 类型 | **~60** 种 |
| AST 节点类型 | **~30** 种 |
| VariantValue 运行时类型 | **25+** 种 |
| 支持的运算符 | **30+** 种 |
| 支持的语句类型 | **20+** 种 |
| 容器类型 | **6** 大类（vector / list / set / map / stack / queue） |
| 容器方法 | **10+** 种（push_back / size / empty / push / pop / top / front / [] / c_str / begin / end） |
| 内置函数 | **3** 个（to_string / stoi / stod） |
| 支持的控制流结构 | **7** 种（if / else if / else / for / while / do-while / range-based for / try-catch） |

---

> 📅 最后更新：2026-06-06
>
> 🔗 主项目：[Mini-ide-simulator-v2](https://github.com/SiHanZhouPKU/Mini-ide-simulator-v2)
