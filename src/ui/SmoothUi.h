#pragma once

#include <QAbstractButton>
#include <QColor>
#include <QEvent>
#include <QFontMetrics>
#include <QIcon>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QVariant>
#include <QWidget>
#include <algorithm>

namespace lumina {

inline QString modernScrollBarQss(bool darkTheme) {
    if (darkTheme) {
        return QStringLiteral(
            "QScrollBar:vertical { background: transparent; width: 9px; margin: 2px 1px; border: none; }"
            "QScrollBar::handle:vertical { background: rgba(255, 255, 255, 65); min-height: 28px; border-radius: 4px; }"
            "QScrollBar::handle:vertical:hover { background: rgba(168, 85, 247, 175); }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; width: 0px; border: none; background: none; }"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
            "QScrollBar:horizontal { background: transparent; height: 9px; margin: 1px 2px; border: none; }"
            "QScrollBar::handle:horizontal { background: rgba(255, 255, 255, 65); min-width: 28px; border-radius: 4px; }"
            "QScrollBar::handle:horizontal:hover { background: rgba(168, 85, 247, 175); }"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { height: 0px; width: 0px; border: none; background: none; }"
            "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"
        );
    }
    return QStringLiteral(
        "QScrollBar:vertical { background: transparent; width: 9px; margin: 2px 1px; border: none; }"
        "QScrollBar::handle:vertical { background: #CBD5E1; min-height: 28px; border-radius: 4px; }"
        "QScrollBar::handle:vertical:hover { background: #A855F7; }"
        "QScrollBar::handle:vertical:pressed { background: #9333EA; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; width: 0px; border: none; background: none; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
        "QScrollBar:horizontal { background: transparent; height: 9px; margin: 1px 2px; border: none; }"
        "QScrollBar::handle:horizontal { background: #CBD5E1; min-width: 28px; border-radius: 4px; }"
        "QScrollBar::handle:horizontal:hover { background: #A855F7; }"
        "QScrollBar::handle:horizontal:pressed { background: #9333EA; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { height: 0px; width: 0px; border: none; background: none; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"
    );
}

// Anti-aliased vector painter for QPushButton using brand violet #A855F7 accents & dynamic light/dark theme support
class SmoothButtonFilter : public QObject {
public:
    explicit SmoothButtonFilter(bool darkTheme, QObject* parent = nullptr)
        : QObject(parent), darkTheme_(darkTheme) {}

    void setDarkTheme(bool darkTheme) { darkTheme_ = darkTheme; }
    bool isDarkTheme() const { return darkTheme_; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() != QEvent::Paint) {
            return QObject::eventFilter(watched, event);
        }
        auto* btn = qobject_cast<QPushButton*>(watched);
        if (!btn || btn->property("skipSmoothPaint").toBool()) {
            return QObject::eventFilter(watched, event);
        }

        const QString name = btn->objectName();
        const bool enabled = btn->isEnabled();
        const bool hovered = enabled && btn->underMouse();
        const bool pressed = enabled && btn->isDown();
        const bool checked = btn->isChecked();

        QColor bg;
        QColor border;
        QColor textCol;
        double radius = 8.0;

