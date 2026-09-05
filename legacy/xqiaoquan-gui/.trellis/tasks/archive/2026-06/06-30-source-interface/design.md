# M8b-1 Source 接口设计

> 依据:理解阶段消费者依赖矩阵(5 子系统全量调研)。本设计回答"消费者依赖什么",定义只读访问契约 + Resident 实现 + Handle 适配器,纯增量、不动现有消费者与 handle。

## 0. 设计定调(一句话)

**Source 接口的本质 = 「每块一次虚调用,返回整块连续只读视图」。** 虚边界只落在 `acquire_*`(每次一次虚派发);视图内部访问全部 non-virtual inline;能借用底层 vector 的零拷贝借用,不能借用的(region/slab/faceId)在 acquire 内一次性物化。逐元素虚调用零容忍。

## 1. 模块位置与依赖

- 落点:`XQ/src/core/source/`(最底层 core,零外部依赖)。被 services / io / visualization / adapters 共同消费,故必须在依赖链最底端。
- C++17:自定义 `ReadSpan<T>`,不用 C++20 `std::span`。
- 纯 XQ 值类型:复用 `core/GeometryTypes.h` 的 `Point3`、`core/XQImageVolume.h` 的 `ScalarType`、handle 的 `Triangle`/`Tet` typedef。

## 2. 类型设计

### 2.1 `ReadSpan<T>`(只读连续视图)— `core/source/ReadSpan.h`

```cpp
template <class T>
class ReadSpan {
public:
    ReadSpan() = default;
    ReadSpan(const T* data, std::size_t size) : data_(data), size_(size) {}

    const T* data() const { return data_; }      // 连续内存指针(AC10 直喂外部库)
    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    const T& operator[](std::size_t i) const { return data_[i]; } // inline,非虚
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }

private:
    const T* data_ = nullptr;
    std::size_t size_ = 0;
};
```

只读(无非 const 访问)。满足 AC9。`data()` 暴露连续指针 → `acquire_points` 的 `ReadSpan<Point3>` 可 `reinterpret_cast<const double*>(data())` 得 `3*n` 连续 double 直喂 TetGen(`Point3`=3 个无填充 double;`Triangle`=`array<int,3>`、`Tet`=`array<int,4>` 同为无填充 POD,reinterpret 到扁平 int 数组合法)。AC10 由测试断言 reinterpret 等价证明。

### 2.2 视图载荷

**`VoxelView`**(`acquire_*` 返回的体素块视图):
```cpp
struct VoxelView {
    bool valid = false;
    ScalarType type = ScalarType::Unknown;
    int dims[3] = {0, 0, 0};      // 本块(whole=整卷;region=子块;slab=dimX×dimY×1)维度
    int components = 1;
    ReadSpan<std::uint8_t> bytes; // x-fastest, 分量交错(与 handle 同布局)

    std::size_t voxelCount() const;                       // dims 连乘,inline
    std::size_t voxelIndex(int x, int y, int z) const;    // x + dimX*(y + dimY*z),inline
    double scalarAt(std::size_t voxelIndex, int comp = 0) const; // inline 解码,非虚
};
```
`scalarAt` 内联按 `type` 解码(复用 `XQMemoryImageBufferHandle::scalarSize` 的同一套规则),不引入逐元素虚调用。

**`TriangleView`**(三角 + 并行 faceId 一次给齐,对应理解阶段"acquire_triangles 同时暴露 faceId"):
```cpp
struct TriangleView {
    ReadSpan<Triangle> triangles; // Triangle = std::array<int,3>
    ReadSpan<int> faceIds;        // 与 triangles 等长、逐元素对应(AC2)
};
```

### 2.3 租约(RAII 借用句柄)— `core/source/ReadLease.h`

租约持有底层 `shared_ptr` keepalive + 可选物化缓冲 + 视图。**move-only**,析构释放;租约存活期间视图指针稳定有效(AC8)。

```cpp
class VoxelLease {
public:
    const VoxelView& view() const { return view_; }
    VoxelLease(VoxelLease&&) = default;
    VoxelLease& operator=(VoxelLease&&) = default;
    VoxelLease(const VoxelLease&) = delete;
    // 工厂:借用(whole)或物化(region/slab)
    static VoxelLease borrow(std::shared_ptr<const void> keepalive, VoxelView view);
    static VoxelLease own(std::vector<std::uint8_t> bytes, ScalarType, const int dims[3], int components);
private:
    std::shared_ptr<const void> keepalive_; // whole 借用时持 handle 存活
    std::vector<std::uint8_t> owned_;        // region/slab 物化块;借用时为空
    VoxelView view_;                         // 指向 keepalive 的底层 或 owned_
};

template <class T>
class GeometryLease {           // 用于 points / tetrahedra(可借用)
public:
    const ReadSpan<T>& span() const { return view_; }
    // move-only;borrow / own 两个工厂同上
private:
    std::shared_ptr<const void> keepalive_;
    std::vector<T> owned_;
    ReadSpan<T> view_;
};

class TriangleLease {           // 三角:triangles 借用 + faceIds 物化
public:
    const TriangleView& view() const { return view_; }
    // move-only
private:
    std::shared_ptr<const void> keepalive_;
    std::vector<int> ownedFaceIds_; // handle 无 faceId vector 访问器 → 物化(见 §4.2)
    TriangleView view_;
};
```

