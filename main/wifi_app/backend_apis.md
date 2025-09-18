# 后端接口文档
## 接口列表
### 1. 连接WiFi接口
- **路径**: `/connect-wifi`
- **方法**: POST
- **参数**:
  - ssid: 要连接的WiFi名称（字符串）
  - password: WiFi密码（字符串）
- **返回值**:
  - 成功: { "status": "success", "message": "连接成功" }
  - 失败: { "status": "error", "message": "连接失败，原因：..." }

### 2. 获取扫描WiFi列表接口
- **路径**: `/scan-wifi`
- **方法**: GET
- **返回值**:
  - 成功: { "status": "success", "data": ["SSID1", "SSID2", ...] }
  - 失败: { "status": "error", "message": "扫描失败" }

### 3. 获取当前IP接口
- **路径**: `/get-ip`
- **方法**: GET
- **返回值**:
  - 成功: { "status": "success", "ip": "xxx.xxx.xxx.xxx" }
  - 失败: { "status": "error", "message": "获取IP失败" }