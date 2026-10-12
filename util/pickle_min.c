/* util/pickle_min.c —— 见 pickle_min.h
 *
 * 栈机解析。支持 CPython 协议 2/3/4 常见操作码，足以解析 Ren'Py 真实 RPA 索引
 * （含 memo 化 BINPUT/PUT/MEMOIZE）。
 *
 * 关键正确性约定（MARK 哨兵 PV_MARK 表示栈上的分组边界）：
 *   - 容器构造 LIST/TUPLE、APPENDS、SETITEMS 都以 MARK 为界，
 *     目标容器（dict/list）位于 MARK **之下** 一格，即 st[mark_idx-1]。
 *   - pop_mark_region() 弹出 MARK 及其上方所有元素，*n 回退到 mark_idx，
 *     于是目标容器落在 st[*n-1]，便于直接取用。
 */
#include "pickle_min.h"
#include "core/plugin.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define PV_MARK ((pv_value*)-1)

static pv_value *new_val(pv_type t) {
    pv_value *v = (pv_value*)calloc(1, sizeof(pv_value));
    if (v) v->type = t;
    return v;
}
static pv_value *new_int(uint64_t x) { pv_value *v = new_val(PV_INT); if (v) v->u = x; return v; }

void pv_free(pv_value *v) {
    if (!v || v == PV_MARK) return;
    free(v->s);
    if (v->items) { for (size_t i = 0; i < v->n; i++) pv_free(v->items[i]); free(v->items); }
    if (v->keys)  { for (size_t i = 0; i < v->nk; i++) { pv_free(v->keys[i]); pv_free(v->vals[i]); } free(v->keys); free(v->vals); }
    free(v);
}

static void push(pv_value ***st, size_t *n, size_t *cap, pv_value *v) {
    if (*n == *cap) { size_t nc = *cap ? *cap * 2 : 16; *st = realloc(*st, nc * sizeof(pv_value*)); *cap = nc; }
    (*st)[(*n)++] = v;
}

/* 弹出 MARK 及其上方所有元素；*n 回退到 MARK 的索引位置（MARK 被移除）。
 * 目标容器因此位于 st[*n-1]。找不到 MARK 返回 NULL。 */
static pv_value **pop_mark_region(pv_value **st, size_t *n, size_t *cnt) {
    size_t k = *n;
    while (k > 0 && st[k-1] != PV_MARK) k--;
    if (k == 0) { *cnt = 0; return NULL; }
    size_t mark_idx = k - 1;                       /* 0-based */
    size_t m = *n - (mark_idx + 1);                /* MARK 之上的元素个数 */
    pv_value **arr = (pv_value**)malloc((m ? m : 1) * sizeof(pv_value*));
    for (size_t j = 0; j < m; j++) arr[j] = st[mark_idx + 1 + j];
    *cnt = m;
    *n = mark_idx;                                /* 移除 MARK */
    return arr;
}

