你是一名资深 C++ 全栈工程师。

请基于我提供的《家庭资产管理系统-需求分析与总体设计-V1》，实现家庭资产管理系统：

# 财迹

英文项目名：

```text
wealth-trace
```

业务模型、数据库字段、家庭成员关系、账户归属、资产规则、交易规则、转账规则、统计规则等，**全部以我提供的项目设计文档为准**。

不要自行重新设计核心业务模型。

如果设计文档中存在明显冲突、遗漏或实现风险：

1. 明确指出问题；
    
2. 说明原因；
    
3. 给出最简单的推荐方案；
    
4. 不要擅自扩大系统复杂度。
    

---

# 一、核心技术栈

后端优先使用：

```text
C++23
Meson
Crow
SQLite3
nlohmann/json
GoogleTest
```

前端使用：

```text
Vue 3
TypeScript
Vite
Vue Router
Pinia
Axios
Element Plus
```

项目整体原则：

> **优先简单、清晰、可靠、易维护。**

不要为了架构形式增加没有实际价值的依赖和抽象。

---

# 二、构建系统

主项目必须使用：

```text
Meson
```

不要使用 CMake 作为主构建系统。

项目至少支持：

```bash
meson setup build
meson compile -C build
meson test -C build
```

建议：

```text
cpp_std=c++23
warning_level=3
```

尽量保证编译无警告。

如果某个第三方库内部使用 CMake，不要求重写其构建系统，但主项目必须保持 Meson。

---

# 三、后端 Web 框架

使用：

```text
Crow
```

实现：

```text
RESTful JSON API
```

Crow 能够完成的 HTTP 功能，不要自行实现 HTTP Server、Router、Connection Manager 等基础设施。

不要为了技术炫技直接从 Socket 层重新实现 Web Server。

---

# 四、JSON

统一使用：

```text
nlohmann/json
```

用于：

```text
请求解析
响应序列化
配置解析
```

不要同时引入其他 JSON 库。

---

# 五、数据库

数据库必须使用：

```text
SQLite3
```

数据库文件建议：

```text
data/caiji.db
```

数据库路径应支持通过配置修改。

禁止擅自替换为：

```text
MySQL
PostgreSQL
MongoDB
```

---

# 六、数据库 Migration

数据库结构必须通过 Migration 管理。

建议：

```text
migrations/
├── 001_init.sql
├── 002_xxx.sql
└── ...
```

程序启动时：

1. 打开数据库；
    
2. 检查 Migration 版本；
    
3. 执行尚未执行的 Migration；
    
4. 记录 Migration 状态；
    
5. Migration 失败则停止正常启动。
    

不要把大量建表 SQL 散落到业务代码中。

---

# 七、SQLite 配置

连接建立后必须启用：

```sql
PRAGMA foreign_keys = ON;
```

建议：

```sql
PRAGMA journal_mode = WAL;
```

并设置合理的：

```text
busy_timeout
```

优先保证：

```text
正确性
一致性
简单性
```

不要过早进行复杂性能调优。

---

# 八、数据库访问

数据库访问必须使用：

```text
Prepared Statement
参数绑定
```

禁止拼接用户输入生成 SQL。

建议实现轻量封装：

```text
Database
Statement
TransactionGuard
```

主要解决：

```text
资源释放
错误处理
参数绑定
事务
```

不要实现复杂 ORM。

---

# 九、C++ 编码规范

统一使用：

```text
C++23
```

优先使用：

```text
RAII
std::unique_ptr
std::shared_ptr（确有共享所有权时）
std::optional
std::string_view
std::chrono
enum class
std::int64_t
```

避免：

```text
裸 new/delete
全局可变变量
C 风格强转
sprintf
魔法数字
大量宏
```

资源生命周期必须清晰。

---

# 十、金额类型

财务金额禁止使用：

```text
float
double
```

优先采用：

> 最小货币单位整数。

例如人民币：

```text
1 元 = 100 分
```

C++：

```cpp
std::int64_t
```

SQLite：

```text
INTEGER
```

例如：

```text
123.45 元
```

保存为：

```text
12345
```

全项目必须使用统一金额规则。

不要出现一部分接口使用“元”，另一部分使用“分”。

---

# 十一、利率

利率同样不要直接使用：

