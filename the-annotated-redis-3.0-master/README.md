# Redis-3.0 源码注释版
==============================

本项目是带有详细注释的 Redis 3.0 版源码。

在我的博客了有较为详细的源码分析文章，大家可以参考学习。

博客地址： http://blog.csdn.net/xiejingfa/article/category/6064201



希望这个项目能给您学习和了解 Redis 带来一些帮助。

最后，求star。


Enjoy Redis！



附：各源码文件的功能和相应的文章链接
------------------------------------------

| 文件           | 链接           | 
| ------------- | ------------- | 
| ``adlist.c`` 、 ``adlist.h``      | 双向链表list数据结构实现，[【Redis源码剖析】 - Redis内置数据结构之双向链表list](http://blog.csdn.net/xiejingfa/article/details/50938028)。 |
| ``sds.c`` 、 ``sds.h``      | 字符串sds数据结构实现，[ 【Redis源码剖析】 - Redis内置数据结构之字符串sds](http://blog.csdn.net/xiejingfa/article/details/50972592)。     |
| ``dict.c`` 、 ``dict.h``      | 字典dict数据结构实现，[【Redis源码剖析】 - Redis内置数据结构之字典dict](http://blog.csdn.net/xiejingfa/article/details/51018337)。     |
| ``ziplist.c`` 、 ``ziplist.h``      | 压缩列表ziplist数据结构实现，ziplist是为了节省列表空间而设计一种特殊编码方式，[【Redis源码剖析】 - Redis内置数据结构之压缩列表ziplist](http://blog.csdn.net/xiejingfa/article/details/51072326)。     |
| ``zipmap.c`` 、 ``zipmap.h``      | 压缩字典zipmap数据结构实现，zipmap是为了节省哈希表空间而设计一种特殊编码方式，[ 【Redis源码剖析】 - Redis内置数据结构值压缩字典zipmap](http://blog.csdn.net/xiejingfa/article/details/51111230)。     |
| ``intset.c`` 、 ``intset.h``      | 整数集合intset数据结构实现，[【Redis源码剖析】 - Reids内置数据结构之整数集合intset](http://blog.csdn.net/xiejingfa/article/details/51124203)。     | 
| ``object.c``      | Redis对象redisObject的实现，函数声明在redis.h文件中，[【Redis源码剖析】 - Redis数据类型之redisObject](http://blog.csdn.net/xiejingfa/article/details/51140041)。     |
| ``t_string.c``      | Redis数据类型string的实现，函数声明在redis.h文件中。     |
| ``t_list.c``      | Redis数据类型List的实现，函数声明在redis.h文件中，[【Redis源码剖析】 - Redis数据类型之列表List](http://blog.csdn.net/xiejingfa/article/details/51166709)。     |
| ``t_hash.c``      | Redis数据类型hash的实现，函数声明在redis.h文件中。     |
| ``t_set.c``      | Redis数据类型set的实现，函数声明在redis.h文件中。     |
| ``t_zset.c``      | Redis数据类型zset的实现，函数声明在redis.h文件中，[【Redis源码剖析】 - Redis数据类型之有序集合zset](http://blog.csdn.net/xiejingfa/article/details/51231967)。     |
| ``multi.c``      | Redis事务的实现，函数声明在redis.h文件中，[【Redis源码剖析】 - Redis之事务的实现原理](http://blog.csdn.net/xiejingfa/article/details/51262268)。     |
| ``db.c``      | Redis数据库，函数声明在redis.h文件中，[【Redis源码剖析】 - Redis之数据库redisDb](http://blog.csdn.net/xiejingfa/article/details/51321282)。     |
| ``rio.c``、 ``rio.h``            | Redis对系统I/O操作的封装，[【Redis源码剖析】 - Redis IO操作之rio](http://blog.csdn.net/xiejingfa/article/details/51433696)。     |
| ``crc64.c``、 ``crc64.h``、 ``crc16.h``            |  计算循环冗余校验(Cyclic Redundancy Check, CRC)。     |
| ``rdb.c``、 ``rdb.h``            |  Redis持久化机制RDB的实现，[【Redis源码剖析】 - Redis持久化之RDB](http://blog.csdn.net/xiejingfa/article/details/51553370)   |
| ``aof.c``           |  Redis持久化机制AOF的实现，[【Redis源码剖析】 - Redis持久化之AOF](http://blog.csdn.net/xiejingfa/article/details/51644390)   |
| ``pubsub.c``           |  Redis发布订阅功能的实现  |







