/*
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "redis.h"
#include <math.h> /* isnan(), isinf() */

/*-----------------------------------------------------------------------------
 * String Commands  字符串相关命令
 *----------------------------------------------------------------------------*/

/* 检查给定的字符串长度是否超过了Redis限制的最大值（512MB）*/ 
static int checkStringLength(redisClient *c, long long size) {
    if (size > 512*1024*1024) {
        addReplyError(c,"string exceeds maximum allowed size (512MB)");
        return REDIS_ERR;
    }
    return REDIS_OK;
}

/* The setGenericCommand() function implements the SET operation with different
 * options and variants. This function is called in order to implement the
 * following commands: SET, SETEX, PSETEX, SETNX.
 *
 * 'flags' changes the behavior of the command (NX or XX, see belove).
 *
 * 'expire' represents an expire to set in form of a Redis object as passed
 * by the user. It is interpreted according to the specified 'unit'.
 *
 * 'ok_reply' and 'abort_reply' is what the function will reply to the client
 * if the operation is performed, or when it is not because of NX or
 * XX flags.
 *
 * If ok_reply is NULL "+OK" is used.
 * If abort_reply is NULL, "$-1" is used. */
/*  setGenericCommand函数实现了SET操作的不同版本：SET、SETEX、PSETEX和SETNX

    参数flag用来指定该函数的操作类型（REDIS_SET_NX or REDIS_SET_XX or REDIS_SET_NO_FLAGS）。

    参数exipre以Redis对象的形式指定对象的过期时间，这个过期时间的格式（秒 or 毫秒）则由参数unit指定。

    参数ok_reply和abort_reply指定了命令的回复内容。
    如果ok_reply为NULL，则"+OK"被返回。
    如果abort_reply为NULL，则"$-1"被返回。
*/

#define REDIS_SET_NO_FLAGS 0
#define REDIS_SET_NX (1<<0)     /* Set if key not exists. */
#define REDIS_SET_XX (1<<1)     /* Set if key exists. */

void setGenericCommand(redisClient *c, int flags, robj *key, robj *val, robj *expire, int unit, robj *ok_reply, robj *abort_reply) {
    // 过期时间，初始化为0
    long long milliseconds = 0; /* initialized to avoid any harmness warning */

    // 取出过期时间
    if (expire) {
        if (getLongLongFromObjectOrReply(c, expire, &milliseconds, NULL) != REDIS_OK)
            return;
        // 给定的过期时间不合法，返回
        if (milliseconds <= 0) {
            addReplyErrorFormat(c,"invalid expire time in %s",c->cmd->name);
            return;
        }
        // 统一转换为以毫秒为单位，因为Redis中总是以毫秒为单位的UNIX 时间戳作为过期时间戳
        if (unit == UNIT_SECONDS) milliseconds *= 1000;
    }

    // NX选项：设置前需要检查指定键是否已经存。如果键不存在才对键进行设置操作。
    // XX选项：设置前需要检查指定键是否已经存。如果键已经存在时才对键进行设置操作。
    // 对应以上规则，如果设置了NX或XX选项，需要进行相应的检查
    if ((flags & REDIS_SET_NX && lookupKeyWrite(c->db,key) != NULL) ||
        (flags & REDIS_SET_XX && lookupKeyWrite(c->db,key) == NULL))
    {
        addReply(c, abort_reply ? abort_reply : shared.nullbulk);
        return;
    }
    // 将key和相应的value关联到数据库db中
    setKey(c->db,key,val);
    server.dirty++;
    // 设置过期时间，再次证明Redis中总是以毫秒为单位的UNIX 时间戳作为过期时间戳
    if (expire) setExpire(c->db,key,mstime()+milliseconds);
    notifyKeyspaceEvent(REDIS_NOTIFY_STRING,"set",key,c->db->id);
    if (expire) notifyKeyspaceEvent(REDIS_NOTIFY_GENERIC,
        "expire",key,c->db->id);
    addReply(c, ok_reply ? ok_reply : shared.ok);
}

