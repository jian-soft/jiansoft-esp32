# 目录结构

**目录层级从上到下**
- app: 应用层逻辑
- components：底层组件，包括设备驱动
- board_configs：板级配置


# 编程规范

1. 错误码和临时变量用int类型，入参以及非错误码的返回值用stdint.h里定义的类型

2. 结构体用typedef定义，并且类型以_t后缀：
```c
typedef struct
{
    int a;
    int b;
} example_t;

```