int pv_loads(const uint8_t *b, size_t len, pv_value **out) {
    pv_value **st = NULL; size_t n = 0, cap = 0;
    pv_value **memo = NULL; size_t memo_cap = 0, memo_n = 0;
    size_t i = 0; int rc = RP_OK;
    *out = NULL;

    if (i < len && b[i] == 0x80) { if (i + 1 >= len) { rc = RP_ERR_FORMAT; goto done; } i += 2; }

    while (i < len) {
        uint8_t op = b[i++];
        switch (op) {
        case 0x2e: /* STOP */
            if (n >= 1) { *out = st[n - 1]; st[n - 1] = NULL; }
            rc = RP_OK; goto done;
        case 0x28: push(&st, &n, &cap, PV_MARK); break;
        case 0x7d: push(&st, &n, &cap, new_val(PV_DICT)); break;
        case 0x5d: push(&st, &n, &cap, new_val(PV_LIST)); break;
        case 0x29: push(&st, &n, &cap, new_val(PV_TUPLE)); break;

        case 0x6c: case 0x74: { /* LIST / TUPLE: MARK .. -> 容器，替换 MARK 槽 */
            size_t cnt; pv_value **arr = pop_mark_region(st, &n, &cnt);
            if (!arr && cnt == 0 && n == 0) { rc = RP_ERR_FORMAT; goto done; }
            pv_value *c = new_val(op == 0x6c ? PV_LIST : PV_TUPLE);
            c->items = arr; c->n = cnt;
            st[n] = c;          /* 原 MARK 位置（n 已回退到该索引）放容器 */
            break;
        }
        case 0x85: case 0x86: case 0x87: { /* TUPLE1/2/3 */
            int cnt = (op == 0x85) ? 1 : (op == 0x86) ? 2 : 3;
            if (n < (size_t)cnt) { rc = RP_ERR_FORMAT; goto done; }
            pv_value *t = new_val(PV_TUPLE);
            t->n = cnt; t->items = malloc(cnt * sizeof(pv_value*));
            for (int j = 0; j < cnt; j++) t->items[j] = st[n - cnt + j];
            n -= cnt;
            push(&st, &n, &cap, t);
            break;
        }
        case 0x61: { /* APPEND: 弹 val，list 为栈顶-1，追加 */
            if (n < 2) { rc = RP_ERR_FORMAT; goto done; }
            pv_value *v = st[--n]; pv_value *lst = st[n-1];
            if (!lst || lst->type != PV_LIST) { rc = RP_ERR_FORMAT; goto done; }
            lst->items = realloc(lst->items, (lst->n + 1) * sizeof(pv_value*));
            lst->items[lst->n++] = v;
            break;
        }
        case 0x65: { /* APPENDS: MARK .. -> 追加进 MARK 之下的 list */
            size_t cnt; pv_value **arr = pop_mark_region(st, &n, &cnt);
            if (!arr) { rc = RP_ERR_FORMAT; goto done; }
            pv_value *lst = st[n-1];   /* MARK 之下即 list */
            if (!lst || lst->type != PV_LIST) { rc = RP_ERR_FORMAT; goto done; }
            lst->items = realloc(lst->items, (lst->n + cnt) * sizeof(pv_value*));
            for (size_t j = 0; j < cnt; j++) lst->items[lst->n++] = arr[j];
            free(arr);
            break;
        }
        case 0x73: { /* SETITEM: 弹 val, key，dict 为栈顶-1 */
            if (n < 3) { rc = RP_ERR_FORMAT; goto done; }
            pv_value *v = st[--n]; pv_value *key = st[--n]; pv_value *d = st[n-1];
            if (!d || d->type != PV_DICT || !key || key->type != PV_STR) { rc = RP_ERR_FORMAT; goto done; }
            d->keys = realloc(d->keys, (d->nk + 1) * sizeof(pv_value*));
            d->vals = realloc(d->vals, (d->nk + 1) * sizeof(pv_value*));
            d->keys[d->nk] = key; d->vals[d->nk] = v; d->nk++;
            break;
        }
        case 0x75: { /* SETITEMS: MARK (key val)* -> MARK 之下的 dict */
            size_t cnt; pv_value **arr = pop_mark_region(st, &n, &cnt);
            if (!arr) { rc = RP_ERR_FORMAT; goto done; }
            pv_value *d = st[n-1];
            if (!d || d->type != PV_DICT) { rc = RP_ERR_FORMAT; goto done; }
            for (size_t j = 0; j + 1 < cnt; j += 2) {
                pv_value *key = arr[j], *val = arr[j+1];
                if (key && key->type == PV_STR) {
                    d->keys = realloc(d->keys, (d->nk + 1) * sizeof(pv_value*));
                    d->vals = realloc(d->vals, (d->nk + 1) * sizeof(pv_value*));
                    d->keys[d->nk] = key; d->vals[d->nk] = val; d->nk++;
                }
            }
            free(arr);
            break;
        }

        /* ---- memo 写操作：记录栈顶到 memo[arg]，不弹栈 ---- */
        case 0x70: { if (i >= len) { rc = RP_ERR_FORMAT; goto done; } uint8_t a = b[i++];
                    if (n>0){ if(a+1>memo_cap){memo_cap=(a+1)*2;memo=realloc(memo,memo_cap*sizeof(pv_value*));} memo[a]=st[n-1]; } break; }
        case 0x71: { if (i >= len) { rc = RP_ERR_FORMAT; goto done; } uint8_t a = b[i++];
                    if (n>0){ if(a+1>memo_cap){memo_cap=(a+1)*2;memo=realloc(memo,memo_cap*sizeof(pv_value*));} memo[a]=st[n-1]; } break; }
        case 0x72: { if (i+4>len) { rc=RP_ERR_FORMAT; goto done; } uint32_t a=b[i]|(b[i+1]<<8)|(b[i+2]<<16)|((uint32_t)b[i+3]<<24); i+=4;
                    if (n>0){ if((size_t)a+1>memo_cap){memo_cap=a+1;memo=realloc(memo,memo_cap*sizeof(pv_value*));} memo[a]=st[n-1]; } break; }
        case 0x94: { if (n>0){ size_t a=memo_n; if(a+1>memo_cap){memo_cap=(a+1)*2;memo=realloc(memo,memo_cap*sizeof(pv_value*));} memo[memo_n++]=st[n-1]; } break; }

        /* ---- 字符串 ---- */
        case 0x8c: { if (i>=len){rc=RP_ERR_FORMAT;goto done;} uint8_t sl=b[i++]; if(i+sl>len){rc=RP_ERR_FORMAT;goto done;}
                    pv_value *v=new_val(PV_STR); v->s=malloc(sl+1); memcpy(v->s,b+i,sl); v->s[sl]=0; i+=sl; push(&st,&n,&cap,v); break; }
        case 0x58: { if (i+4>len){rc=RP_ERR_FORMAT;goto done;} uint32_t sl=b[i]|(b[i+1]<<8)|(b[i+2]<<16)|((uint32_t)b[i+3]<<24); i+=4; if(i+sl>len){rc=RP_ERR_FORMAT;goto done;}
                    pv_value *v=new_val(PV_STR); v->s=malloc(sl+1); memcpy(v->s,b+i,sl); v->s[sl]=0; i+=sl; push(&st,&n,&cap,v); break; }
        case 0x55: { if (i>=len){rc=RP_ERR_FORMAT;goto done;} uint8_t sl=b[i++]; if(i+sl>len){rc=RP_ERR_FORMAT;goto done;}
                    pv_value *v=new_val(PV_STR); v->s=malloc(sl+1); memcpy(v->s,b+i,sl); v->s[sl]=0; i+=sl; push(&st,&n,&cap,v); break; }
        case 0x54: { if (i+4>len){rc=RP_ERR_FORMAT;goto done;} uint32_t sl=b[i]|(b[i+1]<<8)|(b[i+2]<<16)|((uint32_t)b[i+3]<<24); i+=4; if(i+sl>len){rc=RP_ERR_FORMAT;goto done;}
                    pv_value *v=new_val(PV_STR); v->s=malloc(sl+1); memcpy(v->s,b+i,sl); v->s[sl]=0; i+=sl; push(&st,&n,&cap,v); break; }
        case 0x43: { if (i>=len){rc=RP_ERR_FORMAT;goto done;} uint8_t sl=b[i++]; if(i+sl>len){rc=RP_ERR_FORMAT;goto done;}
                    pv_value *v=new_val(PV_STR); v->s=malloc(sl+1); memcpy(v->s,b+i,sl); v->s[sl]=0; i+=sl; push(&st,&n,&cap,v); break; }
        case 0x42: { if (i+4>len){rc=RP_ERR_FORMAT;goto done;} uint32_t sl=b[i]|(b[i+1]<<8)|(b[i+2]<<16)|((uint32_t)b[i+3]<<24); i+=4; if(i+sl>len){rc=RP_ERR_FORMAT;goto done;}
                    pv_value *v=new_val(PV_STR); v->s=malloc(sl+1); memcpy(v->s,b+i,sl); v->s[sl]=0; i+=sl; push(&st,&n,&cap,v); break; }
        case 0x8e: { if (i+8>len){rc=RP_ERR_FORMAT;goto done;} uint64_t sl=0; for(int j=0;j<8;j++) sl|=((uint64_t)b[i+j])<<(8*j); i+=8; if(i+sl>len){rc=RP_ERR_FORMAT;goto done;}
                    pv_value *v=new_val(PV_STR); v->s=malloc(sl+1); memcpy(v->s,b+i,sl); v->s[sl]=0; i+=sl; push(&st,&n,&cap,v); break; }

        /* ---- 整数 ---- */
        case 0x4a: { if (i+4>len){rc=RP_ERR_FORMAT;goto done;} int32_t x=(int32_t)(b[i]|(b[i+1]<<8)|(b[i+2]<<16)|((uint32_t)b[i+3]<<24)); i+=4; push(&st,&n,&cap,new_int((uint64_t)(int64_t)x)); break; }
        case 0x4b: { if (i>=len){rc=RP_ERR_FORMAT;goto done;} push(&st,&n,&cap,new_int(b[i++])); break; }
        case 0x4d: { if (i+2>len){rc=RP_ERR_FORMAT;goto done;} uint64_t x=b[i]|(b[i+1]<<8); i+=2; push(&st,&n,&cap,new_int(x)); break; }
        case 0x8a: { if (i>=len){rc=RP_ERR_FORMAT;goto done;} uint8_t mlen=b[i++]; if(i+mlen>len){rc=RP_ERR_FORMAT;goto done;} uint64_t x=0; for(uint8_t j=0;j<mlen&&j<8;j++) x|=((uint64_t)b[i+j])<<(8*j); i+=mlen; push(&st,&n,&cap,new_int(x)); break; }
        case 0x8b: { if (i+4>len){rc=RP_ERR_FORMAT;goto done;} uint32_t mlen=b[i]|(b[i+1]<<8)|(b[i+2]<<16)|((uint32_t)b[i+3]<<24); i+=4; if(i+mlen>len){rc=RP_ERR_FORMAT;goto done;} uint64_t x=0; for(uint32_t j=0;j<mlen&&j<8;j++) x|=((uint64_t)b[i+j])<<(8*j); i+=mlen; push(&st,&n,&cap,new_int(x)); break; }

        default:
            fprintf(stderr, "[pickle_min] 不支持的操作码 0x%02x @ %zu\n", op, i - 1);
            rc = RP_ERR_FORMAT; goto done;
        }
    }

done:
    if (rc == RP_OK && *out == NULL && n >= 1) { *out = st[n - 1]; st[n - 1] = NULL; }
    for (size_t j = 0; j < n; j++) if (st[j] != PV_MARK) pv_free(st[j]);
    free(st);
    free(memo);
    return rc;
}

const pv_value *pv_dict_get(const pv_value *dict, const char *name) {
    if (!dict || dict->type != PV_DICT) return NULL;
    for (size_t i = 0; i < dict->nk; i++)
        if (dict->keys[i] && dict->keys[i]->type == PV_STR && strcmp(dict->keys[i]->s, name) == 0)
            return dict->vals[i];
    return NULL;
}
