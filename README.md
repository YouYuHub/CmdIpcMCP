## CmdIpcMCP
CmdIpcMCP 目前是一个基于 Windows 命名管道的 MCP 工具。工具文档说明在 mcp_tools.cpp 中

## 安装
### 目前只支持 Windows
### VsCode / Vs 以 code 为例
1. 打开 VSCode。
2. 点击 Copilot 聊天图标。
3. 点击配置工具。
4. 点击添加 MCP 服务器（右上角）；配置大致如下：
```json
{
  "servers": {
    "pipeIpcMcp": {
	  "type": "stdio",
	  "command": "path\\to\\PipeIpcMCP.exe",
	  "args": [],
	  "version": "1.0.0"
	}
  },
  "inputs": []
}
```
- VsCode / Vs 仅测试，更推荐使用官方工具


### OpenCode
1. 找到 opencode 的 mcp 配置文件打开，通常是：
```bash
C:\Users\your—user-name\.config\opencode\opencode.jsonc
```
2. 添加如下配置：
```json
{
  "$schema": "https://opencode.ai/config.json",
  "mcp": {
    "pipeCmdMCP": {
      "type": "local",
      // Or ["bun", "x", "my-mcp-command"]
      "command": [
        "C:\\path\\to\\PipeIpcMCP.exe"
      ],
      "enabled": true,
      "environment": {}
    }
  },
  "tools": {
    "pipeCmdMCP": true
  }
}
```
- OpenCode 官方工具不支持交互命令，本工具补全这点遗憾


### ChatGPT
1. 打开 ChatGPT。
2. 点击左侧的插件，找到 MCP 选项卡，添加：
名称随意，启动命令填写：path\\to\\PipeIpcMCP.exe
- 适用于交互式、长周期连续运行等命令等执行场景


#### 其他所有支持标准 mcp 协议的编程/agent程序都可以使用