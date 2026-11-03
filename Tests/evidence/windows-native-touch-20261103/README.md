# Windows 原生触控入口

Win10／Vulkan Release 使用 WM_POINTER，批量获取同窗口输入帧并跳过同帧剩余消息；Win7／OpenGL Release 注册 WM_TOUCH，批量读取输入记录并关闭触控句柄。API读取使用固定32项栈缓冲区，游戏层仍支持十个触点。侧键模拟方法、状态字段与转换分支已删除。

输入坐标使用现有窗口 PresentRect 转换。读取原生记录后走延迟消息边界，控件回调不在 Pointer／Touch 消息读取期间执行。过滤触控合成鼠标消息，并隔离触控后可能被系统移动的 GetCursorPos 轮询结果；真实鼠标移动使用消息坐标恢复。鼠标捕获丢失不取消独立触点。

两后端实际生产对象／PAK回归通过。隐藏窗口夹具向原生入口提交两个已解码触点记录，验证延迟分发、稳定ID、独立抬起、移动不改鼠标坐标、取消及侧键不再产生触控；继续执行共享选择、炮手势、花园、图集、菜单和保存回归。Vulkan四角框对照差值0，OpenGL最大1个RGBA8色阶。

Win7兼容性导入检查通过；独立查看导入表确认Win7仅使用RegisterTouchWindow／GetTouchInputInfo／CloseTouchInputHandle，Win10使用GetPointerFrameTouchInfo／SkipPointerFrameMessages／GetPointerType。当前系统报告digitizer=197、最大20触点，这不等于已验证真实驱动消息和手感；两条系统消息读取路径尚需真实触屏验收。Win7程序在当前Windows主机上验证，并非在Win7系统上实测。

日志和文件哈希见本目录。DAT64维持v4，没有更改关卡存档结构；两个配套性能布局工具已重建。未提交Git、未更新Android包。
