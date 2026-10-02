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

#include <QLayout>

// A layout that places widgets left to right and wraps them onto new rows
// when the width runs out (Qt's "flow layout" example), so a row of buttons
// never forces its window wider than the widest single button.
class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget* parent = nullptr, int spacing = -1);
    ~FlowLayout() override;

    void addItem(QLayoutItem* item) override;
    int count() const override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    QSize minimumSize() const override;
    QSize sizeHint() const override;
    void setGeometry(const QRect& rect) override;

private:
    int arrange(const QRect& rect, bool apply) const;  // returns the height used
    int gap() const;

    QList<QLayoutItem*> items_;
    int spacing_;
};

#include <map>

class QGridLayout;
class QLabel;

// A two-column form (label | field) on a QGridLayout, with QFormLayout's
// addRow / labelForField. Use it instead of QFormLayout where rows are shown
// and hidden: Qt 6.4's QFormLayout writes past its own memory when rows are
// hidden (found by valgrind; it crashed the AppImage build's Qt 6.4).
class GridForm : public QObject {
public:
    explicit GridForm(QWidget* parent);
    void addRow(const QString& label, QWidget* field);
    void addRow(QWidget* spanning);  // across both columns
    QWidget* labelForField(QWidget* field) const;  // nullptr for spanning rows

private:
    QGridLayout* grid_;
    std::map<QWidget*, QLabel*> labels_;
    int rows_ = 0;
};