/* SET key value [NX] [XX] [EX <seconds>] [PX <milliseconds>] */
/* set命令，格式为：SET key value [NX] [XX] [EX <seconds>] [PX <milliseconds>]*/
void setCommand(redisClient *c) {
    int j;
    robj *expire = NULL;
    int unit = UNIT_SECONDS;
    int flags = REDIS_SET_NO_FLAGS;

    // 获取选项参数
    for (j = 3; j < c->argc; j++) {
        char *a = c->argv[j]->ptr;
        robj *next = (j == c->argc-1) ? NULL : c->argv[j+1];

        // 获取NX选项
        if ((a[0] == 'n' || a[0] == 'N') &&
            (a[1] == 'x' || a[1] == 'X') && a[2] == '\0') {
            flags |= REDIS_SET_NX;
        } 
        // 获取XX选项
        else if ((a[0] == 'x' || a[0] == 'X') &&
                   (a[1] == 'x' || a[1] == 'X') && a[2] == '\0') {
            flags |= REDIS_SET_XX;
        } 
        // 获取EX选项
        else if ((a[0] == 'e' || a[0] == 'E') &&
                   (a[1] == 'x' || a[1] == 'X') && a[2] == '\0' && next) {
            unit = UNIT_SECONDS;
            expire = next;
            j++;
        } 
        // 获取PX选项
        else if ((a[0] == 'p' || a[0] == 'P') &&
                   (a[1] == 'x' || a[1] == 'X') && a[2] == '\0' && next) {
            unit = UNIT_MILLISECONDS;
            expire = next;
            j++;
        } 
        // 语法错误
        else {
            addReply(c,shared.syntaxerr);
            return;
        }
    }

    // 对value进行编码
    c->argv[2] = tryObjectEncoding(c->argv[2]);
    setGenericCommand(c,flags,c->argv[1],c->argv[2],expire,unit,NULL,NULL);
}

/* setnx命令，相当于SET key value EX */
void setnxCommand(redisClient *c) {
    c->argv[2] = tryObjectEncoding(c->argv[2]);
    setGenericCommand(c,REDIS_SET_NX,c->argv[1],c->argv[2],NULL,0,shared.cone,shared.czero);
}

/* setex命令 */
void setexCommand(redisClient *c) {
    c->argv[3] = tryObjectEncoding(c->argv[3]);
    setGenericCommand(c,REDIS_SET_NO_FLAGS,c->argv[1],c->argv[3],c->argv[2],UNIT_SECONDS,NULL,NULL);
}

/* psetex命令 */
void psetexCommand(redisClient *c) {
    c->argv[3] = tryObjectEncoding(c->argv[3]);
    setGenericCommand(c,REDIS_SET_NO_FLAGS,c->argv[1],c->argv[3],c->argv[2],UNIT_MILLISECONDS,NULL,NULL);
}

/* get命令的底层函数 */
int getGenericCommand(redisClient *c) {
    robj *o;

    // 取出key对应的字符串对象，如果该key不存在则o被设置为NULL
    if ((o = lookupKeyReadOrReply(c,c->argv[1],shared.nullbulk)) == NULL)
        return REDIS_OK;

    // 对上一步获取的字符串对象进行类型检查
    if (o->type != REDIS_STRING) {
        addReply(c,shared.wrongtypeerr);
        return REDIS_ERR;
    } 
    // 添加到回复消息中
    else {
        addReplyBulk(c,o);
        return REDIS_OK;
    }
}

/* get命令*/
void getCommand(redisClient *c) {
    getGenericCommand(c);
}

/* getset命令 */
void getsetCommand(redisClient *c) {
    // 先判断指定key对应的字符串对象是否存在，如果不存在直接返回
    if (getGenericCommand(c) == REDIS_ERR) return;
    // 对新值进行编码
    c->argv[2] = tryObjectEncoding(c->argv[2]);
    // 为指定key关联一个新值
    setKey(c->db,c->argv[1],c->argv[2]);
    notifyKeyspaceEvent(REDIS_NOTIFY_STRING,"set",c->argv[1],c->db->id);
    server.dirty++;
}

