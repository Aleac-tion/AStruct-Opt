import ctypes
import os
from flask import Flask, jsonify, render_template, request
from flask_cors import CORS

app = Flask(__name__)
CORS(app)

# ================== 硬编码配置 ==================
DLL_PATH = r"K:/CPP_VS2026/lib/AStructLan/x64/Release/AStructLan.dll"
BASE_DIR = r"F:/NeaseServer/root"  # 服务器根目录
# ===============================================

# 加载 DLL
dll = ctypes.CDLL(DLL_PATH)

# ----- 定义函数原型 -----
# AStruct 相关
CreateAS = dll.CreateAS
CreateAS.argtypes = [ctypes.c_char_p]
CreateAS.restype = ctypes.c_int

loaddata = dll.loaddata
loaddata.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
loaddata.restype = None

getvalue = dll.getvalue
getvalue.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
getvalue.restype = ctypes.c_char_p

# AList 相关
ALists = dll.ALists
ALists.argtypes = [ctypes.c_char_p]
ALists.restype = ctypes.c_int

AList_fetch = dll.AList_fetch
AList_fetch.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
AList_fetch.restype = ctypes.c_char_p

AList_Size = dll.AList_Size
AList_Size.argtypes = [ctypes.c_char_p]
AList_Size.restype = ctypes.c_int

AList_Go = dll.AList_Go
AList_Go.argtypes = [ctypes.c_char_p, ctypes.c_int]
AList_Go.restype = ctypes.c_char_p

AList_Get = dll.AList_Get
AList_Get.argtypes = [ctypes.c_char_p, ctypes.c_int]
AList_Get.restype = ctypes.c_char_p


def parse_astruct_to_medicines(username):
    """根据用户名解析对应的 config.astruct，返回药物列表"""
    # 1. 拼接文件路径
    if not username:
        return []
    
    # 路径格式: F:/NeaseServer/root/{username}/files/Aledicater/config.astruct
    astruct_path = os.path.join(BASE_DIR, username, "files", "Aledicater", "config.astruct")
    
    # 检查文件是否存在
    if not os.path.exists(astruct_path):
        print(f"文件不存在: {astruct_path}")
        return []
    
    # 2. 创建 AStruct 实例并加载文件
    as_id = b"medicine_as"
    CreateAS(as_id)
    loaddata(as_id, astruct_path.encode("utf-8"))

    # 3. 通过 getvalue 获取超级数组字符串
    title = "药物列表".encode("utf-8")
    header = "药物".encode("utf-8")
    key = "name".encode("utf-8")
    array_ptr = getvalue(as_id, title, header, key)
    if not array_ptr:
        print("getvalue 返回空指针")
        return []

    array_str = array_ptr.decode("utf-8")
    print(f"超级数组: {array_str[:200]}...")

    # 4. 解析 AList
    outer_size = AList_Size(array_str.encode("utf-8"))
    print("outer_size =", outer_size)

    medicines = []

    for i in range(outer_size):
        # C++: const char* innerArrayStr = AList_Go(arrayStr, i);
        inner_ptr = AList_Go(array_str.encode("utf-8"), i)
        if not inner_ptr:
            continue

        inner_array_str = inner_ptr.decode("utf-8")

        # 内层：为每个内层数组分配一个临时 ID，并用 AList_fetch + AList_Get
        inner_id = f"inner_{i}".encode("utf-8")
        ALists(inner_id)  # 确保存在
        AList_fetch(inner_id, inner_array_str.encode("utf-8"))

        inner_size = AList_Size(inner_array_str.encode("utf-8"))
        print(f"inner[{i}] size =", inner_size)

        if inner_size < 6:
            continue

        # 像 C++ 那样逐个字段取出
        name_ptr  = AList_Get(inner_id, 0)
        total_ptr = AList_Get(inner_id, 1)
        price_ptr = AList_Get(inner_id, 2)
        per_box_ptr = AList_Get(inner_id, 3)
        day_ptr   = AList_Get(inner_id, 4)
        times_ptr = AList_Get(inner_id, 5)

        # 统一做一次解码与转换
        name      = name_ptr.decode("utf-8") if name_ptr else ""
        total     = float(total_ptr.decode("utf-8")) if total_ptr else 0.0
        price     = float(price_ptr.decode("utf-8")) if price_ptr else 0.0
        per_box   = float(per_box_ptr.decode("utf-8")) if per_box_ptr else 0.0
        day       = float(day_ptr.decode("utf-8")) if day_ptr else 0.0
        times     = float(times_ptr.decode("utf-8")) if times_ptr else 0.0

        medicines.append({
            "name": name,
            "total": total,
            "price": price,
            "perBox": per_box,
            "day": day,
            "times": times
        })

        print(f"解析到第 {i} 个药物: {name}")

    return medicines


@app.route('/')
def index():
    return render_template('index.html')


@app.route('/api/medicines', methods=['GET'])
def get_medicines():
    # 从请求参数中获取用户名，例如 /api/medicines?username=123
    username = request.args.get('username', '')
    if not username:
        return jsonify({"success": False, "error": "缺少用户名参数"}), 400
    
    try:
        meds = parse_astruct_to_medicines(username)
        print(f"用户 {username} 解析到 {len(meds)} 条药物数据")
        return jsonify({"success": True, "medicines": meds})
    except Exception as e:
        print(f"错误: {e}")
        return jsonify({"success": False, "error": str(e)}), 500


if __name__ == '__main__':
    app.run(host='::', port=5456)