        if (darkTheme_) {
            radius = 9.0;
            if (name == QStringLiteral("miniResetBtn")) {
                radius = 5.0;
                bg = !enabled ? Qt::transparent
                              : (pressed ? QColor(168, 85, 247, 85)
                                         : (hovered ? QColor(168, 85, 247, 50) : QColor(255, 255, 255, 16)));
                border = !enabled ? Qt::transparent
                                  : (hovered ? QColor(192, 132, 252, 165) : QColor(255, 255, 255, 28));
                textCol = !enabled ? QColor(100, 116, 139) : QColor(0xE9, 0xD5, 0xFF);
            } else if (name == QStringLiteral("primaryBtn")) {
                bg = !enabled ? QColor(168, 85, 247, 90)
                              : (pressed ? QColor(0x7E, 0x22, 0xCE)
                                         : (hovered ? QColor(0x93, 0x33, 0xEA) : QColor(0xA8, 0x55, 0xF7)));
                border = !enabled ? QColor(192, 132, 252, 95) : QColor(0xC0, 0x84, 0xFC);
                textCol = enabled ? Qt::white : QColor(255, 255, 255, 165);
            } else if (name == QStringLiteral("dangerBtn")) {
                if (checked) {
                    bg = hovered ? QColor(0xDC, 0x26, 0x26) : QColor(0xE1, 0x1D, 0x48);
                    border = bg;
                    textCol = Qt::white;
                } else {
                    bg = !enabled ? QColor(255, 255, 255, 10)
                                  : (pressed ? QColor(244, 63, 94, 80)
                                             : (hovered ? QColor(244, 63, 94, 52) : QColor(244, 63, 94, 28)));
                    border = hovered ? QColor(251, 113, 133, 180) : QColor(244, 63, 94, 110);
                    textCol = enabled ? QColor(0xFD, 0xA4, 0xAF) : QColor(255, 255, 255, 100);
                }
            } else if (name == QStringLiteral("starBtnActive") || (name == QStringLiteral("starBtn") && checked)) {
                bg = hovered ? QColor(250, 204, 21, 56) : QColor(250, 204, 21, 38);
                border = QColor(250, 204, 21, 155);
                textCol = QColor(0xFE, 0xF0, 0x8A);
            } else if (checked) {
                bg = hovered ? QColor(168, 85, 247, 175) : QColor(168, 85, 247, 135);
                border = QColor(0xC0, 0x84, 0xFC);
                textCol = Qt::white;
            } else {
                bg = !enabled ? QColor(255, 255, 255, 10)
                              : (pressed ? QColor(255, 255, 255, 58)
                                         : (hovered ? QColor(255, 255, 255, 40) : QColor(255, 255, 255, 22)));
                border = hovered ? QColor(192, 132, 252, 150) : QColor(255, 255, 255, 34);
                textCol = enabled ? QColor(0xF4, 0xF4, 0xF5) : QColor(255, 255, 255, 105);
            }
        } else {
            if (name == QStringLiteral("miniResetBtn")) {
                radius = 5.0;
                bg = !enabled ? Qt::transparent
                              : (pressed ? QColor(0xF3, 0xE8, 0xFF)
                                         : (hovered ? QColor(0xFA, 0xF5, 0xFF) : QColor(0xF1, 0xF5, 0xF9)));
                border = !enabled ? Qt::transparent
                                  : (hovered ? QColor(0xC0, 0x84, 0xFC) : QColor(0xE2, 0xE8, 0xF0));
                textCol = !enabled ? QColor(0xCB, 0xD5, 0xE1)
                                   : (hovered ? QColor(0x7E, 0x22, 0xCE) : QColor(0x64, 0x74, 0x8B));
            } else if (name == QStringLiteral("primaryBtn")) {
                bg = !enabled ? QColor(0xD8, 0xB4, 0xFE)
                              : (pressed ? QColor(0x7E, 0x22, 0xCE)
                                         : (hovered ? QColor(0x93, 0x33, 0xEA) : QColor(0xA8, 0x55, 0xF7)));
                border = bg;
                textCol = Qt::white;
            } else if (name == QStringLiteral("dangerBtn")) {
                if (checked) {
                    bg = hovered ? QColor(0xDC, 0x26, 0x26) : QColor(0xE1, 0x1D, 0x48);
                    border = bg;
                    textCol = Qt::white;
                } else {
                    bg = !enabled ? QColor(0xF8, 0xFA, 0xFC)
                                  : (pressed ? QColor(0xFE, 0xCD, 0xD3)
                                             : (hovered ? QColor(0xFF, 0xE4, 0xE6) : QColor(0xFF, 0xF1, 0xF2)));
                    border = !enabled ? QColor(0xE2, 0xE8, 0xF0)
                                      : (hovered ? QColor(0xFD, 0xA4, 0xAF) : QColor(0xFE, 0xCD, 0xD3));
                    textCol = enabled ? QColor(0xBE, 0x12, 0x3C) : QColor(0x94, 0xA3, 0xB8);
                }
            } else if (name == QStringLiteral("starBtnActive") || (name == QStringLiteral("starBtn") && checked)) {
                bg = hovered ? QColor(0xFD, 0xE6, 0x8A) : QColor(0xFE, 0xF3, 0xC7);
                border = QColor(0xF5, 0x9E, 0x0B);
                textCol = QColor(0x92, 0x40, 0x0E);
            } else if (name == QStringLiteral("starBtn")) {
                bg = pressed ? QColor(0xFE, 0xF3, 0xC7)
                             : (hovered ? QColor(0xFF, 0xFB, 0xEB) : QColor(0xFF, 0xFF, 0xFF));
                border = hovered ? QColor(0xFC, 0xD3, 0x4D) : QColor(0xCB, 0xD5, 0xE1);
                textCol = hovered ? QColor(0x92, 0x40, 0x0E) : QColor(0x33, 0x41, 0x55);
            } else if (name == QStringLiteral("backBtn")) {
                bg = pressed ? QColor(0xF3, 0xE8, 0xFF)
                             : (hovered ? QColor(0xFA, 0xF5, 0xFF) : QColor(0xF8, 0xFA, 0xFC));
                border = hovered ? QColor(0xC0, 0x84, 0xFC) : QColor(0xCB, 0xD5, 0xE1);
                textCol = hovered ? QColor(0x7E, 0x22, 0xCE) : QColor(0x0F, 0x17, 0x2A);
            } else if (checked) {
                bg = hovered ? QColor(0xF3, 0xE8, 0xFF) : QColor(0xFA, 0xF5, 0xFF);
                border = QColor(0xA8, 0x55, 0xF7);
                textCol = QColor(0x7E, 0x22, 0xCE);
            } else {
                bg = !enabled ? QColor(0xF8, 0xFA, 0xFC)
                              : (pressed ? QColor(0xF3, 0xE8, 0xFF)
                                         : (hovered ? QColor(0xFA, 0xF5, 0xFF) : QColor(0xFF, 0xFF, 0xFF)));
                border = !enabled ? QColor(0xE2, 0xE8, 0xF0)
                                  : (hovered ? QColor(0xC0, 0x84, 0xFC) : QColor(0xCB, 0xD5, 0xE1));
                textCol = enabled ? (hovered ? QColor(0x6B, 0x21, 0xA8) : QColor(0x1E, 0x29, 0x3B))
                                  : QColor(0x94, 0xA3, 0xB8);
            }
        }

