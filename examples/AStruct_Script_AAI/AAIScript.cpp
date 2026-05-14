// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include <AAI/hpp/Center.h>


AAI::Layouts::LineEdit lineedit;

extern "C" __declspec(dllexport) std::string Signature() {
    AAI::AString name = "AAI插件";
    AAI::AString config = "这是一个示例插件";
    std::string signature = AAI::Signature(name.to_String(), "1.0.0", config.to_String());
    return signature;
}

std::vector<std::string> numbervec;
std::vector<AAI::Layouts::PushButton*> buttonlist;
AAI::MESBOX::MessageBoxs Tinput;
AAI::Layouts::TextEdit textedit;
float calculate(const std::vector<std::string>& tokens) {
    if (tokens.empty()) return 0.0f;

    // 把连续的数字 token 拼成一个数，并收集运算符
    std::vector<float> nums;
    std::vector<char> ops;
    std::string cur;
    for (const auto& t : tokens) {
        if (t == "+" || t == "-" || t == "X" || t == "x" || t == "/") {
            if (!cur.empty()) {
                nums.push_back(std::stof(cur));
                cur.clear();
            }
            else if (nums.empty()) {
                // 表达式以运算符开头，视为 0 <op> ...
                nums.push_back(0.0f);
            }
            char opChar = (t == "X" || t == "x") ? '*' : t[0];
            ops.push_back(opChar);
        }
        else {
            // 认为是数字字符（按钮每次压入一个字符串，如 "1"）
            cur += t;
        }
    }
    if (!cur.empty()) nums.push_back(std::stof(cur));

    if (nums.empty()) return 0.0f;
    // 如果最后是运算符，忽略多余运算符
    if (nums.size() < ops.size() + 1) ops.resize((nums.size() > 0) ? nums.size() - 1 : 0);

    // 先计算乘除（使用值栈）
    std::vector<float> vals;
    std::vector<char> addsub; // 保存剩下的 + / -
    vals.push_back(nums[0]);
    for (size_t i = 0; i < ops.size(); ++i) {
        char op = ops[i];
        float rhs = nums[i + 1];
        if (op == '*' || op == '/') {
            float lhs = vals.back(); vals.pop_back();
            float r = 0.0f;
            if (op == '*') r = lhs * rhs;
            else {
                if (rhs == 0.0f) r = std::numeric_limits<float>::infinity();
                else r = lhs / rhs;
            }
            vals.push_back(r);
        }
        else {
            // + 或 -
            vals.push_back(rhs);
            addsub.push_back(op);
        }
    }

    // 再做加减（从左到右）
    float result = vals[0];
    for (size_t i = 0; i < addsub.size(); ++i) {
        if (addsub[i] == '+') result += vals[i + 1];
        else result -= vals[i + 1];
    }

    return result;
}

