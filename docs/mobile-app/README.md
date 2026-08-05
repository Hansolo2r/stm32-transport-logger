# 手机端 BLE 界面

这是 STM32 温度与运动记录器的手机控制界面，通过 JDY-16 的 BLE 透传服务读取状态和 Flash 记录。

## 使用方式

1. 使用 HTTPS 地址打开本页面。iPhone Safari 不提供 Web Bluetooth，需要在 Bluefy 中打开。
2. 点击“连接设备”，选择 JDY-16。
3. 首页正常工作时只显示“设备在线”和最新状态，不显示周期性命令日志。
4. 首页显示 DS1302 当前设备时间。点击“日志数量”在线读取最近 100 条记录，并按真实时间查看温度曲线和表格。
5. 点击“运动次数”或“碰撞次数”查看独立事件列表。
6. “暂停记录”停止新增日志和事件计数，再次点击可继续；实时温度和XYZ仍然更新。
7. “设备休眠”关闭OLED并暂停采集，但保持BLE连接；按钮变为“唤醒设备”，点击即可恢复。
8. “更多 > 通信诊断”仅用于查看异常或发送手动命令。
9. “更多 > 设备参数”可读取和调整碰撞、运动、静止确认及记录周期；点击“保存到设备”后，参数写入W25Q64，断电后仍然有效。

记录预览、运动明细和碰撞明细均按时间正序显示，最早记录在上方。新记录使用 DS1302 的真实日期时间；升级固件前已经存在的旧记录没有历史日期，会回退显示会话内相对时间。

## 通信配置

- BLE Service：`FFE0`
- BLE Characteristic：`FFE1`
- STM32 串口：USART2，PA2/PA3，9600 baud，8N1
- 状态轮询：网页每 3 秒静默发送一次 `status_json`

## 固件协议

- `@TEMP,<0.1摄氏度>,<YYYY-MM-DD HH:MM:SS或NA>`：实时温度和设备时间
- `@EVENT,<类型>,<累计次数>,<YYYY-MM-DD HH:MM:SS或NA>`：实时运动或碰撞通知
- `@STATUS,<温度>,<运动次数>,<碰撞次数>,<记录数>,<可读>,<可写>,<运动中>,<X>,<Y>,<Z>,<暂停>,<YYYY-MM-DD HH:MM:SS或NA>`：完整状态
- `preview`：返回最近 100 条非启动记录，使用 `PREVIEW_BEGIN` / `PREVIEW_END` 分帧
- `events`：返回最近 200 条运动和碰撞记录，使用 `EVENTS_BEGIN` / `EVENTS_END` 分帧
- `pause` / `resume`：暂停或继续新增日志与事件计数
- `sleep`：关闭OLED、暂停SysTick并进入可由USART2接收中断唤醒的Sleep模式
- `wake`：通过JDY-16发送串口数据，唤醒STM32并恢复OLED、采集和状态轮询
- `cfg get`：读取当前检测参数
- `cfg shock <mg>`：设置碰撞合加速度阈值，范围1100至8000 mg
- `cfg motion <mg>`：设置相对1g的运动偏差阈值，范围50至800 mg
- `cfg mconf <ms>` / `cfg sconf <ms>`：设置运动开始和静止确认时间
- `cfg cool <ms>`：设置两次碰撞记录之间的最小间隔
- `cfg temp <s>`：设置温度写入Flash的周期
- `cfg save`：把当前参数保存到W25Q64；`cfg defaults`只把运行参数恢复为默认值，仍需执行保存
- `export`：下载完整简化 CSV，列为 `session,timestamp,time_s,temp_c,motion,shock`，使用 `CSV_BEGIN` / `CSV_END` 分帧

“设备休眠”不是物理断电。JDY-16等直接接在电源上的模块仍会耗电；真正切断电池需要额外的电源锁存或负载开关电路。

设备使用 DS1302 RTC 提供真实日期时间。Sleep 期间 SysTick 暂停，所以相对时间 `time_s` 不增长，但 DS1302 继续走时；唤醒后新记录仍会取得正确的真实时间。

日志记录仍为固定 32 字节。原先保留的第 24 至 29 字节现在存储年、月、日、时、分、秒，CRC 继续覆盖这些字节，因此容量仍为 2048 条。旧记录中的六个字节为零，会被识别为 `NA`，无需清空 Flash。

参数保存在W25Q64的`0x7FE000`扇区；日志使用`0x000000`至`0x00FFFF`，掉电测试标记使用`0x7FF000`，三个区域互不覆盖。配置包含版本标记、范围校验和CRC；配置损坏或首次使用时会回退到默认参数。

休眠时按Reset会让STM32重新启动。固件启动完成后主动发送`@AWAKE`；网页收到`@AWAKE`、`@TEMP`、`@STATUS`或`@EVENT`中的任意一种都会自动清除旧的休眠状态并恢复轮询。

## 部署

仓库中的 `.github/workflows/pages.yml` 会把本目录部署到 GitHub Pages。仓库启用 Pages 的 GitHub Actions 发布源后，每次推送到 `main` 都会自动更新 HTTPS 页面。