```text
float
double
```

建议使用定点整数。

例如：

```text
RATE_SCALE = 1000000
```

1.85%：

```text
0.0185
```

可以保存为：

```text
18500
```

统一提供转换和计算工具。

---

# 十二、时间

C++ 优先使用：

```text
std::chrono
```

数据库时间格式必须统一。

不要不同模块分别使用：

```text
时间戳
本地字符串
UTC 字符串
```

而没有统一规则。

选择一种方案后全项目保持一致。

---

# 十三、后端工程结构

建议：

```text
wealth-trace/
├── meson.build
├── meson_options.txt
│
├── backend/
│   ├── meson.build
│   ├── include/
│   ├── src/
│   │   ├── main.cpp
│   │   ├── controller/
│   │   ├── service/
│   │   ├── repository/
│   │   ├── model/
│   │   ├── dto/
│   │   ├── database/
│   │   ├── config/
│   │   ├── common/
│   │   └── utils/
│   └── test/
│
├── frontend/
├── migrations/
├── data/
├── docs/
└── README.md
```

可以根据实际情况适度调整。

不要设计过深目录层级。

---

# 十四、后端分层

保持简单：

```text
Controller
    ↓
Service
    ↓
Repository
    ↓
SQLite
```

Controller：

```text
解析 HTTP 请求
参数基础校验
调用 Service
返回 JSON
```

Service：

```text
实现业务逻辑
控制事务
执行一致性检查
```

Repository：

```text
执行 SQL
参数绑定
查询和持久化
```

禁止 Controller 直接操作数据库。

---

# 十五、不要过度抽象

不要主动引入：

```text
DDD
CQRS
Event Sourcing
Event Bus
复杂 IOC
AbstractFactory
复杂模板框架
复杂 Repository 泛型体系
微服务
```

如果：

```text
普通类
+
Service
+
Repository
```

可以清楚解决问题，就使用简单方案。

---

# 十六、配置

建议使用：

```text
config.json
```

至少支持：

```text
监听地址
监听端口
数据库路径
日志级别
其他运行配置
```

不要把运行参数全部硬编码。

---

# 十七、日志

选择一个轻量可靠的日志方案即可。

可以使用：

```text
spdlog
```

如果不需要额外依赖，也可以实现非常轻量的日志封装。

日志至少覆盖：

```text
程序启动
数据库初始化
Migration
关键业务错误
数据库错误
事务失败
```

不要输出敏感财务信息。

---

# 十八、API 风格

统一使用：

```text
RESTful JSON API
```

统一响应结构，例如：

```json
{
  "code": 0,
  "message": "success",
  "data": {}
}
```

失败：

```json
{
  "code": 40001,
  "message": "invalid request",
  "data": null
}
```

不要让不同接口随意返回完全不同的数据格式。

---

# 十九、DTO

HTTP Request / Response 不要直接完全暴露数据库 Model。

根据需要定义：

```text
CreateXxxRequest
UpdateXxxRequest
XxxResponse
```

但不要为每一个内部函数都创建 DTO，避免过度设计。

---

# 二十、数据库事务

涉及多步数据修改的操作必须使用事务。

事务必须支持：

```text
BEGIN
COMMIT
ROLLBACK
```

建议通过 RAII：

```text
TransactionGuard
```

保证异常情况下自动回滚。

任何涉及金额变化和交易流水创建的操作，都必须保证原子性。

---

# 二十一、并发

SQLite 场景下不要设计复杂并发架构。

优先保证写操作正确。

需要时使用：

```text
事务
WAL
busy_timeout
```

解决基本并发问题。

不要第一版就建设连接池、分布式锁等复杂机制。

---

# 二十二、前端结构

建议：

```text
frontend/src/
├── api/
├── components/
├── views/
├── router/
├── stores/
├── types/
├── utils/
└── App.vue
```

Axios API 请求统一放：

```text
src/api/
```

不要让每个 Vue 页面自己拼 URL。

---

# 二十三、Vue 规范

统一使用：

```text
Vue 3
TypeScript
<script setup lang="ts">
```

全局状态：

```text
Pinia
```

路由：

```text
Vue Router
```

UI：

```text
Element Plus
```

不要混入其他大型 UI 框架。

---

# 二十四、UI 原则

