#include "Theme.h"
#include <QApplication>
#include <QStyleHints>

Theme* Theme::instance() {
    static Theme* theme = new Theme(qApp);
    return theme;
}
Theme::Theme(QObject* parent) : QObject(parent), currentMode(Config(nullptr).get().themeMode), nativePalette(QApplication::palette()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this]() {
        if (currentMode == ThemeMode::System) apply();
    });
#else
    connect(qApp, &QApplication::paletteChanged, this, [this](const QPalette& palette) {
        if (applying) return;
        nativePalette = palette;
        if (currentMode == ThemeMode::System) apply();
    });
#endif
    apply();
}
bool Theme::isDark() const {
    if (currentMode != ThemeMode::System) return currentMode == ThemeMode::Dark;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
    if (scheme != Qt::ColorScheme::Unknown) return scheme == Qt::ColorScheme::Dark;
#endif
    return nativePalette.color(QPalette::Window).lightness() < 128;
}
QColor Theme::color(const char* light, const char* dark) const { return QColor(isDark() ? dark : light); }
void Theme::setMode(ThemeMode mode) { currentMode = mode; apply(); }
void Theme::apply() {
    QPalette palette(nativePalette);
    palette.setColor(QPalette::Window, color("#f4f7f5", "#141c19"));
    palette.setColor(QPalette::WindowText, color("#203b30", "#e1ece5"));
    palette.setColor(QPalette::Base, color("#ffffff", "#1d2822"));
    palette.setColor(QPalette::AlternateBase, color("#f8fbf9", "#223029"));
    palette.setColor(QPalette::Text, palette.color(QPalette::WindowText));
    palette.setColor(QPalette::Button, palette.color(QPalette::Base));
    palette.setColor(QPalette::ButtonText, palette.color(QPalette::WindowText));
    palette.setColor(QPalette::Highlight, color("#278558", "#40966a"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, color("#9daaa2", "#6b8073"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, palette.color(QPalette::Disabled, QPalette::Text));
    applying = true;
    QApplication::setPalette(palette);
    applying = false;
    emit changed();
}
QString Theme::css(QString text) const {
    if (!isDark()) return text;
    // Shared tokens keep native widgets, custom painting and rich-text logs consistent.
    const QPair<const char*, const char*> tokens[] = {
        {"background: white", "background: #1d2822"}, {"#f4f7f5", "#141c19"},
        {"#203b30", "#e1ece5"}, {"#183e2b", "#edf7ef"}, {"#e1e9e4", "#33483b"},
        {"#7b8d83", "#a0b5a8"}, {"#42986b", "#76c797"}, {"#e5f1e9", "#284535"},
        {"#397756", "#9bdab4"}, {"#fff6ed", "#3a2b23"}, {"#985539", "#f0b190"},
        {"#f0dcc7", "#654733"}, {"#bf6353", "#eea18e"}, {"#2d8f5e", "#84d7a5"},
        {"#365a46", "#c2dcca"}, {"#dce6df", "#3d5345"}, {"#eef6f0", "#2b3e32"},
        {"#a5c9b2", "#72a788"}, {"#deece2", "#344b3d"}, {"#a2593e", "#eab38d"},
        {"#ffecd9", "#4c3728"}, {"#edf1ee", "#26352c"}, {"#9daaa2", "#81958a"},
        {"#e1e7e3", "#33483b"}, {"#6e897a", "#aac5b4"}, {"#e9f0eb", "#314338"},
        {"#f6f8f6", "#1d2822"}, {"#d2dfd6", "#536e5c"}, {"#eaf4ed", "#2b4433"},
        {"#237c51", "#9bdab4"}, {"#f8fbf9", "#223029"}, {"#3a5145", "#cfdfd4"},
        {"#e5f2e9", "#34523e"}, {"#f0f4f1", "#2d3e33"}, {"#f3f7f4", "#25352b"},
        {"#73877a", "#a4bcad"}, {"#42624e", "#bad5c3"}, {"#768a7d", "#a3bcad"},
        {"rgba(237,247,240,245)", "rgba(32,53,41,245)"}, {"#39805a", "#a4dfbb"}
    };
    for (const auto& token : tokens) text.replace(token.first, token.second);
    return text;
}