/* setrange命令将字符串中偏移量为offset后的子串覆盖为指定的值，该命令返回修改后的字符串的长度。*/
void setrangeCommand(redisClient *c) {
    robj *o;
    long offset;
    // 进行覆盖的新值
    sds value = c->argv[3]->ptr;

    // 获取offset参数
    if (getLongFromObjectOrReply(c,c->argv[2],&offset,NULL) != REDIS_OK)
        return;

    // 检查offset参数是否合法
    if (offset < 0) {
        addReplyError(c,"offset is out of range");
        return;
    }

    // 取出指定key所关联的字符串对象
    o = lookupKeyWrite(c->db,c->argv[1]);
    // 如果目标字符串对象不存在，执行下面代码
    if (o == NULL) {
        /* Return 0 when setting nothing on a non-existing string */
        // 如果给定新值为空，直接返回
        if (sdslen(value) == 0) {
            addReply(c,shared.czero);
            return;
        }

        /* Return when the resulting string exceeds allowed size */
        // 检查修改后的字符串的长度是否超过Redis中最大字符串长度限制，如果超过则直接返回
        if (checkStringLength(c,offset+sdslen(value)) != REDIS_OK)
            return;

        // 创建一个新的字符串对象，并添加到数据库db中
        o = createObject(REDIS_STRING,sdsempty());
        dbAdd(c->db,c->argv[1],o);
    } 
    // 如果目标字符串对象存在，执行下面代码
    else {
        size_t olen;

        /* Key exists, check type */
        // 检查该对象的类型
        if (checkType(c,o,REDIS_STRING))
            return;

        /* Return existing string length when setting nothing */
        // 获取原对象的长度
        olen = stringObjectLen(o);
        // 如果给定新值为空，直接返回原对象长度
        if (sdslen(value) == 0) {
            addReplyLongLong(c,olen);
            return;
        }

        /* Return when the resulting string exceeds allowed size */
        // 检查修改后的字符串的长度是否超过Redis中最大字符串长度限制，如果超过则直接返回
        if (checkStringLength(c,offset+sdslen(value)) != REDIS_OK)
            return;

        /* Create a copy when the object is shared or encoded. */
        o = dbUnshareStringValue(c->db,c->argv[1],o);
    }

    if (sdslen(value) > 0) {
        // 空间扩展
        o->ptr = sdsgrowzero(o->ptr,offset+sdslen(value));
        // 将value复制到字符串中指定偏移量开始的位置
        memcpy((char*)o->ptr+offset,value,sdslen(value));
        signalModifiedKey(c->db,c->argv[1]);
        notifyKeyspaceEvent(REDIS_NOTIFY_STRING,
            "setrange",c->argv[1],c->db->id);
        server.dirty++;
    }
    addReplyLongLong(c,sdslen(o->ptr));
}

/* getrange命令 */
void getrangeCommand(redisClient *c) {
    robj *o;
    long long start, end;
    char *str, llbuf[32];
    size_t strlen;

    // 获取start参数
    if (getLongLongFromObjectOrReply(c,c->argv[2],&start,NULL) != REDIS_OK)
        return;
    // 获取end参数
    if (getLongLongFromObjectOrReply(c,c->argv[3],&end,NULL) != REDIS_OK)
        return;
    // 从数据库db中找到该key对应的字符串对象并进行类型检查
    if ((o = lookupKeyReadOrReply(c,c->argv[1],shared.emptybulk)) == NULL ||
        checkType(c,o,REDIS_STRING)) return;

    // 如果为整型编码，先转换为字符串，方便操作
    if (o->encoding == REDIS_ENCODING_INT) {
        str = llbuf;
        strlen = ll2string(llbuf,sizeof(llbuf),(long)o->ptr);
    } else {
        str = o->ptr;
        strlen = sdslen(str);
    }

    /* Convert negative indexes */
    // 将负数索引转化为正数索引
    if (start < 0) start = strlen+start;
    if (end < 0) end = strlen+end;
    // 如果start或end小于0，上提到数值0
    if (start < 0) start = 0;
    if (end < 0) end = 0;
    if ((unsigned long long)end >= strlen) end = strlen-1;

    /* Precondition: end >= 0 && end < strlen, so the only condition where
     * nothing can be returned is: start > end. */
    // 指定的范围左边界大于右边界或该范围为空，返回空消息
    if (start > end || strlen == 0) {
        addReply(c,shared.emptybulk);
    } 
    // 指定的范围不为空，直接返回该范围内的子串
    else {
        addReplyBulkCBuffer(c,(char*)str+start,end-start+1);
    }
}

/* mget命令 */
void mgetCommand(redisClient *c) {
    int j;

    addReplyMultiBulkLen(c,c->argc-1);
    // 遍历每一个输入值，逐一获取所对应的字符串对象
    for (j = 1; j < c->argc; j++) {
        // 查找当前key所关联的字符串对象
        robj *o = lookupKeyRead(c->db,c->argv[j]);
        // 若该对象为空，返回空回复
        if (o == NULL) {
            addReply(c,shared.nullbulk);
        } else {
            // 类型检查
            if (o->type != REDIS_STRING) {
                addReply(c,shared.nullbulk);
            } else {
                addReplyBulk(c,o);
            }
        }
    }
}