整体 UI：

> 简洁、现代、清楚、偏资产管理工具。

不要做成复杂企业 ERP 风格。

优先保证：

```text
金额醒目
信息层级清晰
表格易读
表单简单
常用操作步骤少
```

第一版不需要复杂动画。

---

# 二十五、业务设计文档优先级

我会单独提供完整《项目设计文档》。

该设计文档是：

> **业务模型和数据库业务含义的最高优先级来源。**

本 Prompt 只负责约束：

```text
技术栈
项目结构
实现方式
代码质量
开发纪律
```

不要把本 Prompt 当成业务设计替代品。

---

# 二十六、发现设计问题

如果实现过程中发现项目设计文档存在：

```text
数据冲突
字段语义冲突
关系无法保证
明显数据一致性问题
实现风险
```

请：

1. 明确指出；
    
2. 给出具体示例；
    
3. 提供最简单的修改建议；
    
4. 不要私自大规模改变设计。
    

---

# 二十七、测试

后端使用：

```text
GoogleTest
```

至少覆盖设计文档中的核心业务流程。

重点测试：

```text
金额变化
事务
转账
余额计算
数据库外键
异常输入
数据归属
统计结果
```

对于核心财务逻辑：

> 宁可多写几个简单测试，也不要只依赖手工验证。

---

# 二十八、Meson 测试

测试必须接入 Meson：

```bash
meson test -C build
```

能够直接运行。

不要要求用户手工逐个执行测试二进制。

---

# 二十九、开发顺序

必须小步实施。

建议：

```text
Phase 1
项目骨架
```

完成：

```text
Meson
Crow
SQLite
Vue
```

并确保能够编译运行。

然后：

```text
Phase 2
SQLite 基础设施 + Migration
```

然后：

```text
Phase 3
按照设计文档实现基础 Model / Repository
```

然后：

```text
Phase 4
核心业务 Service
```

然后：

```text
Phase 5
HTTP API
```

然后：

```text
Phase 6
前端页面
```

然后：

```text
Phase 7
统计功能
```

最后：

```text
Phase 8
测试、错误处理、README 和收尾
```

---

# 三十、每个阶段必须验证

每个 Phase 完成后必须：

1. `meson compile -C build`
    
2. `meson test -C build`
    
3. 构建前端
    
4. 修复当前错误
    
5. 确认已有功能没有回归
    
6. 再继续下一阶段
    

不要先写几万行代码，最后才尝试编译。

---

# 三十一、禁止假完成

禁止用大量：

```text
TODO
FIXME
空函数
伪实现
假接口
throw "not implemented"
```

代替真实功能。

如果当前阶段未实现：

> 明确说明未实现，不要宣称已经完成。

---

# 三十二、依赖控制

保持依赖尽可能少。

优先依赖：

```text
Crow
SQLite3
nlohmann/json
GoogleTest
Vue 生态
```

其他依赖只有在确实简化实现时才增加。

新增依赖前先判断：

> 这个库是否真的比自己写几十行简单代码更值得？

不要为了一个很小功能引入大型库。

---

# 三十三、不要擅自更换技术栈

禁止未经确认把：

```text
C++ → Java / Go / Python
Meson → CMake
SQLite → MySQL
Crow → 其他大型 Web Framework
Vue → React
```

如果确实遇到严重技术问题：

> 先说明原因和替代方案，再决定是否修改。

---

# 三十四、README

最终必须提供完整 README。

至少包含：

```text
项目介绍
技术栈
目录结构
依赖
构建方法
数据库初始化
Migration
运行方式
前端启动
测试方法
配置说明
API 简介
设计文档位置
```

确保一个新的开发者拿到仓库后，可以根据 README 启动项目。

---

# 三十五、最终目标

最终交付：

> **财迹（wealth-trace）——可实际运行的家庭资产管理系统 V1**

至少满足：

```text
后端可编译
Meson 可正常工作
SQLite 可自动初始化
Migration 正常
REST API 可用
前端可运行
数据可持久化
核心财务逻辑有测试
README 完整
代码结构清楚
```

实现过程中始终遵循优先级：

> **正确性 > 简单性 > 可维护性 > 扩展性 > 炫技。**

不要为了未来可能永远不会出现的需求，提前增加 V1 的复杂度。