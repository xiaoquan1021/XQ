// R4 字体根治验证(glyph 可用性,非离屏直方图)。历史教训
// (memory harness-cannot-read-local-images):本环境看不到本地图,tofu 不能靠抓图判;
// 这里用 QRawFont::glyphIndexesForChars 对汉字断言 glyphIndex != 0(0 = 无字形 = tofu)。
// 复现 main.cpp::installUiFont 的加载路径,确认装出的 UI 字体能渲染汉字与字母。

#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QRawFont>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cstdio>

namespace {

int g_failures = 0;
void check(bool ok, const char* what)
{
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

// Mirror of main.cpp::installUiFont: register msyh.ttc explicitly and use the
// family it yields, falling back to the by-name family.
QFont resolveUiFont()
{
    QString family = QStringLiteral("Microsoft YaHei");
    const int id = QFontDatabase::addApplicationFont(
        QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
    if (id >= 0) {
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        if (!families.isEmpty()) {
            family = families.first();
        }
    }
    return QFont(family, 9);
}

bool glyphAvailable(const QRawFont& rawFont, QChar ch)
{
    if (!rawFont.isValid()) {
        return false;
    }
    const QVector<quint32> idx = rawFont.glyphIndexesForString(QString(ch));
    return idx.size() == 1 && idx[0] != 0; // 0 == .notdef == tofu
}

} // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);

    const QFont uiFont = resolveUiFont();
    const QRawFont rawFont = QRawFont::fromFont(uiFont);
    check(rawFont.isValid(), "UI font resolves to a valid QRawFont");

    // Han glyphs must render (the tofu bug): 轴 (U+8F74), 中 (U+4E2D), 文 (U+6587).
    check(glyphAvailable(rawFont, QChar(0x8F74)), "Han glyph 轴 has a real glyph (not tofu)");
    check(glyphAvailable(rawFont, QChar(0x4E2D)), "Han glyph 中 has a real glyph");
    check(glyphAvailable(rawFont, QChar(0x6587)), "Han glyph 文 has a real glyph");

    // Latin must still render (English stays correct).
    check(glyphAvailable(rawFont, QChar('A')), "Latin glyph A has a real glyph");
    check(glyphAvailable(rawFont, QChar('x')), "Latin glyph x has a real glyph");

    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("all UI font glyph checks passed\n");
    return 0;
}
