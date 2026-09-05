// test_itk_vascular_segmenter —— ITK 血管两阶段水平集 adapter 测试(无框架 CHECK+main)。
//
// 批2 合成 gate:验证在会击垮自研 Chan-Vese 的图上,SV 血管水平集仍输出「贴合、
// 有界」的闭环(不是脱离内容的大圆、不是空):
//   ① 清晰 disk:sanity,环贴 disk 边界
//   ② 模糊低对比 ramp:边界弱化仍有界贴合
//   ③ c1≈c2 强 ramp(真机失败模式):内外均值几乎相等,区域型崩,GAC 靠梯度仍收敛
//   ④ 确定性:同输入两次跑结果一致
#include "adapters/itk/ItkVascularSegmenter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int g_failures = 0;
#define CHECK(cond, msg)                                                        \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::printf("FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__);       \
            ++g_failures;                                                       \
        }                                                                       \
    } while (0)

namespace {

// 造 W×H 灰度:圆心 (cx,cy) 半径 r 的 disk 内 fg、外 bg,叠加水平 ramp(rampAmp
// 越大内外均值越接近 -> 区域型模型失效)。row-major idx = y*W + x。
std::vector<double> makeDisk(int W, int H, double cx, double cy, double r,
                             double fg, double bg, double rampAmp)
{
    std::vector<double> g(static_cast<std::size_t>(W) * H, 0.0);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const double dx = x - cx, dy = y - cy;
            const double d = std::sqrt(dx * dx + dy * dy);
            const double base = (d <= r) ? fg : bg;
            const double ramp = rampAmp * static_cast<double>(x) / W;
            g[static_cast<std::size_t>(y) * W + x] = base + ramp;
        }
    }
    return g;
}

double loopArea(const std::vector<xq::SectionPoint2D>& loop)
{
    double a = 0.0;
    const std::size_t n = loop.size();
    for (std::size_t i = 0; i < n; ++i) {
        const auto& p = loop[i];
        const auto& q = loop[(i + 1) % n];
        a += p.u * q.v - q.u * p.v;
    }
    return std::abs(0.5 * a);
}

// 环的包围盒最大半径(相对中心),用于判「有没有溢出成大圆/整幅」。
double loopMaxRadius(const std::vector<xq::SectionPoint2D>& loop)
{
    double mx = 0.0;
    for (const auto& p : loop) {
        mx = std::max(mx, std::hypot(p.u, p.v));
    }
    return mx;
}

bool pointInLoop(const std::vector<xq::SectionPoint2D>& loop, double u, double v)
{
    bool inside = false;
    const std::size_t n = loop.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const auto& a = loop[i];
        const auto& b = loop[j];
        const bool straddles = (a.v > v) != (b.v > v);
        if (straddles) {
            const double xCross = (b.u - a.u) * (v - a.v) / (b.v - a.v) + a.u;
            if (u < xCross) inside = !inside;
        }
    }
    return inside;
}

// 对一张 disk 图跑 segment,断言:非空、闭环(>=8点)、包住种子、有界(半径不超
// disk 半径太多 = 没溢出)、面积落在 disk 真面积的合理带内(贴合而非塌缩/膨胀)。
void gateFittedLoop(const char* name, int W, int H, double diskRpx, double rampAmp,
                    double fg, double bg)
{
    const double cx = W / 2.0, cy = H / 2.0;
    const double pxMm = 1.0;   // 1 像素 = 1 mm,便于换算
    auto gray = makeDisk(W, H, cx, cy, diskRpx, fg, bg, rampAmp);

    xq::ItkVascularSegmenter seg;
    xq::LevelSetParams params;   // SV 默认(prop=0/adv=0/curv=1)
    xq::SectionPoint2D seed{0.0, 0.0};   // 断面中心 = disk 中心

    auto loop = seg.segment(gray, W, H, pxMm, seed, params);

    std::printf("  [%s] pts=%zu area=%.1f maxR=%.1f (diskR=%.1f)\n",
                name, loop.size(), loop.empty() ? 0.0 : loopArea(loop),
                loop.empty() ? 0.0 : loopMaxRadius(loop), diskRpx);

    CHECK(!loop.empty(), name);
    if (loop.empty()) return;

    CHECK(loop.size() >= 8, "loop is a real closed contour (>= 8 pts)");
    CHECK(pointInLoop(loop, seed.u, seed.v), "loop encloses the seed");

    // 有界:不溢出成大圆。最大半径不超 disk 半径的 1.6 倍(留边界模糊余量)。
    const double maxR = loopMaxRadius(loop);
    CHECK(maxR <= diskRpx * 1.6,
          "loop is bounded (does not balloon past the disk)");

    // 贴合:面积落在 disk 真面积的 [0.25, 2.5] 倍带内(既不塌成点也不膨成整幅)。
    const double diskArea = M_PI * diskRpx * diskRpx;
    const double area = loopArea(loop);
    CHECK(area >= diskArea * 0.25 && area <= diskArea * 2.5,
          "loop area fits the disk (not collapsed, not overflowed)");
}

} // namespace

int main() {
    std::printf("test_itk_vascular_segmenter: batch-2 synthetic gates\n");

    // ① 清晰 disk(fg 远高于 bg,无 ramp)
    gateFittedLoop("clear-disk", 64, 64, 16.0, 0.0, 1000.0, 0.0);

    // ② 模糊低对比:弱信号 + 中等 ramp
    gateFittedLoop("low-contrast", 64, 64, 16.0, 300.0, 500.0, 100.0);

    // ③ c1≈c2 强 ramp(真机失败模式):ramp 幅度接近 fg-bg,全局均值内外几乎相等
    gateFittedLoop("c1-approx-c2", 64, 64, 16.0, 900.0, 1000.0, 0.0);

    // ④ 确定性:同输入两次一致
    {
        auto gray = makeDisk(64, 64, 32.0, 32.0, 16.0, 1000.0, 0.0, 300.0);
        xq::ItkVascularSegmenter seg;
        xq::LevelSetParams params;
        xq::SectionPoint2D seed{0.0, 0.0};
        auto a = seg.segment(gray, 64, 64, 1.0, seed, params);
        auto b = seg.segment(gray, 64, 64, 1.0, seed, params);
        CHECK(a.size() == b.size() && !a.empty(), "deterministic: same point count");
        bool same = a.size() == b.size();
        for (std::size_t i = 0; same && i < a.size(); ++i) {
            if (std::abs(a[i].u - b[i].u) > 1e-9 || std::abs(a[i].v - b[i].v) > 1e-9) {
                same = false;
            }
        }
        CHECK(same, "deterministic: identical loops across two runs");
    }

    if (g_failures == 0) {
        std::printf("test_itk_vascular_segmenter: all checks passed\n");
        return 0;
    }
    std::printf("test_itk_vascular_segmenter: %d failure(s)\n", g_failures);
    return 1;
}
