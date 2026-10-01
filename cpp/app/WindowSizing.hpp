#pragma once

#include <QScrollArea>
#include <QSize>
#include <QString>
#include <QWidget>
#include <utility>
#include <vector>

class QResizeEvent;

// Window sizes that fit the screen and persist, and layouts that adapt to
// the window (README TODO: resizable windows). Every top-level window and
// dialog uses fitToScreen instead of a fixed resize().

// Sizes `window` to `preferred`, shrunk to fit within 90% of the available
// area of the screen it's on (or the primary screen), then restores the size
// the user last left it at, if any (QSettings key "windowSize/<key>"), also
// clamped to the screen. The size is saved whenever the window is hidden.
void fitToScreen(QWidget* window, QSize preferred, const QString& key);

// Wraps `content` in a frameless, widget-resizable QScrollArea parented to
// `parent`, so it scrolls instead of being cut off when its window is small.
QScrollArea* scrollable(QWidget* content, QWidget* parent);

// Two columns of widgets that collapse into one (the first column, then the
// second) when narrower than `breakpoint` pixels. Put it in scrollable() so
// the single column scrolls.
class ReflowColumns : public QWidget {
    Q_OBJECT

public:
    explicit ReflowColumns(int breakpoint, QWidget* parent = nullptr);
    void addWidget(int column, QWidget* widget);
    bool isTwoColumns() const { return twoColumns_; }
    // In two columns, no minimum width: a scroll area must be able to make it
    // narrower than the breakpoint, or it would never switch to one column.
    QSize minimumSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuild(bool twoColumns);

    int breakpoint_;
    bool twoColumns_ = false;
    bool built_ = false;
    std::vector<std::pair<int, QWidget*>> widgets_;
};