        QPainter p(btn);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QRectF r = QRectF(btn->rect()).adjusted(0.75, 0.75, -0.75, -0.75);
        QPainterPath path;
        path.addRoundedRect(r, radius, radius);
        p.fillPath(path, bg);

        const QVariant swatchVar = btn->property("swatchColor");
        if (swatchVar.isValid()) {
            const QColor sw(swatchVar.toString());
            if (sw.isValid()) {
                p.save();
                p.setClipPath(path);
                p.fillRect(QRectF(r.left(), r.top(), 14.0, r.height()), sw);
                p.restore();
            }
        }

        if (border.alpha() > 0) {
            p.setPen(QPen(border, 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        }

        const QIcon ic = btn->icon();
        const QString txt = btn->text();
        const bool hasIcon = !ic.isNull();
        const bool hasText = !txt.isEmpty();

        QSize icSz = btn->iconSize();
        if (!icSz.isValid() || icSz.isEmpty()) {
            icSz = QSize(16, 16);
        }

        QRectF contentRect = r.adjusted(4.0, 1.0, -4.0, -1.0);
        if (swatchVar.isValid()) {
            contentRect.adjust(12.0, 0.0, 0.0, 0.0);
        }

        p.setFont(btn->font());
        p.setPen(textCol);

        const QFontMetricsF fm(btn->font());
        const double textW = hasText ? fm.horizontalAdvance(txt) : 0.0;
        const double gap = (hasIcon && hasText) ? 6.0 : 0.0;
        const double iconW = hasIcon ? icSz.width() : 0.0;
        const double totalW = iconW + gap + textW;

        if (hasIcon && hasText) {
            const double startX = std::max(contentRect.left(), contentRect.center().x() - totalW * 0.5);
            const QRect icRect(qRound(startX),
                               qRound(contentRect.center().y() - icSz.height() * 0.5),
                               icSz.width(),
                               icSz.height());
            ic.paint(&p, icRect, Qt::AlignCenter, enabled ? QIcon::Normal : QIcon::Disabled,
                     checked ? QIcon::On : QIcon::Off);
            const QRectF txtRect(icRect.right() + gap, contentRect.top(),
                                 std::max(0.0, contentRect.right() - (icRect.right() + gap)),
                                 contentRect.height());
            p.drawText(txtRect, Qt::AlignLeft | Qt::AlignVCenter, txt);
        } else if (hasIcon) {
            const QRect icRect(qRound(contentRect.center().x() - icSz.width() * 0.5),
                               qRound(contentRect.center().y() - icSz.height() * 0.5),
                               icSz.width(),
                               icSz.height());
            ic.paint(&p, icRect, Qt::AlignCenter, enabled ? QIcon::Normal : QIcon::Disabled,
                     checked ? QIcon::On : QIcon::Off);
        } else if (hasText) {
            p.drawText(contentRect, Qt::AlignCenter, txt);
        }

        return true;
    }

private:
    bool darkTheme_ = false;
};

inline SmoothButtonFilter* installSmoothButtons(QWidget* root, bool darkTheme) {
    if (!root) return nullptr;
    auto* filter = new SmoothButtonFilter(darkTheme, root);
    const QList<QPushButton*> buttons = root->findChildren<QPushButton*>();
    for (QPushButton* btn : buttons) {
        if (btn && !btn->property("skipSmoothPaint").toBool()) {
            btn->installEventFilter(filter);
        }
    }
    return filter;
}

// Horizontally & vertically centered thumbnail strip delegate with #A855F7 brand active highlight
class FilmstripDelegate : public QStyledItemDelegate {
public:
    explicit FilmstripDelegate(bool darkTheme, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), darkTheme_(darkTheme) {}

