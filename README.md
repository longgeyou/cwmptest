# cwmptest
CPE WAN Management Protocol，测试

### 1、文档来源

由Broadband Forum组织在2004年制定；[协议相关文档](https://wiki.broadband-forum.org/display/RESOURCES)，搜索NUMBER = TR-069。

### 2、协议内容

参考文档《tr-069-1-0-0.pdf》

#### 2.1 介绍

```c
TR-069 describes the CPE WAN Management Protocol, intended for communication between a CPE and Auto-Configuration Server (ACS). The CPE WAN Management Protocol defines a mechanism that encompasses secure auto-configuration of a CPE, and also incorporates other CPE management functions into a common framework.
```

```c
This document specifies the generic requirements of the management protocol methods which can be applied to any TR-069 CPE. Other documents specify the managed objects, or data models, for specific types of devices or services.
```

#### 2.2 模型结构

|      序协议栈      |
| :----------------: |
|  cpe/acs应用程序   |
| 远程过程调用 (RPC) |
|        soap        |
|     http 协议      |
|      SSL/TLS       |
|       TCP/IP       |

#### 2.3 附录

- **A** RPC方法：远程调用
- **B** CPE参数：大量参数，预订功能
- **C** 签名凭证：安全相关
- **D** WEB身份管理：安全相关
- **E** 签名包格式：安全相关