/* mset*命令的底层实现。参数nx为0执行mset命令，参数nx为1执行msetnx命令 */
void msetGenericCommand(redisClient *c, int nx) {
    int j, busykeys = 0;

    // 对于mset命令，指定的key和设置的新值必须成对出现，因此参数个数为2的整数倍。这里先做一次检查
    if ((c->argc % 2) == 0) {
        addReplyError(c,"wrong number of arguments for MSET");
        return;
    }
    /* Handle the NX flag. The MSETNX semantic is to return zero and don't
     * set nothing at all if at least one already key exists. */
    // 处理NX选项参数。对于MSETNX命令，设置前需要检查指定key是否已经存，只要有一个key存在则放弃执行该命令。
    if (nx) {
        // 遍历所有的输入key
        for (j = 1; j < c->argc; j += 2) {
            // 如果该key已经存在，增加计数值
            if (lookupKeyWrite(c->db,c->argv[j]) != NULL) {
                busykeys++;
            }
        }
        // 在设置NX参数的情况下，只要输入key中有一个已经存在于数据库db中，直接返回0
        if (busykeys) {
            addReply(c, shared.czero);
            return;
        }
    }

    // 遍历输入的key和新value值，进行更新操作
    for (j = 1; j < c->argc; j += 2) {
        // 对新值进行编码
        c->argv[j+1] = tryObjectEncoding(c->argv[j+1]);
        // 为指定key设置新值
        setKey(c->db,c->argv[j],c->argv[j+1]);
        notifyKeyspaceEvent(REDIS_NOTIFY_STRING,"set",c->argv[j],c->db->id);
    }
    server.dirty += (c->argc-1)/2;
    addReply(c, nx ? shared.cone : shared.ok);
}

/* mset命令 */
void msetCommand(redisClient *c) {
    msetGenericCommand(c,0);
}

/* msetnx命令 */
void msetnxCommand(redisClient *c) {
    msetGenericCommand(c,1);
}

/* helper函数，对指定key的字符串对象执行加法操作（减法也可以转换为加法），步长由参数incr指定。*/
void incrDecrCommand(redisClient *c, long long incr) {
    long long value, oldvalue;
    robj *o, *new;

    // 取出字符串对象
    o = lookupKeyWrite(c->db,c->argv[1]);
    // 检查该字符串对象是否存在，如果存在则进一步检查其类型
    if (o != NULL && checkType(c,o,REDIS_STRING)) return;
    // 将该对象转换为整数值，如果无法转换则返回
    if (getLongLongFromObjectOrReply(c,o,&value,NULL) != REDIS_OK) return;

    // 判断执行加法操作后是否会发生溢出
    oldvalue = value;
    if ((incr < 0 && oldvalue < 0 && incr < (LLONG_MIN-oldvalue)) ||
        (incr > 0 && oldvalue > 0 && incr > (LLONG_MAX-oldvalue))) {
        addReplyError(c,"increment or decrement would overflow");
        return;
    }
    // 真正执行加法操作
    value += incr;

    if (o && o->refcount == 1 && o->encoding == REDIS_ENCODING_INT &&
        (value < 0 || value >= REDIS_SHARED_INTEGERS) &&
        value >= LONG_MIN && value <= LONG_MAX)
    {
        new = o;
        o->ptr = (void*)((long)value);
    } else {
        // 创建一个新对象
        new = createStringObjectFromLongLong(value);
        // 如果原字符串对象已经存在则替换之，否则将新值添加到数据库db中
        if (o) {
            dbOverwrite(c->db,c->argv[1],new);
        } else {
            dbAdd(c->db,c->argv[1],new);
        }
    }
    signalModifiedKey(c->db,c->argv[1]);
    notifyKeyspaceEvent(REDIS_NOTIFY_STRING,"incrby",c->argv[1],c->db->id);
    server.dirty++;
    addReply(c,shared.colon);
    addReply(c,new);
    addReply(c,shared.crlf);
}

/* incr命令 */
void incrCommand(redisClient *c) {
    incrDecrCommand(c,1);
}

/* decr命令 */
void decrCommand(redisClient *c) {
    incrDecrCommand(c,-1);
}

