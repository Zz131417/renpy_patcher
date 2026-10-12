/* rpa/rpa_format.h —— RPA 文件头结构定义 */
#ifndef RENPY_PATCHER_RPA_FORMAT_H
#define RENPY_PATCHER_RPA_FORMAT_H

#define RPA3_MAGIC          "RPA-3.0"
#define RPA3_MAGIC_LEN      7
#define RPA3_PREFIX_DEFAULT 0x12345678u
/* 头部固定长度： "RPA-3.0"(7) + prefix(8hex) + index_len(8hex) + "\n"(1) = 24 */
#define RPA3_HEADER_LEN     24

#endif
