#pragma once
#include "Config.h"
#include <QPalette>

class Theme : public QObject {
    Q_OBJECT
public:
    static Theme* instance();
    void setMode(ThemeMode mode);
    ThemeMode mode() const { return currentMode; }
    bool isDark() const;
    QString css(QString text) const;
    QColor color(const char* light, const char* dark) const;
signals:
    void changed();
private:
    explicit Theme(QObject* parent);
    ThemeMode currentMode;
    QPalette nativePalette;
    bool applying = false;
    void apply();
};
