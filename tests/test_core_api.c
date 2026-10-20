/* tests/test_core_api.c
 * 端到端测试：只调用 core_api.h 导出的高层 API（即 GUI 通过 P/Invoke 实际调用的函数）。
 * 自包含：自己造一个临时游戏目录 → build 成 rpa → unpack / mod / credit / 烘焙DLL / export，
 * 断言每步返回 RP_OK 且产物存在。
 * Windows 上链接 patcher_core.dll（GUI 真正用的内核），Linux 上链接 renpy_core 静态库（同一份源码）。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#else
#include <sys/stat.h>
#define MKDIR(d) mkdir(d, 0755)
#endif

#include "core_api/core_api.h"

/* core_api.h 未暴露状态码宏，本地定义测试所需（与 core/plugin.h 保持一致：RP_OK=0） */
#ifndef RP_OK
#define RP_OK 0
#endif

static int g_fail = 0;

static void check(int cond, const char *step) {
    if (cond) {
        printf("[OK]   %s\n", step);
    } else {
        printf("[FAIL] %s\n", step);
        g_fail++;
    }
}

static int file_exists(const char *p) {
    FILE *f = fopen(p, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

static void write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(content, 1, strlen(content), f); fclose(f); }
}

static void mkdirp(const char *d) { MKDIR(d); }

int main(void) {
    rp_set_log_level(0);
    rp_game_init(NULL);

    const char *root = "rp_core_test";
    mkdirp(root);
    char src[512], mod[512], game_rpa[512], unpacked[512], credited[512],
         baked[512], outzip[512], testdll[512];
    snprintf(src, sizeof(src), "%s/src_game", root);
    snprintf(mod, sizeof(mod), "%s/mod", root);
    snprintf(game_rpa, sizeof(game_rpa), "%s/game.rpa", root);
    snprintf(unpacked, sizeof(unpacked), "%s/unpacked", root);
    snprintf(credited, sizeof(credited), "%s/credited.rpa", root);
    snprintf(baked, sizeof(baked), "%s/baked.rpa", root);
    snprintf(outzip, sizeof(outzip), "%s/out.zip", root);
    snprintf(testdll, sizeof(testdll), "%s/test_payload.dll", root);

    mkdirp(src);
    mkdirp(src);
    mkdirp(mod);
    /* 造源游戏目录 */
    write_file("rp_core_test/src_game/script.rpy",
        "label start:\n    \"Hello original\"\n");
    write_file("rp_core_test/src_game/options.rpy", "# options\n");
    mkdirp("rp_core_test/src_game/images");
    write_file("rp_core_test/src_game/images/logo.png", "\x89PNG\r\n\x1a\n(fake-png-bytes)");
    /* 造 mod 目录（含中文脚本，模拟汉化） */
    write_file("rp_core_test/mod/zz_hanzi.rpy",
        "# -*- coding: utf-8 -*-\nlabel start:\n    \"你好，世界\"\n");

    /* 1) build：目录 → rpa */
    int rc = rp_build(src, game_rpa);
    check(rc == RP_OK, "rp_build (dir -> rpa)");
    check(file_exists(game_rpa), "game.rpa 产物存在");

    /* 2) unpack：rpa → 目录 */
    rc = rp_unpack(game_rpa, unpacked);
    check(rc == RP_OK, "rp_unpack (rpa -> dir)");
    check(file_exists("rp_core_test/unpacked/script.rpy"), "解包出 script.rpy");

    /* 3) mod：把 mod 合并进已解包目录 */
    rc = rp_mod(unpacked, mod);
    check(rc == RP_OK, "rp_mod (合并 mod 目录)");
    check(file_exists("rp_core_test/unpacked/zz_hanzi.rpy"), "合并出 zz_hanzi.rpy");

    /* 4) credit：为 rpa 追加来源声明 */
    rc = rp_credit_add(game_rpa, credited, "core_api_test");
    check(rc == RP_OK, "rp_credit_add (追加来源声明)");
    check(file_exists(credited), "credited.rpa 产物存在");

    /* 5) 烘焙 DLL 进包（跨平台）：放一个占位 .dll → 解包→放入→声明→重打包 */
    write_file(testdll, "MZ\x90\x00(fake-dll-for-bake-test)");
    rc = rp_inject_dll_bake(game_rpa, testdll, baked);
    if (rc != RP_OK) printf("    (bake 失败原因: %s)\n", rp_error_message());
    check(rc == RP_OK, "rp_inject_dll_bake (烘焙 DLL 进包)");
    check(file_exists(baked), "baked.rpa 产物存在");
    if (rc == RP_OK) {
        char tmp2[512];
        snprintf(tmp2, sizeof(tmp2), "%s/baked_out", root);
        rp_unpack(baked, tmp2);
        char dll_in[640];
        snprintf(dll_in, sizeof(dll_in), "%s/baked_out/test_payload.dll", root);
        check(file_exists(dll_in), "烘焙后包内含 test_payload.dll");
    }

    /* 6) export：目录 → zip */
    rc = rp_export(unpacked, outzip, 0);
    check(rc == RP_OK, "rp_export (dir -> zip)");
    check(file_exists(outzip), "out.zip 产物存在");

    /* 7) 进程注入：无效/不存在的目标应返回非 0（说明调用可达，不会误报成功） */
    rc = rp_inject_dll(999999, "nonexistent.dll");
    check(rc != RP_OK, "rp_inject_dll (无效目标返回非0，接口可达)");

    /* 8) drop-in 模式：把 mod/ 与 dll/ 装入已解包游戏目录，并写 bootstrap */
    char drop_mod[512], drop_dll[512];
    snprintf(drop_mod, sizeof(drop_mod), "%s/dropin_mod", root);
    snprintf(drop_dll, sizeof(drop_dll), "%s/dropin_dll", root);
    mkdirp(drop_mod); mkdirp(drop_dll);
    write_file("rp_core_test/dropin_mod/mod_main.py",
        "print('renpy_patcher drop-in mod loaded')\n");
    write_file("rp_core_test/dropin_mod/zz_patch.rpy",
        "# -*- coding: utf-8 -*-\n# drop-in patch\n");
    write_file("rp_core_test/dropin_dll/test_payload.dll",
        "MZ\\x90\\x00(fake-dll-for-dropin-test)");
    rc = rp_mod_dropin(unpacked, drop_mod, drop_dll);
    check(rc == RP_OK, "rp_mod_dropin (安装 drop-in mod+dll)");
    check(file_exists("rp_core_test/unpacked/mod/mod_main.py"),
          "drop-in: game/mod/mod_main.py 已生成");
    check(file_exists("rp_core_test/unpacked/dll/test_payload.dll"),
          "drop-in: game/dll/test_payload.dll 已生成");
    check(file_exists("rp_core_test/unpacked/zzz_renpy_patcher.rpy"),
          "drop-in: bootstrap 已生成");

    if (g_fail == 0) {
        printf("\nALL CORE API TESTS PASSED ✅\n");
        return 0;
    }
    printf("\n%d CORE API TEST(S) FAILED ❌\n", g_fail);
    return 1;
}