    void setDarkTheme(bool darkTheme) { darkTheme_ = darkTheme; }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override {
        return QSize(52, 40);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QRectF cell = QRectF(option.rect).adjusted(2.0, 2.0, -2.0, -2.0);
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;

        if (selected) {
            painter->setBrush(darkTheme_ ? QColor(168, 85, 247, 68) : QColor(0xF3, 0xE8, 0xFF));
            painter->setPen(QPen(QColor(0xA8, 0x55, 0xF7), 2.0));
            painter->drawRoundedRect(cell, 5.0, 5.0);
        } else if (hovered) {
            painter->setBrush(darkTheme_ ? QColor(255, 255, 255, 24) : QColor(0xE2, 0xE8, 0xF0));
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(cell, 5.0, 5.0);
        }

        const QRectF thumbBox = cell.adjusted(3.0, 3.0, -3.0, -3.0);
        QPixmap pm = index.data(Qt::UserRole + 5).value<QPixmap>();
        if (pm.isNull()) {
            const QIcon ic = index.data(Qt::DecorationRole).value<QIcon>();
            if (!ic.isNull()) {
                pm = ic.pixmap(thumbBox.size().toSize());
            }
        }
        if (!pm.isNull()) {
            const QSize fitSz = pm.size().scaled(thumbBox.size().toSize(), Qt::KeepAspectRatio);
            const QRectF dst(thumbBox.center().x() - fitSz.width() * 0.5,
                             thumbBox.center().y() - fitSz.height() * 0.5,
                             fitSz.width(),
                             fitSz.height());
            QPainterPath clip;
            clip.addRoundedRect(dst, 3.0, 3.0);
            painter->setClipPath(clip);
            painter->drawPixmap(dst, pm, QRectF(0.0, 0.0, pm.width(), pm.height()));
        }

        painter->restore();
    }

private:
    bool darkTheme_ = false;
};

} // namespace lumina
