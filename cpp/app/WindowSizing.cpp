#include "WindowSizing.hpp"

#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QStyle>
#include <QVBoxLayout>
#include <QGuiApplication>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QWidget>

namespace {

QString settingsKey(const QString& key) {
    return "windowSize/" + key;
}

// Saves the window's size when it's hidden (closed, accepted, or rejected).
class SizeSaver : public QObject {
public:
    SizeSaver(QWidget* window, QString key) : QObject(window), window_(window), key_(std::move(key)) {
        window->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == window_ && event->type() == QEvent::Hide && window_->isWindow()) {
            QSettings().setValue(settingsKey(key_), window_->size());
        }
        return false;
    }

private:
    QWidget* window_;
    QString key_;
};

QSize clampToScreen(QWidget* window, QSize size) {
    QScreen* screen = window->screen() ? window->screen() : QGuiApplication::primaryScreen();
    if (!screen) return size;
    QSize available = screen->availableGeometry().size() * 0.9;
    return size.boundedTo(available).expandedTo(window->minimumSizeHint().boundedTo(available));
}

}  // namespace

void fitToScreen(QWidget* window, QSize preferred, const QString& key) {
    QSize size = preferred;
    QVariant saved = QSettings().value(settingsKey(key));
    if (saved.canConvert<QSize>() && saved.toSize().isValid()) size = saved.toSize();
    window->resize(clampToScreen(window, size));
    new SizeSaver(window, key);
}

QScrollArea* scrollable(QWidget* content, QWidget* parent) {
    auto* scroll = new QScrollArea(parent);
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    return scroll;
}

ReflowColumns::ReflowColumns(int breakpoint, QWidget* parent) : QWidget(parent), breakpoint_(breakpoint) {}

void ReflowColumns::addWidget(int column, QWidget* widget) {
    widget->setParent(this);
    widgets_.push_back({column, widget});
    rebuild(twoColumns_);
}

QSize ReflowColumns::minimumSizeHint() const {
    QSize hint = QWidget::minimumSizeHint();
    if (twoColumns_) hint.setWidth(0);
    return hint;
}

void ReflowColumns::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    bool wide = width() >= breakpoint_;
    if (!built_ || wide != twoColumns_) rebuild(wide);
}

void ReflowColumns::rebuild(bool twoColumns) {
    delete layout();  // the widgets stay; only the arrangement changes
    twoColumns_ = twoColumns;
    built_ = true;
    if (twoColumns) {
        auto* row = new QHBoxLayout(this);
        row->setContentsMargins(0, 0, 0, 0);
        QVBoxLayout* columns[2] = {new QVBoxLayout(), new QVBoxLayout()};
        for (auto [column, widget] : widgets_) columns[column == 0 ? 0 : 1]->addWidget(widget);
        for (auto* column : columns) {
            column->addStretch(1);
            row->addLayout(column, 1);
        }
    } else {
        auto* stack = new QVBoxLayout(this);
        stack->setContentsMargins(0, 0, 0, 0);
        for (int pass = 0; pass < 2; ++pass) {
            for (auto [column, widget] : widgets_) {
                if ((column == 0 ? 0 : 1) == pass) stack->addWidget(widget);
            }
        }
        stack->addStretch(1);
    }
}

FlowLayout::FlowLayout(QWidget* parent, int spacing) : QLayout(parent), spacing_(spacing) {}

FlowLayout::~FlowLayout() {
    while (QLayoutItem* item = takeAt(0)) delete item;
}

void FlowLayout::addItem(QLayoutItem* item) {
    items_.append(item);
}

int FlowLayout::count() const {
    return static_cast<int>(items_.size());
}

QLayoutItem* FlowLayout::itemAt(int index) const {
    return index >= 0 && index < items_.size() ? items_.at(index) : nullptr;
}

QLayoutItem* FlowLayout::takeAt(int index) {
    return index >= 0 && index < items_.size() ? items_.takeAt(index) : nullptr;
}

Qt::Orientations FlowLayout::expandingDirections() const {
    return {};
}

bool FlowLayout::hasHeightForWidth() const {
    return true;
}

int FlowLayout::heightForWidth(int width) const {
    return arrange(QRect(0, 0, width, 0), false);
}

QSize FlowLayout::minimumSize() const {
    QSize size;
    for (const QLayoutItem* item : items_) size = size.expandedTo(item->minimumSize());
    const QMargins margins = contentsMargins();
    return size + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}

QSize FlowLayout::sizeHint() const {
    // As wide as everything on one row.
    int width = 0;
    int height = 0;
    for (const QLayoutItem* item : items_) {
        width += item->sizeHint().width() + (width > 0 ? gap() : 0);
        height = std::max(height, item->sizeHint().height());
    }
    const QMargins margins = contentsMargins();
    return QSize(width + margins.left() + margins.right(), height + margins.top() + margins.bottom());
}

void FlowLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);
    arrange(rect, true);
}

int FlowLayout::gap() const {
    if (spacing_ >= 0) return spacing_;
    QWidget* parent = parentWidget();
    return parent ? parent->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing, nullptr, parent) : 6;
}

int FlowLayout::arrange(const QRect& rect, bool apply) const {
    const QMargins margins = contentsMargins();
    QRect area = rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
    int spacing = std::max(gap(), 4);
    int x = area.x();
    int y = area.y();
    int rowHeight = 0;
    for (QLayoutItem* item : items_) {
        QSize size = item->sizeHint();
        if (x > area.x() && x + size.width() > area.right() + 1) {  // wrap
            x = area.x();
            y += rowHeight + spacing;
            rowHeight = 0;
        }
        if (apply) item->setGeometry(QRect(QPoint(x, y), size));
        x += size.width() + spacing;
        rowHeight = std::max(rowHeight, size.height());
    }
    return y + rowHeight - rect.y() + margins.bottom();
}

GridForm::GridForm(QWidget* parent) : QObject(parent), grid_(new QGridLayout(parent)) {
    grid_->setColumnStretch(1, 1);
}

void GridForm::addRow(const QString& label, QWidget* field) {
    auto* name = new QLabel(label, field->parentWidget());
    name->setBuddy(field);
    grid_->addWidget(name, rows_, 0, Qt::AlignLeft | Qt::AlignVCenter);
    grid_->addWidget(field, rows_, 1);
    labels_[field] = name;
    ++rows_;
}

void GridForm::addRow(QWidget* spanning) {
    grid_->addWidget(spanning, rows_, 0, 1, 2);
    ++rows_;
}

QWidget* GridForm::labelForField(QWidget* field) const {
    auto found = labels_.find(field);
    return found == labels_.end() ? nullptr : found->second;
}
