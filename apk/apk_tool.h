/* apk/apk_tool.h —— 基于已有 APK 重打包（注入 mod，生成未签名 APK）
 *
 * 流程：解开已有 APK（一个 ZIP）→ 注入 mod 目录（相对 APK 根路径，如 assets/game/...）
 *       → 4 字节对齐重新打包成未签名 APK。最后请用 Android SDK 的 apksigner 自签：
 *           apksigner sign --ks my.keystore out.apk
 *
 * 不内置签名/密码学（保持零依赖）；默认移除旧 META-INF 签名文件，便于重新签名。
 */
#ifndef RP_APK_TOOL_H
#define RP_APK_TOOL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 把已有 APK 解开并注入 mod_dir，重打包为对齐的未签名 APK。
 * apk_path : 输入 APK 路径
 * mod_dir  : 注入目录（路径相对 APK 根；传 NULL/"" 则只重打包）
 * out_apk  : 输出 APK 路径
 * strip_sig: 非 0 时移除旧 META-INF 下的 .SF/.RSA/.DSA/.EC 签名文件（建议开启）
 * 返回 RP_OK / 错误码。 */
int apk_repack(const char *apk_path, const char *mod_dir, const char *out_apk, int strip_sig);

#ifdef __cplusplus
}
#endif
#endif /* RP_APK_TOOL_H */
