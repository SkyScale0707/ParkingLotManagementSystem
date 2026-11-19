#pragma once
// ---------------------------------------------------------------------------
// page.h — 页面基类：统一 refresh / tick 接口，便于主窗口切换与定时刷新
// ---------------------------------------------------------------------------
#include <QWidget>

namespace ui {

class Page : public QWidget
{
public:
    using QWidget::QWidget;

    virtual void refresh() = 0;
    virtual void tick() {}
};

} // namespace ui