extern "C" __declspec(dllexport) void Trigger_Event(AStruct* data) {
    using namespace AAI;

	textedit.AddonTextEdit("TextEdit", { 300,10,200,30 });

    for(int i=0;i<buttonlist.size();i++)
    {
		buttonlist[i]->Event.SetEvent(data);
	}    
    
    if (buttonlist[0]->Event.Clicked()) {
        lineedit.method.AppendText("1");
        numbervec.push_back("1");
        return;
    }
    if (buttonlist[1]->Event.Clicked()) {
        lineedit.method.AppendText("2");
        numbervec.push_back("2");
        return;
    }
    if (buttonlist[2]->Event.Clicked()) {
        lineedit.method.AppendText("3");
        numbervec.push_back("3");
        return;
    }
    if (buttonlist[3]->Event.Clicked()) {
        lineedit.method.AppendText("4");
        numbervec.push_back("4");   
        return;
    }
    if (buttonlist[4]->Event.Clicked()) {
        lineedit.method.AppendText("5");
        numbervec.push_back("5");
        return;
    }
    if (buttonlist[5]->Event.Clicked()) {
        lineedit.method.AppendText("6");
        numbervec.push_back("6");
        return;
    }
    if (buttonlist[6]->Event.Clicked()) {
        lineedit.method.AppendText("7");
        numbervec.push_back("7");
        return;
    }
    if (buttonlist[7]->Event.Clicked()) {
        lineedit.method.AppendText("8");
        numbervec.push_back("8");
        return;
    }
    if (buttonlist[8]->Event.Clicked()) {
        lineedit.method.AppendText("9");
        numbervec.push_back("9");
        return;
    }
    if (buttonlist[9]->Event.Clicked()) {
        lineedit.method.AppendText("0");
        numbervec.push_back("0");
        return;
    }
    if (buttonlist[10]->Event.Clicked()) {
        lineedit.method.AppendText("+");
        numbervec.push_back("+");
        return;
    }
    if (buttonlist[11]->Event.Clicked()) {
        lineedit.method.AppendText("-");
        numbervec.push_back("-");
        return;
    }
    if (buttonlist[12]->Event.Clicked()) {
        lineedit.method.AppendText("X");
        numbervec.push_back("X");
        return;
    }
    if (buttonlist[13]->Event.Clicked()) {
        lineedit.method.AppendText("/");
        numbervec.push_back("/");
        return;
    }
    if (buttonlist[14]->Event.Clicked()) {
        float result = calculate(numbervec);
		lineedit.method.SetText(std::to_string(result));
        return;
    }
	if (buttonlist[15]->Event.Clicked())
    {
        
       auto msg = Tinput.GetInput("a", "b");
		lineedit.method.SetText(msg);
        return;
    }
    
}

extern "C" __declspec(dllexport) void ReceiveEvent(AStruct* data) {
    lineedit.SetData(data);
	Tinput.SetData(data);

}

extern "C" __declspec(dllexport) void MAINS() {
    AAI::ConnectToCenter();
    using namespace AAI;

    
    // 创建一个 LineEdit（显示用）
    lineedit.AddonLineEdit("LineEdit2", { 10,10 });

    // 计算器按钮布局参数
    const int startX = 10;
    const int startY = 80; // LineEdit 下方开始
    const int btnW = 50;
    const int btnH = 50;
    const int gap = 10;

    // 数字 1..9 (按行填充)
    int nums[9] = { 1,2,3,4,5,6,7,8,9 };
    for (int i = 0; i < 9; ++i) {
        int n = nums[i];
        int row = i / 3; // 0..2
        int col = i % 3; // 0..2
        int x = startX + col * (btnW + gap);
        int y = startY + row * (btnH + gap);
        buttonlist.push_back(new AAI::Layouts::PushButton());
        buttonlist.back()->AddonButton(std::to_string(n), { x, y, btnW, btnH });
    }

    // 在中间下方添加 0 按钮（居中在第二列）
    int zeroX = startX + 1 * (btnW + gap); // 第二列
    int zeroY = startY + 3 * (btnH + gap);
    buttonlist.push_back(new AAI::Layouts::PushButton());
    buttonlist.back()->AddonButton("0", { zeroX, zeroY, btnW, btnH });

    // 运算符按钮放在数字右侧一列： + - X /
    int opStartX = startX + 3 * (btnW + gap) + 10;
    std::vector<std::string> ops = { "+", "-", "X", "/" };
    for (size_t i = 0; i < ops.size(); ++i) {
        int y = startY + static_cast<int>(i) * (btnH + gap);
        buttonlist.push_back(new AAI::Layouts::PushButton());
        buttonlist.back()->AddonButton(ops[i], { opStartX, y, btnW, btnH });
    }

    int funcW = btnW * 2 + gap; 
    int funcX = opStartX; 
    int calcY = zeroY; 
    buttonlist.push_back(new AAI::Layouts::PushButton());
    buttonlist.back()->AddonButton(AAI::AString("计算").to_String(), { funcX, calcY, funcW, btnH });

    buttonlist.push_back(new AAI::Layouts::PushButton());
    buttonlist.back()->AddonButton(AAI::AString("清空").to_String(), { funcX, calcY + (btnH + gap), funcW, btnH });

	lineedit.metal.OnlyRead(true);
}





BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