/* incrby命令 */
void incrbyCommand(redisClient *c) {
    long long incr;

    if (getLongLongFromObjectOrReply(c, c->argv[2], &incr, NULL) != REDIS_OK) return;
    incrDecrCommand(c,incr);
}

/* decrby命令 */
void decrbyCommand(redisClient *c) {
    long long incr;

    if (getLongLongFromObjectOrReply(c, c->argv[2], &incr, NULL) != REDIS_OK) return;
    incrDecrCommand(c,-incr);
}

/* incrbyfloat命令 */
void incrbyfloatCommand(redisClient *c) {
    long double incr, value;
    robj *o, *new, *aux;

    // 取出指定key对应的字符串对象
    o = lookupKeyWrite(c->db,c->argv[1]);
    // 检查该字符串对象是否存在，如果存在则进一步检查其类型 
    if (o != NULL && checkType(c,o,REDIS_STRING)) return;
    // 将该对象转换为浮点数，同时对incrbyfloat命令指定的步长值也转换为浮点数，如果两者中有一个无法转换就直接返回
    if (getLongDoubleFromObjectOrReply(c,o,&value,NULL) != REDIS_OK ||
        getLongDoubleFromObjectOrReply(c,c->argv[2],&incr,NULL) != REDIS_OK)
        return;

    // 执行加法操作
    value += incr;
    if (isnan(value) || isinf(value)) {
        addReplyError(c,"increment would produce NaN or Infinity");
        return;
    }
    // 创建一个新对象
    new = createStringObjectFromLongDouble(value,1);
    // 如果原字符串对象已经存在则替换之，否则将新值添加到数据库db中
    if (o)
        dbOverwrite(c->db,c->argv[1],new);
    else
        dbAdd(c->db,c->argv[1],new);
    signalModifiedKey(c->db,c->argv[1]);
    notifyKeyspaceEvent(REDIS_NOTIFY_STRING,"incrbyfloat",c->argv[1],c->db->id);
    server.dirty++;
    addReplyBulk(c,new);

    /* Always replicate INCRBYFLOAT as a SET command with the final value
     * in order to make sure that differences in float precision or formatting
     * will not create differences in replicas or after an AOF restart. */
    // 在传播INCRBYFLOAT命令是，以SET命令替换之。目的是为了确保不会因为浮点数精度或格式化精度不同而导致数据不一致
    aux = createStringObject("SET",3);
    rewriteClientCommandArgument(c,0,aux);
    decrRefCount(aux);
    rewriteClientCommandArgument(c,2,new);
}

/* append命令实现 */
void appendCommand(redisClient *c) {
    size_t totlen;
    robj *o, *append;

    // 取出指定key所对应的字符串对象
    o = lookupKeyWrite(c->db,c->argv[1]);
    // 如果目标字符串对象不存在，则创建一个并添加到数据库db中
    if (o == NULL) {
        /* Create the key */
        c->argv[2] = tryObjectEncoding(c->argv[2]);
        dbAdd(c->db,c->argv[1],c->argv[2]);
        incrRefCount(c->argv[2]);
        totlen = stringObjectLen(c->argv[2]);
    } 
    // 如果目标字符串对象存在，则执行下面代码
    else {
        /* Key exists, check type */
        if (checkType(c,o,REDIS_STRING))
            return;

        /* "append" is an argument, so always an sds */
        // 先判断如果执行append操作后，检查新字符串的长度是否会超过Redis的限制
        append = c->argv[2];
        totlen = stringObjectLen(o)+sdslen(append->ptr);
        if (checkStringLength(c,totlen) != REDIS_OK)
            return;

        /* Append the value */
        // 真正执行append操作，调用字符串的内部函数sdscatlen实现
        o = dbUnshareStringValue(c->db,c->argv[1],o);
        o->ptr = sdscatlen(o->ptr,append->ptr,sdslen(append->ptr));
        totlen = sdslen(o->ptr);
    }
    signalModifiedKey(c->db,c->argv[1]);
    notifyKeyspaceEvent(REDIS_NOTIFY_STRING,"append",c->argv[1],c->db->id);
    server.dirty++;
    addReplyLongLong(c,totlen);
}

/* strlen命令 */
void strlenCommand(redisClient *c) {
    robj *o;
    if ((o = lookupKeyReadOrReply(c,c->argv[1],shared.czero)) == NULL ||
        checkType(c,o,REDIS_STRING)) return;
    addReplyLongLong(c,stringObjectLen(o));
}
