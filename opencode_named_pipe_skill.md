## 注意事项：
- opencode 的 mcp 工具默认最大允许等待超时为 60s 左右（似乎可以通过 experimental.mcp_timeout 配置），加上 mcp 服务启动时间，建议设置 wait_milliseconds 小于等于 55 秒内
- 在 opencode 中设置 wait_milliseconds=0 同样返回错误，断其设计可能存在超时下限（也可能需要时间创建终端启动 mcp 等可能问题），建议给 2-3 秒等待 opencode 下限时间（命名管道工具本身代码测试等待超时 0 值可用）
