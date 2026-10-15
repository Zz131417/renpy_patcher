/* translate/translate_cli.c —— 翻译流水线 CLI（中间格式为 JSON）
 *
 * 子命令（argv[0] = "translate"）：
 *   translate extract <解包目录> -o dialogs.json
 *       提取目录下所有 .rpy 的可翻译文本（对话/菜单/界面），导出 JSON（file,line,type,speaker,source,target）
 *   translate ai <dialogs.json> -o out.json
 *       [--from 源语言] [--to 目标语言] [--ai-base URL] [--ai-key KEY] [--ai-model M]
 *       用 AI 批量翻译 JSON 中的 source，结果写入 target
 *   translate human <dialogs.json> --lang zh -o mod_dir
 *   translate build <dialogs.json> --lang zh -o mod_dir
 *       把有译文的条目（对话/菜单/界面，统一用 old/new）生成 Ren'Py 翻译 mod（tl/<lang>/）
 *
 * 工作流：
 *   1) extract 解包目录 -> dialogs.json
 *   2) 人工：编辑 dialogs.json 填 target；或 ai：自动填 target
 *   3) build/human -> 翻译 mod 目录（再用 build 注入游戏）
 */
#include "translate/translate_cli.h"
#include "translate/rpy_dialog.h"
#include "translate/json_io.h"
#include "translate/ai_translate.h"
#include "util/log.h"
#include "core/plugin_loader.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *get_opt(int argc, char **argv, int start, const char *key) {
    for (int i = start; i + 1 < argc; i++)
        if (strcmp(argv[i], key) == 0) return argv[i + 1];
    return NULL;
}

int cmd_translate(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "用法:\n");
        fprintf(stderr, "  translate extract <解包目录> -o dialogs.json   （提取对话/菜单/界面文本）\n");
        fprintf(stderr, "  translate ai <dialogs.json> -o out.json [--from 语言] [--to 语言] [--ai-base URL] [--ai-key KEY] [--ai-model M]\n");
        fprintf(stderr, "  translate human <dialogs.json> --lang zh -o mod_dir\n");
        fprintf(stderr, "  translate build <dialogs.json> --lang zh -o mod_dir\n");
        return 1;
    }
    plugin_init();
    const char *sub = argv[1];

    if (!strcmp(sub, "extract")) {
        if (argc < 3) { fprintf(stderr, "用法: translate extract <解包目录> -o dialogs.json\n"); return 1; }
        const char *dir = argv[2];
        const char *out = get_opt(argc, argv, 3, "-o");
        if (!out) out = "dialogs.json";
        dialog_entry_t *arr = NULL; size_t cnt = 0;
        int rc = extract_dialogs(dir, &arr, &cnt);
        if (rc != RP_OK) { log_error("提取对话失败，错误码 %d", rc); return rc; }
        rc = write_dialogs_json(arr, cnt, out);
        printf("已提取 %zu 条对话 -> %s\n", cnt, out);
        free_dialog_entries(arr, cnt);
        return rc;
    }

    if (!strcmp(sub, "ai")) {
        if (argc < 3) { fprintf(stderr, "用法: translate ai <dialogs.json> -o out.json [...]\n"); return 1; }
        const char *json = argv[2];
        const char *out = get_opt(argc, argv, 3, "-o");
        if (!out) out = "dialogs_translated.json";
        const char *from = get_opt(argc, argv, 3, "--from");
        const char *to   = get_opt(argc, argv, 3, "--to"); if (!to) to = "Chinese";
        const char *base = get_opt(argc, argv, 3, "--ai-base");
        const char *key  = get_opt(argc, argv, 3, "--ai-key");
        const char *model= get_opt(argc, argv, 3, "--ai-model");
        if (!key) key = getenv("RP_AI_API_KEY");
        if (!base) base = getenv("RP_AI_BASE_URL");
        if (!model) model = getenv("RP_AI_MODEL");

        dialog_entry_t *arr = NULL; size_t cnt = 0;
        int rc = read_dialogs_json(json, &arr, &cnt);
        if (rc != RP_OK) return rc;
        if (cnt == 0) { log_error("JSON 无条目"); free_dialog_entries(arr, cnt); return 1; }

        const char **srcs = malloc(cnt * sizeof(char*));
        char **tgts = malloc(cnt * sizeof(char*));
        for (size_t i = 0; i < cnt; i++) { srcs[i] = arr[i].source; tgts[i] = NULL; }
        rc = ai_translate_batch(srcs, tgts, cnt, from, to, base, key, model);
        if (rc == RP_OK) {
            for (size_t i = 0; i < cnt; i++) {
                if (tgts[i] && *tgts[i]) { free(arr[i].target); arr[i].target = tgts[i]; }
                else free(tgts[i]);
            }
            write_dialogs_json(arr, cnt, out);
            printf("AI 翻译完成 -> %s\n", out);
        } else {
            for (size_t i = 0; i < cnt; i++) free(tgts[i]);
        }
        free(srcs); free(tgts);
        free_dialog_entries(arr, cnt);
        return rc;
    }

    if (!strcmp(sub, "human") || !strcmp(sub, "build")) {
        if (argc < 3) { fprintf(stderr, "用法: translate %s <dialogs.json> --lang zh -o mod_dir\n", sub); return 1; }
        const char *json = argv[2];
        const char *lang = get_opt(argc, argv, 3, "--lang"); if (!lang) lang = "zh";
        const char *out = get_opt(argc, argv, 3, "-o");
        if (!out) out = "translation_mod";
        dialog_entry_t *arr = NULL; size_t cnt = 0;
        int rc = read_dialogs_json(json, &arr, &cnt);
        if (rc != RP_OK) return rc;
        rc = build_translation_mod(arr, cnt, out, lang);
        if (rc == RP_OK) printf("已生成翻译 mod -> %s/tl/%s/\n", out, lang);
        free_dialog_entries(arr, cnt);
        return rc;
    }

    fprintf(stderr, "未知 translate 子命令: %s\n", sub);
    return 1;
}
