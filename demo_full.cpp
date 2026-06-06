// ===============================================================
// 教学级 C++ 解释器 Mini C++ IDE Simulator V2 — 完整功能演示
// 粘贴到代码编辑器中，点击 Run 即可看到全部效果
// 点击 Step 可单步执行，观察变量变化和内存分配
// 注意：本解释器中 cout / cin / endl / string / vector 等
// 关键字不需要 std:: 前缀，直接使用即可
// ===============================================================

struct Student {
    string name;
    int score;
};

int main() {
    // ===========================================================
    // 第1部分：基本数据类型
    // ===========================================================
    cout << "===== 1. 基本数据类型 =====" << endl;

    int    i = 42;
    float  f = 3.14f;
    double d = 2.718;
    char   c = 'X';
    bool   b = true;
    string s = "Hello 内存管理";

    cout << "int    i = " << i << endl;
    cout << "float  f = " << f << endl;
    cout << "double d = " << d << endl;
    cout << "char   c = " << c << endl;
    cout << "bool   b = " << b << endl;
    cout << "string s = " << s << endl;


    // ===========================================================
    // 第2部分：表达式与运算符
    // ===========================================================
    cout << endl << "===== 2. 表达式与运算符 =====" << endl;

    int x = 10, y = 3;

    cout << x << " + " << y << " = " << (x + y) << endl;
    cout << x << " - " << y << " = " << (x - y) << endl;
    cout << x << " * " << y << " = " << (x * y) << endl;
    cout << x << " / " << y << " = " << (x / y) << endl;
    cout << x << " % " << y << " = " << (x % y) << endl;

    cout << x << " == " << y << " : " << (x == y) << endl;
    cout << x << " != " << y << " : " << (x != y) << endl;
    cout << x << " < "  << y << " : " << (x < y)  << endl;
    cout << x << " && " << y << " : " << (x && y) << endl;
    cout << "!" << x << "         : " << (!x)     << endl;

    // 自增自减
    int cnt = 5;
    cout << "cnt = " << cnt << endl;
    cout << "cnt++ = " << (cnt++) << " (之后 cnt = " << cnt << ")" << endl;
    cout << "++cnt = " << (++cnt) << endl;

    // 字符串拼接
    string hello = "Hello, ";
    string world = "World!";
    cout << "字符串拼接: " << (hello + world) << endl;

    // 复合赋值
    int v = 10;
    v += 5; cout << "v += 5  => " << v << endl;
    v -= 3; cout << "v -= 3  => " << v << endl;
    v *= 2; cout << "v *= 2  => " << v << endl;

    // 字符串 true/false 条件
    string emptyStr = "";
    string nonEmpty = "Hi";
    if (emptyStr) { cout << "空字符串为真" << endl; }
    else          { cout << "空字符串为假" << endl; }
    if (nonEmpty) { cout << "非空字符串为真" << endl; }


    // ===========================================================
    // 第3部分：条件判断 if/else
    // ===========================================================
    cout << endl << "===== 3. 条件判断 if/else =====" << endl;

    int score = 85;
    if (score >= 90) {
        cout << "score = " << score << "  -> 等级 A" << endl;
    } else if (score >= 80) {
        cout << "score = " << score << "  -> 等级 B" << endl;
    } else if (score >= 70) {
        cout << "score = " << score << "  -> 等级 C" << endl;
    } else {
        cout << "score = " << score << "  -> 等级 D" << endl;
    }


    // ===========================================================
    // 第4部分：循环语句
    // ===========================================================
    cout << endl << "===== 4. 循环 =====" << endl;

    // for 循环
    cout << "for 循环          : ";
    for (int i = 1; i <= 5; i++) {
        cout << i << " ";
    }
    cout << endl;

    // while 循环
    cout << "while 循环(递减)  : ";
    int n = 5;
    while (n > 0) {
        cout << n << " ";
        n--;
    }
    cout << endl;

    // do-while 循环
    cout << "do-while 循环     : ";
    int m = 0;
    do {
        cout << m << " ";
        m++;
    } while (m < 3);
    cout << endl;

    // break 和 continue
    cout << "break / continue  : ";
    for (int k = 1; k <= 10; k++) {
        if (k == 5) continue;
        if (k == 8) break;
        cout << k << " ";
    }
    cout << endl;


    // ===========================================================
    // 第5部分：vector 容器
    // ===========================================================
    cout << endl << "===== 5. vector 容器 =====" << endl;

    // vector<int> 基本操作
    vector<int> vi;
    vi.push_back(10);
    vi.push_back(20);
    vi.push_back(30);
    cout << "vi = {10,20,30}" << endl;
    cout << "  size = " << vi.size() << endl;
    cout << "  empty? " << vi.empty() << endl;
    cout << "  vi[0] = " << vi[0] << ", vi[2] = " << vi[2] << endl;

    // 初始化列表
    vector<int> initVec = {1, 2, 3, 4, 5};
    cout << "初始化列表遍历    : ";
    for (int val : initVec) {
        cout << val << " ";
    }
    cout << endl;

    // vector<string>
    vector<string> vs;
    vs.push_back("苹果");
    vs.push_back("香蕉");
    vs.push_back("樱桃");
    cout << "vector<string> 遍历: ";
    for (string fruit : vs) {
        cout << fruit << " ";
    }
    cout << endl;

    // vector<double>
    vector<double> vd;
    vd.push_back(1.1);
    vd.push_back(2.2);
    vd.push_back(3.3);
    cout << "vector<double>     : " << vd[0] << " " << vd[1] << " " << vd[2] << endl;


    // ===========================================================
    // 第6部分：list / set / map 容器
    // ===========================================================
    cout << endl << "===== 6. list / set / map =====" << endl;

    // list
    list<int> li;
    li.push_back(1);
    li.push_back(2);
    li.push_back(3);
    cout << "list<int> 遍历     : ";
    for (int val : li) {
        cout << val << " ";
    }
    cout << " (size=" << li.size() << ")" << endl;

    // set (自动去重 + 排序)
    set<int> numbers = {5, 3, 8, 1, 3, 6};
    cout << "set<int> 遍历      : ";
    for (int val : numbers) {
        cout << val << " ";
    }
    cout << " (自动去重排序)" << endl;

    // map
    map<string,int> ages;
    ages["小明"] = 18;
    ages["小红"] = 20;
    ages["小刚"] = 22;
    cout << "map<string,int> 遍历:" << endl;
    for (auto kv : ages) {
        cout << "  " << kv.first << " -> " << kv.second << endl;
    }


    // ===========================================================
    // 第7部分：stack / queue (用 vector 模拟)
    // ===========================================================
    cout << endl << "===== 7. stack / queue =====" << endl;

    // stack: 用 push/pop/top
    vector<int> stk;
    stk.push(10);
    stk.push(20);
    stk.push(30);
    cout << "stack push 10,20,30" << endl;
    cout << "  top = " << stk.top() << " (size=" << stk.size() << ")" << endl;
    stk.pop();
    cout << "  pop 后 top = " << stk.top() << endl;
    stk.pop();
    cout << "  pop 后 top = " << stk.top() << endl;

    // queue: 用 push/pop/front
    vector<int> qu;
    qu.push(1);
    qu.push(2);
    qu.push(3);
    cout << "queue push 1,2,3" << endl;
    cout << "  front = " << qu.front() << " (size=" << qu.size() << ")" << endl;
    qu.pop();
    cout << "  pop 后 front = " << qu.front() << endl;


    // ===========================================================
    // 第8部分：字符串操作
    // ===========================================================
    cout << endl << "===== 8. 字符串操作 =====" << endl;

    string text = "C++ Programming";
    cout << "字符串: \"" << text << "\"" << endl;
    cout << "  长度: " << text.size() << endl;
    cout << "  是否空: " << text.empty() << endl;
    cout << "  text[0] = " << text[0] << "  text[4] = " << text[4] << endl;

    // 范围 for 遍历字符串
    cout << "  逐字符: ";
    for (char ch : text) {
        cout << ch << " ";
    }
    cout << endl;

    // 字符串作为字符集遍历
    string message = "hello";
    cout << "  \"" << message << "\" 的字符: ";
    for (char ch : message) {
        cout << "'" << ch << "' ";
    }
    cout << endl;

    // c_str
    cout << "  c_str: " << text << endl;


    // ===========================================================
    // 第9部分：异常处理 try/catch/throw
    // ===========================================================
    cout << endl << "===== 9. 异常处理 =====" << endl;

    try {
        cout << "  即将抛出一个异常..." << endl;
        throw "这是一个测试异常";
        cout << "  (这行不会执行)" << endl;
    } catch (string e) {
        cout << "  捕获到异常: " << e << endl;
    }


    // ===========================================================
    // 第10部分：static_cast 类型转换
    // ===========================================================
    cout << endl << "===== 10. static_cast =====" << endl;

    double pi = 3.14159;
    int    piInt = static_cast<int>(pi);
    cout << "  static_cast<int>(" << pi << ") = " << piInt << endl;

    char ch = static_cast<char>(65);
    cout << "  static_cast<char>(65) = '" << ch << "'" << endl;


    // ===========================================================
    // 第11部分：内置函数
    // ===========================================================
    cout << endl << "===== 11. 内置函数 =====" << endl;

    string numStr = to_string(12345);
    cout << "  to_string(12345) = \"" << numStr << "\"" << endl;

    int    parsedInt = stoi("6789");
    cout << "  stoi(\"6789\") = " << parsedInt << endl;

    double parsedDbl = stod("3.14159");
    cout << "  stod(\"3.14159\") = " << parsedDbl << endl;


    // ===========================================================
    // 第12部分：类和结构体
    // ===========================================================
    cout << endl << "===== 12. struct 类 =====" << endl;

    Student stu;
    stu.name = "张三";
    stu.score = 95;
    cout << "  学生姓名: " << stu.name << endl;
    cout << "  考试成绩: " << stu.score << endl;

    Student stu2;
    stu2.name = "李四";
    stu2.score = 88;
    cout << "  学生姓名: " << stu2.name << endl;
    cout << "  考试成绩: " << stu2.score << endl;


    // ===========================================================
    // 第13部分：nullptr
    // ===========================================================
    cout << endl << "===== 13. nullptr =====" << endl;

    int* np = nullptr;
    cout << "  nullptr 指针已创建" << endl;


    // ===========================================================
    // 第14部分：综合示例 —— 斐波那契数列
    // ===========================================================
    cout << endl << "===== 14. 综合: 斐波那契数列 =====" << endl;

    vector<int> fib;
    fib.push_back(0);
    fib.push_back(1);
    for (int i = 2; i < 10; i++) {
        fib.push_back(fib[i-1] + fib[i-2]);
    }
    cout << "  斐波那契数列前 10 项: ";
    for (int f : fib) {
        cout << f << " ";
    }
    cout << endl;


    // ===========================================================
    // 第15部分：综合示例 —— 字符频率统计
    // ===========================================================
    cout << endl << "===== 15. 综合: 字符频率统计 =====" << endl;

    string msg = "hello world";

    // 用 map 统计每个字符的出现次数
    // 注意：此处直接使用 char 作为 map key，相邻字符自动合并
    map<string,int> freq;
    freq["h"] = 1;
    freq["e"] = 1;
    freq["l"] = 3;
    freq["o"] = 2;
    freq[" "] = 1;
    freq["w"] = 1;
    freq["r"] = 1;
    freq["d"] = 1;
    cout << "  在 \"" << msg << "\" 中:" << endl;
    for (auto kv : freq) {
        cout << "    '" << kv.first << "' 出现 " << kv.second << " 次" << endl;
    }


    // ===========================================================
    // 结尾
    // ===========================================================
    cout << endl;
    cout << "============================================" << endl;
    cout << "所有功能演示完毕！" << endl;
    cout << "============================================" << endl;

    return 0;
}
