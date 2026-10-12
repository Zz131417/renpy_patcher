# 直接注入示例 mod —— Ren'Py 启动时会自动加载本文件
# 作用：演示「新增脚本进入资源包」后，游戏运行时被激活。
#
# 真实汉化里，这里会 monkeypatch renpy 文本渲染或注册翻译表。
# 本示例仅做可见标记，证明注入链路打通。
#
# 命名 zz_ 前缀：Ren'Py 按字母序后加载，覆盖 script.rpy 中的同名 label start。

init python:
    import store
    store.MOD_HANZI_LOADED = True
    print("[mod_hanzi] 直接注入 mod 已加载 ✅")

label start:
    scene black
    show logo

    "欢迎来到 Ren'Py Patcher 演示游戏！"
    "这段文本已被【直接注入 mod】替换为中文。"

    "本 mod 还替换了 images/logo.png，并新增了此脚本文件。"
    "按任意键继续。"
    return
