#include "Utility.h"
#include "TableModel.h"
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QtSvg/QSvgRenderer>
#include <iostream>
#include <cstdlib>

int checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
QImage render(const QByteArray& svg) {
    QSvgRenderer renderer(svg);
    check(renderer.isValid(), "SVG is renderable");
    QImage image(200, 200, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    renderer.render(&painter);
    return image;
}
void svgCase(const QByteArray& input, bool expectSmaller, bool expectOriginal = false) {
    QByteArray output;
    QString error;
    check(Utility::optimizeSvg(input, output, error), "valid SVG succeeds");
    check(error.isEmpty(), "successful optimization clears errors");
    check(output.size() <= input.size(), "SVG never grows");
    if (expectSmaller && output.size() >= input.size()) std::cerr << "Not smaller: " << input.constData() << "\nOutput: " << output.constData() << "\n";
    if (expectSmaller) check(output.size() < input.size(), "formatted SVG gets smaller");
    if (expectOriginal) check(output == input, "sensitive SVG stays byte-identical");
    check(render(input) == render(output), "pixels and transparency stay identical");
}
int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    check(Utility::checkFileFormat("picture.SVG"), "uppercase SVG accepted");
    check(Utility::checkFileFormat("picture.WeBp"), "existing WebP accepted");
    check(Utility::checkFileFormat("picture.JPEG"), "existing JPEG accepted");
    for (const QString& name : {QString("picture.jpg"), QString("picture.JPG"), QString("picture.jPg"), QString("picture.jpeg")}) {
        check(Utility::checkFileFormat(name), "JPG and JPEG accepted regardless of extension case");
    }
    check(Utility::getFileDialogFilter().contains("*.jpg") && Utility::getFileDialogFilter().contains("*.JPG"), "native file dialog includes lowercase and uppercase JPG");
    check(!Utility::checkFileFormat("picture.avif"), "AVIF not added");
    check(!Utility::checkFileFormat("picture.svg.txt"), "incorrect extension rejected");
    check(!Utility::checkFileFormat("/directory.svg/picture"), "extension in directory rejected");
    TableModelRow row;
    for (int status : {-1, 0, 1, 2}) {
        row.status = status;
        row.thin_size = 100; // Stale output sizes must not decide which task to retry.
        check(row.needsCompression(true) == (status == -1), "retry selects failures only");
        check(row.needsCompression() == (status == -1 || status == 0), "start selects pending and failed only");
    }
    svgCase(R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
    <!-- design annotation -->
    <g opacity="0.6">
        <path d="M10 10H90V90H10Z" fill="#267a52" />
        <circle cx="50" cy="50" r="20" fill="#e8bc70" />
    </g>
</svg>)SVG", true);
    svgCase(QString::fromUtf8(R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 200">
    <text x="10" y="80" font-size="18">Hello <tspan>世界</tspan> <tspan> SVG </tspan></text>
</svg>)SVG").toUtf8(), true);
    svgCase(R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 200">
    <text x="10" y="80" xml:space="preserve"> A  <tspan> B </tspan> C </text>
</svg>)SVG", true);
    svgCase(R"SVG(<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="0 0 100 100">
    <defs>
        <linearGradient id="gradient"><stop offset="0" stop-color="#267a52"/><stop offset="1" stop-color="#ffd0a0"/></linearGradient>
        <circle id="dot" cx="50" cy="50" r="30" fill="url(#gradient)"/>
    </defs>
    <use xlink:href="#dot" />
</svg>)SVG", true);
    svgCase(R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
    <style>circle { fill: #abcdef; }</style>
    <!-- retained in sensitive documents -->
    <circle cx="50" cy="50" r="30"/>
</svg>)SVG", false, true);
    svgCase(R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><script>/* DOM-sensitive */</script><circle cx="50" cy="50" r="30"/></svg>)SVG", false, true);
    svgCase(R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><foreignObject><div xmlns="http://www.w3.org/1999/xhtml"> A B </div></foreignObject></svg>)SVG", false, true);
    svgCase(R"SVG(<!DOCTYPE svg [<!ENTITY color "red">]><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><circle fill="&color;" cx="50" cy="50" r="30"/></svg>)SVG", false, true);
    svgCase(R"SVG(<svg xmlns="http://www.w3.org/2000/svg"/>)SVG", false, true);
    for (const QByteArray input : {QByteArray(), QByteArray("<html/>"), QByteArray("<svg><path></svg>"), QByteArray("<svg xmlns='urn:wrong'/>"), QByteArray("not an image")}) {
        QByteArray output("unchanged");
        QString error;
        check(!Utility::optimizeSvg(input, output, error), "malformed SVG fails");
        check(!error.isEmpty(), "malformed SVG has an explanation");
        check(output == "unchanged", "failed optimization does not replace output");
    }
    std::cout << "Passed " << checks << " checks\n";
}
