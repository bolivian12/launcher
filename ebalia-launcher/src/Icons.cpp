#include "Icons.hpp"

#include <QImage>
#include <QHash>

static QPixmap makeIcon(const QStringList &rows, const QHash<QChar, QRgb> &pal, int size)
{
    QImage img(8, 8, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8 && x < rows[y].size(); ++x) {
            const QChar c = rows[y][x];
            if (c != QLatin1Char('.'))
                img.setPixel(x, y, pal.value(c, qRgb(255, 0, 255)));
        }
    return QPixmap::fromImage(img).scaled(size, size,
                                          Qt::IgnoreAspectRatio,
                                          Qt::FastTransformation);
}

namespace icons {

QPixmap home(int s)
{
    return makeIcon({
        "...11...", "..1111..", ".111111.", "11111111",
        ".11..11.", ".11..11.", ".111111.", "........",
    }, {{QLatin1Char('1'), qRgb(154, 154, 154)}}, s);
}

QPixmap allVersions(int s)
{
    return makeIcon({
        "BB..BB..", "BB..BB..", "........", "..BB..BB",
        "..BB..BB", "........", "BB..BB..", "BB..BB..",
    }, {{QLatin1Char('B'), qRgb(139, 139, 139)}}, s);
}

QPixmap alpha(int s)
{
    return makeIcon({
        "GGGGGGGG", "GgGGGGgG", "GGGGGGGG", "DDDDDDDD",
        "DdDDDDdD", "DDDDDDDD", "DDdDDDDd", "dDDDDdDD",
    }, {
        {QLatin1Char('G'), qRgb(106, 174, 63)},
        {QLatin1Char('g'), qRgb(92, 152, 52)},
        {QLatin1Char('D'), qRgb(122, 90, 52)},
        {QLatin1Char('d'), qRgb(104, 76, 42)},
    }, s);
}

QPixmap horror(int s)
{
    return makeIcon({
        "KKKKKKKK", "KKKKKKKK", "KWWKKWWK", "KWWKKWWK",
        "KKKKKKKK", "KKKKKKKK", "KKKKKKKK", "KKKKKKKK",
    }, {
        {QLatin1Char('K'), qRgb(30, 22, 40)},
        {QLatin1Char('W'), qRgb(245, 245, 245)},
    }, s);
}

QPixmap release(int s)
{
    return makeIcon({
        "...AAA...", "..AAAAA..", ".AAWAAAA.", "AAAAAAAA.",
        ".AAAAAAA.", "..AAAAA..", "...AAA...", ".........",
    }, {
        {QLatin1Char('A'), qRgb(85, 199, 217)},
        {QLatin1Char('W'), qRgb(255, 255, 255)},
    }, s);
}

QPixmap mods(int s)
{
    return makeIcon({
        "MMMMMMMM", "M......M", "M.MM.MM.", "M......M",
        "M.MM.MM.", "M......M", "MMMMMMMM", "........",
    }, {{QLatin1Char('M'), qRgb(176, 132, 48)}}, s);
}

QPixmap support(int s)
{
    return makeIcon({
        ".HH..HH.", "HHHHHHHH", "HHHHHHHH", "HHHHHHHH",
        ".HHHHHH.", "..HHHH..", "...HH...", "........",
    }, {{QLatin1Char('H'), qRgb(224, 90, 90)}}, s);
}

QPixmap news(int s)
{
    return makeIcon({
        "PPPPPPPP", "P......P", "P.PPPP.P", "P......P",
        "P.PPPP.P", "P......P", "PPPPPPPP", "........",
    }, {{QLatin1Char('P'), qRgb(208, 208, 208)}}, s);
}

QPixmap server(int s)
{
    return makeIcon({
        "SSSSSSSS", "S.SSS..S", "SSSSSSSS", "S.SSS..S",
        "SSSSSSSS", "S.SSS..S", "SSSSSSSS", "........",
    }, {
        {QLatin1Char('S'), qRgb(120, 160, 200)},
        {QLatin1Char('.'), qRgb(0, 0, 0)},
    }, s);
}

QPixmap learn(int s)
{
    return makeIcon({
        ".LL..LL.", "LLLLLLLL", "LLLLLLLL", "LLLLLLLL",
        "LLLLLLLL", ".LLLLLL.", "..LLLL..", "...LL...",
    }, {{QLatin1Char('L'), qRgb(230, 180, 60)}}, s);
}

QPixmap setup(int s)
{
    return makeIcon({
        "WW....WW", "WWW..WWW", ".WWWWWW.", "..WWWW..",
        "..WWWW..", ".WWWWWW.", "WWW..WWW", "WW....WW",
    }, {{QLatin1Char('W'), qRgb(160, 160, 160)}}, s);
}

QPixmap discord(int s)
{
    return makeIcon({
        "DDDDDDDD", "D......D", "D.W..W.D", "D.W..W.D",
        "D......D", "D.W..W.D", "DDDDDDDD", "........",
    }, {
        {QLatin1Char('D'), qRgb(88, 101, 242)},
        {QLatin1Char('W'), qRgb(255, 255, 255)},
    }, s);
}

QPixmap youtube(int s)
{
    return makeIcon({
        "YYYYYYYY", "Y......Y", "Y..T...Y", "Y..TT..Y",
        "Y..TTT.Y", "Y..TT..Y", "YYYYYYYY", "........",
    }, {
        {QLatin1Char('Y'), qRgb(255, 0, 0)},
        {QLatin1Char('T'), qRgb(255, 255, 255)},
    }, s);
}

QPixmap patreon(int s)
{
    return makeIcon({
        "PPPPPP..", "PPPPPPP.", "PP...PP.", "PPPPPPP.",
        "PPPPPP..", "PP......", "PP......", "PP......",
    }, {{QLatin1Char('P'), qRgb(255, 66, 77)}}, s);
}

QPixmap shop(int s)
{
    return makeIcon({
        "...TT...", "..TTTT..", ".TT.TT..", ".TTTT...",
        "..TTTTT.", "...TT.TT", "...TTTT.", "....TT..",
    }, {{QLatin1Char('T'), qRgb(255, 170, 0)}}, s);
}

QPixmap wiki(int s)
{
    return makeIcon({
        "..GGGG..", ".GGGGGG.", "GG.GG.GG", "GGGGGGGG",
        "GG.GG.GG", ".GGGGGG.", "..GGGG..", "........",
    }, {{QLatin1Char('G'), qRgb(120, 200, 120)}}, s);
}

QPixmap fanart(int s)
{
    return makeIcon({
        "...AA...", "...AA...", ".AAAAAA.", "AAAAAAAA",
        ".AAAAAA.", "..AAAA..", ".AA..AA.", "........",
    }, {{QLatin1Char('A'), qRgb(255, 215, 90)}}, s);
}

QPixmap gear(int s)
{
    return makeIcon({
        "..GGGG..", ".GGGGGG.", "GGG..GGG", "GG....GG",
        "GG....GG", "GGG..GGG", ".GGGGGG.", "..GGGG..",
    }, {{QLatin1Char('G'), qRgb(150, 150, 150)}}, s);
}

QPixmap folder(int s)
{
    return makeIcon({
        "........", ".FFFF...", "FFFFFFFF", "F......F",
        "F......F", "F......F", "FFFFFFFF", "........",
    }, {{QLatin1Char('F'), qRgb(220, 175, 85)}}, s);
}

}
