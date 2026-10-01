#include "WindowSizing.hpp"

#include <QEvent>
#include <QHBoxLayout>
#include <QResizeEvent>
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
