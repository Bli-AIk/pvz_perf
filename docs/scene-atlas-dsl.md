# 场景图集 DSL

`scene_atlas.py` 离线解析场景图片和局部 view，生成 atlas PNG 与 C++ 静态 UV 区域。
游戏只持有最终 GPU 指针和 `TextureRegion`，不解析 DSL，不读取场景行为 compiled，
不通过资源 SymbolId 查图。

当前作者文件位于 `assets/definitions/ui/`：Magnet、Almanac、Achievements、
DesktopUI 和 LandscapeUI 各有 `.atlas.dsl`。CommonUI 的共用按钮、对话框和蓝色选择框
由 `tools/scene_atlas.py` 的显式源表描述。旧 FixedUI 已拆分，不是当前生成接口。

| 模块 | 用途 |
| --- | --- |
| Magnet、Almanac、Achievements | 磁铁物件、图鉴、成就 |
| CommonUI | 两种布局共用 |
| DesktopUI | 电脑布局专用 |
| LandscapeUI | 横板布局专用 |

view 数量与完整源 sheet 数量不同；数量以当前生成清单为准。两种专用图集均打包，
但运行时只加载启动时选定的一种，CommonUI 共同加载。

## 整张源图与局部矩形

原有 sheet 必须整张装箱。编译器不拆帧、不重排内部像素、不裁透明边、
不删除未被引用的区域、不旋转、不缩放，也不为 view 生成单独的源 PNG。
既有作者描述要求的 alpha 合成在装箱前完成；这里的像素保持以原加载语义得到的
完整逻辑 RGBA 为准。

同一 sheet 在一个装箱单元中只放一次。不同 view 可以重叠、引用相同区域，
或覆盖不同区域；它们只增加静态采样区域，不增加源 sheet 的副本。
原局部矩形 `[x,y,w,h]` 先在离线装箱坐标中变为：

```text
[sheet_origin_x + x, sheet_origin_y + y, w, h]
```

生成 C++ 时再使用已知图集宽高，编译期换算成
`{u0,v0,u1,v1,w,h}`。前四项是 float 归一化采样边界，后两项是 int
逻辑绘制宽高，不是图集宽高。运行期不再对这份静态数据按图集尺寸做除法。
局部裁剪在已有 UV 区域内插值，目标矩形与屏幕 clip 仍使用像素单位。

宽高和局部像素排列保持不变。两像素扩边位于完整 sheet 的外侧，
不会插入原 sheet 内部的帧之间。Pogo 的三种状态仍共享整张 132×86 sheet，
成就二十个图标仍共享整张 512×256 sheet，未用的图标尾部也保留。

## 基本语法

每个文件只声明一个 `export atlas`，模块名与编译器支持的模块对应，例如 `Magnet`、`Almanac`、
`Achievements`、`DesktopUI` 或 `LandscapeUI`。模块之间不能重复声明，同一模块内的 sprite 名称必须唯一。
字段以分号结束；支持 `//`、`#` 和 `/* ... */` 注释。

```text
export atlas Magnet {
    sprite ZOMBIEPOGO_1 {
        image: "IMAGE_ZOMBIEPOGO";
        rect: [0, 0, 44, 86];
        scale: 0.8;
    }
    sprite ZOMBIEPOGO_2 {
        image: "IMAGE_ZOMBIEPOGO";
        rect: [44, 0, 44, 86];
        scale: 0.8;
    }
}
```

该片段展示语法；修改实际文件时保留该模块的其他 sprite，不创建第二份同名模块。
sprite 名称成为模块内 `Sprite` 枚举名；如果写入 `IMAGE_` 前缀，生成器会去掉它。
它不是跨模块共享的全局资源编号。

| 字段 | 要求 | 默认值 |
| --- | --- | --- |
| `image` | 必填字符串，直接图片路径或既有作者 `IMAGE_*` 标签 | 无 |
| `rect` | 四个整数 `[x,y,width,height]`；非负原点、正宽高，必须在完整源 sheet 内 | 整张 sheet |
| `scale` | Magnet 支持有限正数，表示磁铁物件的绘制缩放，不改变 atlas 像素；UI 模块当前只接受 `1.0`，布局仍由其现有目标矩形控制 | `1.0` |

重复字段、未知字段、不安全路径、未知作者标签、空模块、越界矩形、
缺失图片和无效缩放会导致离线构建失败。没有自动透明边裁切或隐式旋转选项。

## 直接添加新图片

新图片不需要先注册到 `resources.json`，也不需要新增 `Resources.h/.cpp`
中的 `Image*`。例如，把完整源文件放在
`assets/runtime/images/new_magnet_item.png`，然后在现有 Magnet 模块中添加：

