// tests/test_cpp_api.cpp
// 验证核心 C 接口可被 C++ 调用（模拟 GUI 会走的路径）。
// 仅用于开发期接口校验；不属于最终产物。
#include <cstdio>
#include "rpa/rpa_v3.h"
#include "core/game_mgr.h"
#include "util/credit.h"
#include "util/log.h"
#include "inject/direct/direct_inject.h"
#include "inject/export/export_module.h"

int main() {
    log_init("test_api.log");
    log_set_tag("test_cpp_api");

    game_mgr_init(nullptr);
    int rc = game_add("demo", "tests/sample_game.rpa", "test");
    if (rc != RP_OK) { printf("game_add 失败 %d\n", rc); return 1; }
    const game_entry_t *g = game_get("demo");
    printf("[game] %s -> %s\n", g ? g->name : "(null)", g ? g->path : "(null)");

    rc = rpa_v3_unpack("tests/sample_game.rpa", "api_out");
    printf("[unpack] rc=%d\n", rc);
    if (rc != RP_OK) return 2;

    rc = direct_inject_build("tests/sample_game.rpa", "tests/mod_hanzi", "api_patched.rpa");
    printf("[build]  rc=%d\n", rc);
    if (rc != RP_OK) return 3;

    rc = credit_add_to_rpa("tests/sample_game.rpa", "api_credited.rpa", "demo");
    printf("[credit] rc=%d\n", rc);
    if (rc != RP_OK) return 4;

    rc = export_dir("api_out", "api_export.zip", 0, 1);
    printf("[export] rc=%d\n", rc);
    if (rc != RP_OK) return 5;

    game_remove("demo");
    printf("OK: C++ 接口验证通过（GUI 调用路径可用）\n");
    return 0;
}