**借用 vs 物化决策**(关键设计点):
| 数据 | 底层是否有连续 vector 访问器 | 策略 |
|---|---|---|
| voxel whole | `bytes()` 有 | **借用**(零拷贝) |
| voxel region/slab | 子区域非连续 | **物化**(acquire 内一次性拷出 x-fastest 子块) |
| points | `points()` 有 | **借用** |
| triangles | `triangles()` 有 | **借用** |
| faceIds | **无 vector 访问器**(只有 `triangleFaceId(i)`) | **物化**(acquire 内一次 O(N) 拷贝) |
| tetrahedra | `tets()` 有 | **借用** |

物化只发生在 acquire 内部、一次性、非虚——不违反"逐元素虚调用零容忍"与 AC7(acquire 次数 = entity 数)。

### 2.4 元数据(免物化)

```cpp
struct VoxelMeta { bool valid=false; ScalarType type=ScalarType::Unknown;
                   int dims[3]={0,0,0}; int components=1; std::size_t voxelCount=0; };
struct GeometryMeta { bool valid=false; std::size_t pointCount=0,
                      triangleCount=0, tetCount=0; };
```
从 handle 的 count/dim 访问器直接读,不触碰数据缓冲(AC6)。

## 3. 接口

```cpp
// core/source/IVoxelSource.h
class IVoxelSource {
public:
    virtual ~IVoxelSource() = default;
    virtual VoxelMeta meta() const = 0;                          // 免物化
    virtual VoxelLease acquire_whole() const = 0;                // 整卷,借用
    virtual VoxelLease acquire_region(const int extent[6]) const = 0; // {x0,x1,y0,y1,z0,z1} inclusive
    virtual VoxelLease acquire_slab(int z) const = 0;            // 第 z 个 xy 平面
};

// core/source/IGeometrySource.h
class IGeometrySource {
public:
    virtual ~IGeometrySource() = default;
    virtual GeometryMeta meta() const = 0;                       // 免物化
    virtual GeometryLease<Point3> acquire_points() const = 0;
    virtual TriangleLease acquire_triangles() const = 0;         // triangles + 并行 faceId
    virtual GeometryLease<Tet> acquire_tetrahedra() const = 0;
};
```

一个 source 实例可能只持有几何的一部分(surface 无 tet;tet 无 triangle):`meta()` 对应 count 报 0,`acquire_*` 对缺失种类返回空 lease(`span().empty()`)。**不是错误**,由调用方按 meta 决定取哪些。

错误语义(越界 region/slab):返回 `valid=false` 的 `VoxelView`,不抛异常(沿用 core "explicit result, no exceptions through UI")。AC4/AC5 断言越界报错。

## 4. Resident 实现 + Handle 适配器

适配器持 `shared_ptr<const Handle>`,lease 的 keepalive 复制该 shared_ptr → 借用 span 在 lease 生命周期内安全,即便适配器先析构(AC8)。handle 在适配器内 const、无 mutation API 调用 → 租约期内底层稳定不 realloc。

### 4.1 `ResidentVoxelSource : IVoxelSource`(`core/source/ResidentVoxelSource.{h,cpp}`)
包 `shared_ptr<const XQMemoryImageBufferHandle>`。
- `meta()`:读 handle 的 type/dimX/Y/Z/components/voxelCount。
- `acquire_whole()`:`VoxelView{bytes()=借用, dims, type, components}`,keepalive=handle。
- `acquire_region(extent)`:校验 inclusive extent ⊂ [0,dim);物化子块(三重循环按目标 x-fastest memcpy 每体素 `components*scalarSize` 字节);越界→空 invalid lease。
- `acquire_slab(z)`:`acquire_region({0,dimX-1,0,dimY-1,z,z})` 的特化;越界→空。

### 4.2 `ResidentSurfaceSource : IGeometrySource`(`core/source/ResidentSurfaceSource.{h,cpp}`)
包 `shared_ptr<const XQTriangleSurfaceGeometryHandle>`。
- `meta()`:pointCount/triangleCount 实数,tetCount=0。
- `acquire_points()`:借用 `points()`。
- `acquire_triangles()`:triangles 借用 `triangles()`;faceIds **物化**(`ownedFaceIds_[i]=h->triangleFaceId(i)`,一次 O(N)),`TriangleView{triangles=借用, faceIds=指向 ownedFaceIds_}`。
- `acquire_tetrahedra()`:空 lease。

### 4.3 `ResidentTetSource : IGeometrySource`(`core/source/ResidentTetSource.{h,cpp}`)
包 `shared_ptr<const XQTetVolumeMeshHandle>`。
- `meta()`:pointCount/tetCount 实数,triangleCount=0。
- `acquire_points()`:借用 `points()`。
- `acquire_triangles()`:空 lease(TriangleView 两 span 皆空)。
- `acquire_tetrahedra()`:借用 `tets()`。

## 5. 不做(防止越界)

- 不改三种 handle 的任何 API(faceId 走物化而非加访问器,严格纯增量)。
- 不迁移任何现有消费者到 Source(后续 M8b-2+)。
- 不引入懒加载/卸载/磁盘驻留(驻留行为零改动)。
- 不定义 mutate / 写回通道(消费侧全只读;批量构造入口留待后续)。

## 6. 验证策略

CMake 内新建 `xq_core_source_tests`(或挂入既有 core 测试),Release 全量 ctest。每条 AC 对应独立断言(见 implement.md)。假绿抽查:篡改 region 偏移 / faceId 物化下标 → 必须 FAIL,恢复 PASS,exe 时间戳真变(memory ninja-target 假绿坑)。`reader::read` 类副作用不进 assert(memory no-sideeffect-in-assert)。