```text
sprite NEW_MAGNET_ITEM {
    image: "images/new_magnet_item.png";
    scale: 0.8;
}
```

这里的路径是资源虚拟路径，不包含 `assets/runtime/` 前缀。
省略 `rect` 表示完整原图；如果新文件本身是 sheet，可用多个 sprite 的局部
`rect` 引用它，仍只装箱整张原 sheet 一次。生成后业务可使用
`MagnetAtlas::Sprite::NEW_MAGNET_ITEM` 对应的静态 `TextureRegion`。
新增业务种类是否需要行为代码取决于调用方，DSL 本身只描述图片几何和缩放。

直接路径会尝试恢复匹配的旧作者图片解码与 alpha 策略；没有旧作者项时沿用
自动 alpha 查找规则。若同一路径匹配到互相冲突的旧解码配方，构建会拒绝，
应改用明确的既有作者标签消除歧义。
`image: "IMAGE_ZOMBIEPOGO";` 等旧标签仅为离线作者 alias：
它们可在对应的运行时 `Image*` 声明删除后继续使用，不是 runtime SymbolId。
源文件及原作者标签不会因普通全局绑定退役而删除。

## 成就图标顺序

成就图标使用 `ICON_0`、`ICON_1`……命名。生成器按数字后缀建立独立的
`AchievementsAtlas::Icons[IconCount]` UV 区域数组，校验编号唯一且从零连续，
拒绝 gap。DSL 的声明可以乱序，不能依靠 `ICON_0` 的枚举值加成就 ID 访问图片。
普通场景图片继续位于 `Images[Count]` 中；`Icons` 明确承担成就 ID 到采样区域的映射。
调用方用 `static_assert` 将 `IconCount` 与业务的 `MAX_ACHIEVEMENTS` 对齐。

## 配置与生成输出

资源配置的现行选择方式是：

```text
sources(scene "ui/*.atlas.dsl")
atlas(magnet_items scene:Magnet)
atlas(almanac_ui scene:Almanac)
atlas(achievements_ui scene:Achievements)
atlas(common_ui scene:CommonUI)
atlas(desktop_ui scene:DesktopUI)
atlas(landscape_ui scene:LandscapeUI)
```

CommonUI 使用生成器中的显式源表，不新增作者文件。
这些单元使用公共原生初始布局、精确求解器、几何缓存、禁止旋转和两像素扩边；
当前每单元预算 60 秒。只有证明完成才能称为最优；限时可行结果必须保留
真实状态、下界和 gap。缓存复用不会把可行解改标成最优解。
已经完成的旧图集搜索缓存继续保留，新增场景单元不要求清空它们。

`SceneAtlasData.h/.cpp` 在 `Sexy.TodLib/generated/` 下生成，提供
`MagnetAtlas`、`AlmanacAtlas`、`AchievementsAtlas`、`CommonUIAtlas`、
`DesktopUIAtlas`、`LandscapeUIAtlas` 六个全局 namespace：

- 每模块一个 `Sexy::GpuTexture* Texture`，不再公开图集 `Width`、`Height`。
- 模块内 `Sprite`、`Count` 和 `extern const Sexy::TextureRegion Images[Count]`。
- 只有包含非 1 缩放的模块生成 `Scales`，当前为 Magnet。
- Achievements 另外生成按数字顺序排列的 `IconCount` 和 `Icons`。
- `Load()` 通过 `DefinitionBindAtlasPath` 绑定最终指针；`Free()` 解绑。
  `LoadSceneAtlases(bool landscapeUI)` 加载公共模块和选定布局；
  `FreeSceneAtlases()` 逆序清理，未加载模块的空指针也可安全清理。

`TextureSource` 仅借用 GPU 指针和采样 flags，不保存图集尺寸。
生成的 loader 仍传入预期 PNG 宽高字面量以验证资源，没有新增常驻采样尺寸。
GPU 对象的公共所有权和释放边界沿用现有加载器；view 不创建 Image 包装、
独立 GPU 纹理或运行时查询表。`SceneAtlasBindings.h` 仅供离线验证，
记录 atlas 尺寸、整张源 sheet 的区域和 RGBA SHA256，不由游戏包含。
验证器逐张检查装入 atlas 的完整 sheet 像素；原 sheet 的未引用区域同样参与摘要。
离线装箱与像素复制继续使用整数坐标；UV 存储不改变 PNG、PAK 或存档布局。
碰撞框、目标布局和浮点绘制偏移不在本次变更范围。

本页记录作者接口和已经实现的生成结构，不预记最终图集尺寸、包摘要、
构建验证结论、显存节省或 FPS 收益。
