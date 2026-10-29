#include "MyOverlay.h"
#include <QLabel>
#include <QVBoxLayout>

MyOverlay::MyOverlay(QWidget* parent) : QWidget(parent) {
    setObjectName("overlay");
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setStyleSheet("#overlay { background: rgba(237,247,240,245); border: 2px dashed #6bae84; border-radius: 12px; } QLabel { border: 0; background: transparent; color: #39805a; }");
    QVBoxLayout* column = new QVBoxLayout(this);
    QLabel* title = new QLabel("松开鼠标，添加到压缩队列");
    title->setStyleSheet("font-size: 20px; font-weight: 600;");
    QLabel* subtitle = new QLabel("PNG / JPEG / WebP / SVG");
    column->addStretch();
    column->addWidget(title, 0, Qt::AlignCenter);
    column->addWidget(subtitle, 0, Qt::AlignCenter);
    column->addStretch();
    hide();
}
MyOverlay::~MyOverlay() {}
