#include "QuickViewerWindow.h"
#include "SettingsDialog.h"
#include "SmoothUi.h"
#include "../core/AppSettings.h"
#include "../core/ImageLoader.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QImageWriter>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointer>
#include <QPolygonF>
#include <QProgressDialog>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtConcurrent>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>

namespace lumina {

namespace {

enum class ViewerIconKind {
    ZoomIn,
    ZoomOut,
    Prev,
    Next,
    RotateLeft,
    RotateRight,
    StarOutline,
    StarFilled,
    Layers,
    Backdrop,
    Export,
    Settings,
    WindowShrink,
    WindowExpand,
    Close
};

void drawViewerVectorGlyph(QPainter& p, ViewerIconKind kind, const QRectF& box, const QColor& color) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    const double cx = box.center().x();
    const double cy = box.center().y();
    const double s = std::min(box.width(), box.height());

    QPen pen(color, std::max(1.6, s * 0.095), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (kind) {
        case ViewerIconKind::ZoomIn:
        case ViewerIconKind::ZoomOut: {
            const double r = s * 0.25;
            const QPointF gc(cx - s * 0.08, cy - s * 0.08);
            p.drawEllipse(gc, r, r);
            p.drawLine(QPointF(gc.x() + r * 0.72, gc.y() + r * 0.72), QPointF(cx + s * 0.34, cy + s * 0.34));
            p.drawLine(QPointF(gc.x() - r * 0.48, gc.y()), QPointF(gc.x() + r * 0.48, gc.y()));
            if (kind == ViewerIconKind::ZoomIn) {
                p.drawLine(QPointF(gc.x(), gc.y() - r * 0.48), QPointF(gc.x(), gc.y() + r * 0.48));
            }
            break;
        }
        case ViewerIconKind::Prev: {
            QPainterPath path;
            path.moveTo(cx + s * 0.14, cy - s * 0.28);
            path.lineTo(cx - s * 0.16, cy);
            path.lineTo(cx + s * 0.14, cy + s * 0.28);
            p.drawPath(path);
            break;
        }
        case ViewerIconKind::Next: {
            QPainterPath path;
            path.moveTo(cx - s * 0.14, cy - s * 0.28);
            path.lineTo(cx + s * 0.16, cy);
            path.lineTo(cx - s * 0.14, cy + s * 0.28);
            p.drawPath(path);
            break;
        }
        case ViewerIconKind::RotateLeft: {
            const QRectF arcR(cx - s * 0.30, cy - s * 0.28, s * 0.60, s * 0.60);
            p.drawArc(arcR, 90 * 16, -245 * 16);
            QPolygonF head;
            head << QPointF(cx, cy - s * 0.40) << QPointF(cx - s * 0.22, cy - s * 0.27) << QPointF(cx, cy - s * 0.14);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawPolygon(head);
            break;
        }
        case ViewerIconKind::RotateRight: {
            const QRectF arcR(cx - s * 0.30, cy - s * 0.28, s * 0.60, s * 0.60);
            p.drawArc(arcR, 90 * 16, 245 * 16);
            QPolygonF head;
            head << QPointF(cx, cy - s * 0.40) << QPointF(cx + s * 0.22, cy - s * 0.27) << QPointF(cx, cy - s * 0.14);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawPolygon(head);
            break;
        }
        case ViewerIconKind::StarOutline:
        case ViewerIconKind::StarFilled: {
            QPolygonF star;
            const double outerR = s * 0.36;
            const double innerR = outerR * 0.43;
            constexpr double pi = 3.14159265358979323846;
            for (int i = 0; i < 10; ++i) {
                const double r = (i % 2 == 0) ? outerR : innerR;
                const double angle = -pi / 2.0 + i * (pi / 5.0);
                star << QPointF(cx + r * std::cos(angle), cy + s * 0.02 + r * std::sin(angle));
            }
            if (kind == ViewerIconKind::StarFilled) {
                p.setBrush(color);
            }
            p.drawPolygon(star);
            break;
        }
        case ViewerIconKind::Layers: {
            QPolygonF top;
            top << QPointF(cx, cy - s * 0.34) << QPointF(cx + s * 0.34, cy - s * 0.15)
                << QPointF(cx, cy + s * 0.04) << QPointF(cx - s * 0.34, cy - s * 0.15);
            p.drawPolygon(top);
            QPainterPath mid;
            mid.moveTo(cx - s * 0.34, cy + s * 0.02);
            mid.lineTo(cx, cy + s * 0.21);
            mid.lineTo(cx + s * 0.34, cy + s * 0.02);
            p.drawPath(mid);
            QPainterPath bot;
            bot.moveTo(cx - s * 0.34, cy + s * 0.17);
            bot.lineTo(cx, cy + s * 0.36);
            bot.lineTo(cx + s * 0.34, cy + s * 0.17);
            p.drawPath(bot);
            break;
        }
        case ViewerIconKind::Backdrop: {
            const QRectF sq(cx - s * 0.30, cy - s * 0.30, s * 0.60, s * 0.60);
            p.setBrush(QColor(255, 255, 255));
            p.drawRoundedRect(sq, 2.5, 2.5);
            QPainterPath half;
            half.moveTo(sq.topRight());
            half.lineTo(sq.bottomRight());
            half.lineTo(sq.bottomLeft());
            half.closeSubpath();
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(20, 22, 28));
            p.drawPath(half);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(sq, 2.5, 2.5);
            break;
        }
        case ViewerIconKind::Export: {
            // Upward arrow out of crisp tray
            p.drawLine(QPointF(cx, cy + s * 0.14), QPointF(cx, cy - s * 0.30));
            QPainterPath arrow;
            arrow.moveTo(cx - s * 0.19, cy - s * 0.13);
            arrow.lineTo(cx, cy - s * 0.32);
            arrow.lineTo(cx + s * 0.19, cy - s * 0.13);
            p.drawPath(arrow);
            QPainterPath tray;
            tray.moveTo(cx - s * 0.30, cy + s * 0.08);
            tray.lineTo(cx - s * 0.30, cy + s * 0.30);
            tray.lineTo(cx + s * 0.30, cy + s * 0.30);
            tray.lineTo(cx + s * 0.30, cy + s * 0.08);
            p.drawPath(tray);
            break;
        }
        case ViewerIconKind::Settings: {
            const double rOuter = s * 0.37;
            const double rInner = s * 0.255;
            const double rCore = s * 0.125;
            constexpr double pi = 3.14159265358979323846;
            constexpr double daRoot = 0.22;
            constexpr double daTip = 0.14;
            QPolygonF poly;
            for (int i = 0; i < 8; ++i) {
                const double a0 = i * (2.0 * pi / 8.0);
                poly << QPointF(cx + rInner * std::cos(a0 - daRoot), cy + rInner * std::sin(a0 - daRoot))
                     << QPointF(cx + rOuter * std::cos(a0 - daTip),  cy + rOuter * std::sin(a0 - daTip))
                     << QPointF(cx + rOuter * std::cos(a0 + daTip),  cy + rOuter * std::sin(a0 + daTip))
                     << QPointF(cx + rInner * std::cos(a0 + daRoot), cy + rInner * std::sin(a0 + daRoot));
            }
            QPainterPath gearPath;
            gearPath.setFillRule(Qt::OddEvenFill);
            gearPath.addPolygon(poly);
            gearPath.closeSubpath();
            gearPath.addEllipse(QPointF(cx, cy), rCore, rCore);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawPath(gearPath);
            break;
        }
        case ViewerIconKind::WindowShrink: {
            // Overlapping restore windows glyph
            p.drawRoundedRect(QRectF(cx - s * 0.12, cy - s * 0.30, s * 0.42, s * 0.40), 2.0, 2.0);
            p.setBrush(QColor(24, 26, 32));
            p.drawRoundedRect(QRectF(cx - s * 0.32, cy - s * 0.10, s * 0.42, s * 0.40), 2.0, 2.0);
            break;
        }
        case ViewerIconKind::WindowExpand: {
            const double d = s * 0.28;
            const double arm = s * 0.17;
            // Top-left corner
            p.drawLine(QPointF(cx - d, cy - d), QPointF(cx - d + arm, cy - d));
            p.drawLine(QPointF(cx - d, cy - d), QPointF(cx - d, cy - d + arm));
            // Top-right corner
            p.drawLine(QPointF(cx + d, cy - d), QPointF(cx + d - arm, cy - d));
            p.drawLine(QPointF(cx + d, cy - d), QPointF(cx + d, cy - d + arm));
            // Bottom-left corner
            p.drawLine(QPointF(cx - d, cy + d), QPointF(cx - d + arm, cy + d));
            p.drawLine(QPointF(cx - d, cy + d), QPointF(cx - d, cy + d - arm));
            // Bottom-right corner
            p.drawLine(QPointF(cx + d, cy + d), QPointF(cx + d - arm, cy + d));
            p.drawLine(QPointF(cx + d, cy + d), QPointF(cx + d, cy + d - arm));
            break;
        }
        case ViewerIconKind::Close: {
            const double d = s * 0.24;
            p.drawLine(QPointF(cx - d, cy - d), QPointF(cx + d, cy + d));
            p.drawLine(QPointF(cx + d, cy - d), QPointF(cx - d, cy + d));
            break;
        }
    }
    p.restore();
}

QIcon makeViewerIcon(ViewerIconKind kind, const QColor& color, int size = 20) {
    QPixmap pm(size * 3, size * 3);
    pm.setDevicePixelRatio(3.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    drawViewerVectorGlyph(p, kind, QRectF(0, 0, size, size), color);
    return QIcon(pm);
}

// Custom anti-aliased circular button painted directly with QPainter (zero QSS border aliasing or off-center shift)
class ViewerRoundButton : public QPushButton {
public:
    ViewerRoundButton(ViewerIconKind kind, bool isCloseBtn, QWidget* parent = nullptr)
        : QPushButton(parent), kind_(kind), isCloseBtn_(isCloseBtn) {
        setFixedSize(36, 36);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setProperty("skipSmoothPaint", true);
    }

    void setIconKind(ViewerIconKind kind) {
        kind_ = kind;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF r = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
        const bool hovered = underMouse();
        const bool pressed = isDown();

        QColor bg(24, 26, 32, 224);
        QColor border(255, 255, 255, 48);
        if (isCloseBtn_) {
            if (pressed) {
                bg = QColor(185, 28, 28, 245);
                border = QColor(248, 113, 113, 220);
            } else if (hovered) {
                bg = QColor(220, 38, 38, 242);
                border = QColor(239, 68, 68, 220);
            }
        } else {
            if (pressed) {
                bg = QColor(168, 85, 247, 200);
                border = QColor(216, 180, 254, 215);
            } else if (hovered) {
                bg = QColor(55, 62, 76, 236);
                border = QColor(192, 132, 252, 150);
            }
        }

        p.setPen(QPen(border, 1.2));
        p.setBrush(bg);
        p.drawEllipse(r);

        const QRectF glyphBox(width() * 0.5 - 9.5, height() * 0.5 - 9.5, 19.0, 19.0);
        drawViewerVectorGlyph(p, kind_, glyphBox, QColor(0xF8, 0xFA, 0xFC));
    }

private:
    ViewerIconKind kind_;
    bool isCloseBtn_ = false;
};

} // namespace

// ==================== SCROLLBAR-FREE ZOOM & ANYTIME PAN CANVAS ====================
ViewerCanvasWidget::ViewerCanvasWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setAttribute(Qt::WA_TranslucentBackground, true);
}

void ViewerCanvasWidget::setImage(const QImage& img, bool resetView) {
    errorText_.clear();
    hasAlpha_ = !img.isNull() && img.hasAlphaChannel();
    pixmap_ = img.isNull() ? QPixmap() : QPixmap::fromImage(img);
    if (!pixmap_.isNull()) {
        pixmap_.setDevicePixelRatio(1.0);
    }
    if (resetView) {
        fitToWindow_ = true;
        panOffset_ = QPointF(0.0, 0.0);
        zoomFactor_ = computeFitScale();
    } else {
        clampPan();
    }
    updateHoverCursor(mapFromGlobal(QCursor::pos()));
    update();
    emit zoomChanged(effectiveScale(), fitToWindow_);
}

void ViewerCanvasWidget::setErrorText(const QString& text) {
    pixmap_ = QPixmap();
    hasAlpha_ = false;
    errorText_ = text;
    fitToWindow_ = true;
    zoomFactor_ = 1.0;
    panOffset_ = QPointF(0.0, 0.0);
    setCursor(Qt::ArrowCursor);
    update();
    emit zoomChanged(1.0, true);
}

void ViewerCanvasWidget::setFitToWindow(bool fit) {
    if (pixmap_.isNull()) return;
    fitToWindow_ = fit;
    if (fit) {
        panOffset_ = QPointF(0.0, 0.0);
        zoomFactor_ = computeFitScale();
    } else {
        zoomFactor_ = 1.0;
        panOffset_ = QPointF(0.0, 0.0);
    }
    clampPan();
    updateHoverCursor(mapFromGlobal(QCursor::pos()));
    update();
    emit zoomChanged(effectiveScale(), fitToWindow_);
}

void ViewerCanvasWidget::setActualSize() {
    if (pixmap_.isNull()) return;
    fitToWindow_ = false;
    zoomFactor_ = 1.0;
    panOffset_ = QPointF(0.0, 0.0);
    clampPan();
    updateHoverCursor(mapFromGlobal(QCursor::pos()));
    update();
    emit zoomChanged(effectiveScale(), fitToWindow_);
}

void ViewerCanvasWidget::setBackdropMode(CanvasBackdropMode mode) {
    if (backdropMode_ == mode) return;
    backdropMode_ = mode;
    update();
}

void ViewerCanvasWidget::zoomBy(double factor, const QPointF& anchorWidgetPos) {
    if (pixmap_.isNull() || factor <= 0.0) return;

    const double oldScale = effectiveScale();
    const double newScale = std::clamp(oldScale * factor, 0.05, 8.0);
    if (qFuzzyCompare(oldScale, newScale)) return;

    const QPointF widgetCenter(width() * 0.5, height() * 0.5);
    const QPointF anchor = anchorWidgetPos.isNull() ? widgetCenter : anchorWidgetPos;

    if (fitToWindow_) {
        fitToWindow_ = false;
    }

    const QPointF imgCenter = widgetCenter + panOffset_;
    const QPointF delta = anchor - imgCenter;
    const double ratio = newScale / oldScale;
    panOffset_ = anchor - delta * ratio - widgetCenter;
    zoomFactor_ = newScale;

    clampPan();
    updateHoverCursor(anchor);
    update();
    emit zoomChanged(effectiveScale(), fitToWindow_);
}

double ViewerCanvasWidget::computeFitScale() const {
    if (pixmap_.isNull()) return 1.0;
    const double availW = std::max(64.0, static_cast<double>(width()) - 48.0);
    const double availH = std::max(64.0, static_cast<double>(height()) - 24.0);
    const double sx = availW / static_cast<double>(pixmap_.width());
    const double sy = availH / static_cast<double>(pixmap_.height());
    // Default 1:1 native resolution unless image exceeds window bounds
    return std::clamp(std::min({1.0, sx, sy}), 0.01, 1.0);
}

double ViewerCanvasWidget::effectiveScale() const {
    if (pixmap_.isNull()) return 1.0;
    return fitToWindow_ ? computeFitScale() : zoomFactor_;
}

QRectF ViewerCanvasWidget::currentImageRect() const {
    if (pixmap_.isNull()) return QRectF();
    const double s = effectiveScale();
    const double w = pixmap_.width() * s;
    const double h = pixmap_.height() * s;
    const double cx = width() * 0.5 + panOffset_.x();
    const double cy = height() * 0.5 + panOffset_.y();
    return QRectF(cx - w * 0.5, cy - h * 0.5, w, h);
}

void ViewerCanvasWidget::clampPan() {
    if (pixmap_.isNull() || fitToWindow_) {
        panOffset_ = QPointF(0.0, 0.0);
        return;
    }
    const double s = effectiveScale();
    const double drawnW = pixmap_.width() * s;
    const double drawnH = pixmap_.height() * s;
    const double maxPanX = (drawnW + width()) * 0.5 - 60.0;
    const double maxPanY = (drawnH + height()) * 0.5 - 60.0;
    panOffset_.setX(std::clamp(panOffset_.x(), -std::max(0.0, maxPanX), std::max(0.0, maxPanX)));
    panOffset_.setY(std::clamp(panOffset_.y(), -std::max(0.0, maxPanY), std::max(0.0, maxPanY)));
}

void ViewerCanvasWidget::updateHoverCursor(const QPointF& mousePos) {
    if (pixmap_.isNull()) {
        setCursor(Qt::ArrowCursor);
        return;
    }
    const bool inside = currentImageRect().contains(mousePos);
    setCursor(inside ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

void ViewerCanvasWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (!errorText_.isEmpty()) {
        p.setPen(QColor(0xF8, 0x71, 0x71));
        QFont f = font();
        f.setPointSize(11);
        p.setFont(f);
        p.drawText(rect().adjusted(32, 32, -32, -32), Qt::AlignCenter | Qt::TextWordWrap, errorText_);
        return;
    }

    if (pixmap_.isNull()) return;

    const QRectF imgRect = currentImageRect();

    // Optional backdrop strictly behind the image (Default = OFF)
    if (backdropMode_ == CanvasBackdropMode::White) {
        p.fillRect(imgRect, Qt::white);
    } else if (backdropMode_ == CanvasBackdropMode::Black) {
        p.fillRect(imgRect, Qt::black);
    } else if (!hasAlpha_) {
        p.fillRect(imgRect.adjusted(-2.0, 4.0, 6.0, 12.0), QColor(0, 0, 0, 60));
        p.fillRect(imgRect.adjusted(0.0, 2.0, 3.0, 6.0), QColor(0, 0, 0, 95));
    }

    p.drawPixmap(imgRect, pixmap_, QRectF(0.0, 0.0, pixmap_.width(), pixmap_.height()));
}

void ViewerCanvasWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (fitToWindow_) {
        panOffset_ = QPointF(0.0, 0.0);
        if (!pixmap_.isNull()) {
            zoomFactor_ = computeFitScale();
        }
    } else {
        clampPan();
    }
    update();
    if (!pixmap_.isNull()) {
        emit zoomChanged(effectiveScale(), fitToWindow_);
    }
}

void ViewerCanvasWidget::wheelEvent(QWheelEvent* event) {
    if (pixmap_.isNull()) {
        event->ignore();
        return;
    }
    int delta = event->angleDelta().y();
    if (delta == 0) {
        delta = event->pixelDelta().y();
    }
    if (delta != 0) {
        const double factor = std::pow(1.0014, static_cast<double>(delta));
        zoomBy(factor, event->position());
    }
    event->accept();
}

void ViewerCanvasWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !pixmap_.isNull()) {
        const QRectF imgRect = currentImageRect();
        pressedInsideImage_ = imgRect.contains(event->position());
        dragging_ = true;
        didDrag_ = false;
        dragStartPos_ = event->pos();
        panStartOffset_ = panOffset_;
        if (pressedInsideImage_) {
            setCursor(Qt::ClosedHandCursor);
        }
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ViewerCanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_ && !pixmap_.isNull() && pressedInsideImage_) {
        const QPoint delta = event->pos() - dragStartPos_;
        if (delta.manhattanLength() > 3) {
            didDrag_ = true;
        }
        if (didDrag_) {
            if (fitToWindow_) {
                zoomFactor_ = computeFitScale();
                fitToWindow_ = false;
                emit zoomChanged(effectiveScale(), false);
            }
            panOffset_ = panStartOffset_ + QPointF(delta);
            clampPan();
            update();
        }
        event->accept();
        return;
    }
    updateHoverCursor(event->position());
    QWidget::mouseMoveEvent(event);
}

void ViewerCanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        updateHoverCursor(event->position());
        if (!didDrag_ && !pressedInsideImage_) {
            emit backdropClicked();
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ViewerCanvasWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !pixmap_.isNull()) {
        if (fitToWindow_) {
            fitToWindow_ = false;
            zoomFactor_ = 1.0;
            panOffset_ = QPointF(0.0, 0.0);
            clampPan();
            updateHoverCursor(event->position());
            update();
            emit zoomChanged(effectiveScale(), fitToWindow_);
        } else {
            setFitToWindow(true);
        }
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

// ==================== QUICK VIEWER WINDOW ====================
QuickViewerWindow::QuickViewerWindow(LibraryDatabase* db,
                                     const QString& initialFilePath,
                                     QWidget* parent)
    : QMainWindow(parent), db_(db) {
    setWindowTitle(trUi(QStringLiteral("Lumina — Просмотр"), QStringLiteral("Lumina — Viewer")));
    setWindowIcon(QIcon(QStringLiteral(":/icons/lumina.png")));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::StrongFocus);

    setStyleSheet(QStringLiteral(
        "QWidget#viewerDock { background-color: rgba(24, 26, 32, 242); border: 1px solid rgba(255, 255, 255, 34); border-radius: 14px; }"
        "QWidget#psdDrawer { background-color: rgba(24, 26, 32, 242); border: 1px solid rgba(255, 255, 255, 32); border-radius: 12px; }"
        "QPushButton { background-color: rgba(255, 255, 255, 20); color: #F4F4F5; border: none; border-radius: 9px; padding: 0px 12px; font-size: 12px; font-weight: 500; }"
        "QPushButton:hover { background-color: rgba(255, 255, 255, 42); }"
        "QPushButton:checked { background-color: rgba(168, 85, 247, 170); color: #FFFFFF; }"
        "QPushButton#primaryBtn { background-color: #A855F7; font-weight: 600; color: #FFFFFF; padding: 0px 14px; }"
        "QPushButton#primaryBtn:hover { background-color: #9333EA; }"
        "QLabel#zoomPctBadge { background-color: rgba(255, 255, 255, 14); color: #E4E4E7; border: 1px solid rgba(255, 255, 255, 26); border-radius: 9px; padding: 0px 10px; font-size: 12px; font-weight: 600; }"
        "QListWidget#miniStrip { background: transparent; border: none; outline: 0; padding: 0px; }"
        "QListWidget#psdLayers { background: transparent; border: none; color: #E4E4E7; outline: 0; font-size: 12px; }"
        "QListWidget#psdLayers::item { border-radius: 6px; padding: 5px; border: 1px solid transparent; }"
        "QListWidget#psdLayers::item:selected { background-color: rgba(168, 85, 247, 85); border-color: #A855F7; color: #FFFFFF; }"
    ) + modernScrollBarQss(true));

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(20, 14, 20, 16);
    root->setSpacing(8);

    const QColor iconInk(0xF4, 0xF4, 0xF5);

    // Top bar: Settings gear + Window Mode toggle + Close round buttons
    auto* topRow = new QHBoxLayout();
    topRow->addStretch();

    settingsBtn_ = new ViewerRoundButton(ViewerIconKind::Settings, false, central);
    winModeBtn_ = new ViewerRoundButton(ViewerIconKind::WindowShrink, false, central);
    closeBtn_ = new ViewerRoundButton(ViewerIconKind::Close, true, central);

    topRow->addWidget(settingsBtn_);
    topRow->addSpacing(8);
    topRow->addWidget(winModeBtn_);
    topRow->addSpacing(8);
    topRow->addWidget(closeBtn_);
    root->addLayout(topRow);

    // Center Canvas + PSD Layer Drawer
    auto* midRow = new QHBoxLayout();
    midRow->setSpacing(14);

    canvas_ = new ViewerCanvasWidget(central);

    psdDrawer_ = new QWidget(central);
    psdDrawer_->setObjectName(QStringLiteral("psdDrawer"));
    psdDrawer_->setFixedWidth(268);
    psdDrawer_->setVisible(false);
    auto* psdDrawerLay = new QVBoxLayout(psdDrawer_);
    psdDrawerLay->setContentsMargins(10, 10, 10, 10);
    psdDrawerLay->setSpacing(8);

    psdHeader_ = new QLabel(psdDrawer_);
    psdHeader_->setStyleSheet(QStringLiteral("color: #E4E4E7; font-size: 12px; font-weight: 600;"));
    psdLayerList_ = new QListWidget(psdDrawer_);
    psdLayerList_->setObjectName(QStringLiteral("psdLayers"));
    psdLayerList_->setFocusPolicy(Qt::NoFocus);
    psdLayerList_->setIconSize(QSize(38, 38));

    exportLayerBtn_ = new QPushButton(psdDrawer_);
    exportLayerBtn_->setObjectName(QStringLiteral("primaryBtn"));
    exportLayerBtn_->setFixedHeight(34);
    exportLayerBtn_->setFocusPolicy(Qt::NoFocus);
    exportLayerBtn_->setEnabled(false);

    psdDrawerLay->addWidget(psdHeader_);
    psdDrawerLay->addWidget(psdLayerList_, 1);
    psdDrawerLay->addWidget(exportLayerBtn_);

    midRow->addWidget(canvas_, 1);
    midRow->addWidget(psdDrawer_);
    root->addLayout(midRow, 1);

    // Filename, dimensions & zoom info centered above bottom dock
    metaLabel_ = new QLabel(central);
    metaLabel_->setAlignment(Qt::AlignCenter);
    metaLabel_->setStyleSheet(QStringLiteral("color: #E4E4E7; font-size: 12px; font-weight: 600; background: transparent;"));
    root->addWidget(metaLabel_);

    // Bottom Picasa Photo Viewer Dock (uniform 36px height on every button/badge + centered filmstrip)
    static constexpr int kDockH = 36;
    auto* dock = new QWidget(central);
    dock->setObjectName(QStringLiteral("viewerDock"));
    auto* dockLay = new QHBoxLayout(dock);
    dockLay->setContentsMargins(12, 8, 12, 8);
    dockLay->setSpacing(6);
    dockLay->setAlignment(Qt::AlignVCenter);

    auto makeIconBtn = [&](ViewerIconKind kind, int width = 36) {
        auto* b = new QPushButton(dock);
        b->setIcon(makeViewerIcon(kind, iconInk, 18));
        b->setIconSize(QSize(18, 18));
        b->setFixedSize(width, kDockH);
        b->setFocusPolicy(Qt::NoFocus);
        return b;
    };

    auto* zoomInBtn = makeIconBtn(ViewerIconKind::ZoomIn);
    zoomInBtn->setObjectName(QStringLiteral("zoomInBtn"));
    auto* zoomOutBtn = makeIconBtn(ViewerIconKind::ZoomOut);
    zoomOutBtn->setObjectName(QStringLiteral("zoomOutBtn"));

    zoomPctLabel_ = new QLabel(QStringLiteral("100%"), dock);
    zoomPctLabel_->setObjectName(QStringLiteral("zoomPctBadge"));
    zoomPctLabel_->setAlignment(Qt::AlignCenter);
    zoomPctLabel_->setFixedHeight(kDockH);
    zoomPctLabel_->setMinimumWidth(56);

    fitBtn_ = new QPushButton(QStringLiteral("1:1"), dock);
    fitBtn_->setFixedHeight(kDockH);
    fitBtn_->setFocusPolicy(Qt::NoFocus);

    bgBtn_ = new QPushButton(dock);
    bgBtn_->setIcon(makeViewerIcon(ViewerIconKind::Backdrop, iconInk, 16));
    bgBtn_->setIconSize(QSize(16, 16));
    bgBtn_->setFixedHeight(kDockH);
    bgBtn_->setCheckable(true);
    bgBtn_->setChecked(false);
    bgBtn_->setFocusPolicy(Qt::NoFocus);

    openEditorBtn_ = new QPushButton(dock);
    openEditorBtn_->setObjectName(QStringLiteral("primaryBtn"));
    openEditorBtn_->setFixedHeight(kDockH);
    openEditorBtn_->setFocusPolicy(Qt::NoFocus);

    exportBtn_ = makeIconBtn(ViewerIconKind::Export, kDockH);

    prevBtn_ = makeIconBtn(ViewerIconKind::Prev);
    nextBtn_ = makeIconBtn(ViewerIconKind::Next);

    rotLeftBtn_ = new QPushButton(QStringLiteral("-90°"), dock);
    rotLeftBtn_->setIcon(makeViewerIcon(ViewerIconKind::RotateLeft, iconInk, 18));
    rotLeftBtn_->setIconSize(QSize(18, 18));
    rotLeftBtn_->setFixedHeight(kDockH);
    rotLeftBtn_->setFocusPolicy(Qt::NoFocus);

    rotRightBtn_ = new QPushButton(QStringLiteral("+90°"), dock);
    rotRightBtn_->setIcon(makeViewerIcon(ViewerIconKind::RotateRight, iconInk, 18));
    rotRightBtn_->setIconSize(QSize(18, 18));
    rotRightBtn_->setFixedHeight(kDockH);
    rotRightBtn_->setFocusPolicy(Qt::NoFocus);

    starBtn_ = makeIconBtn(ViewerIconKind::StarOutline, kDockH);

    layersBtn_ = new QPushButton(dock);
    layersBtn_->setIcon(makeViewerIcon(ViewerIconKind::Layers, iconInk, 18));
    layersBtn_->setIconSize(QSize(18, 18));
    layersBtn_->setFixedHeight(kDockH);
    layersBtn_->setFocusPolicy(Qt::NoFocus);
    layersBtn_->setVisible(false);

    filmstrip_ = new QListWidget(dock);
    filmstrip_->setObjectName(QStringLiteral("miniStrip"));
    filmstrip_->setFocusPolicy(Qt::NoFocus);
    filmstrip_->setFlow(QListView::LeftToRight);
    filmstrip_->setViewMode(QListView::IconMode);
    filmstrip_->setWrapping(false);
    filmstrip_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    filmstrip_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    filmstrip_->setIconSize(QSize(44, 32));
    filmstrip_->setGridSize(QSize(52, 40));
    filmstrip_->setUniformItemSizes(true);
    filmstrip_->setFixedHeight(40);
    filmstrip_->setMinimumWidth(140);
    filmstrip_->setMaximumWidth(380);
    filmstrip_->setItemDelegate(new FilmstripDelegate(true, filmstrip_));

    dockLay->addWidget(zoomOutBtn, 0, Qt::AlignVCenter);
    dockLay->addWidget(zoomPctLabel_, 0, Qt::AlignVCenter);
    dockLay->addWidget(zoomInBtn, 0, Qt::AlignVCenter);
    dockLay->addWidget(fitBtn_, 0, Qt::AlignVCenter);
    dockLay->addWidget(bgBtn_, 0, Qt::AlignVCenter);
    dockLay->addWidget(openEditorBtn_, 0, Qt::AlignVCenter);
    dockLay->addWidget(exportBtn_, 0, Qt::AlignVCenter);
    dockLay->addSpacing(4);
    dockLay->addWidget(prevBtn_, 0, Qt::AlignVCenter);
    dockLay->addWidget(nextBtn_, 0, Qt::AlignVCenter);
    dockLay->addWidget(rotLeftBtn_, 0, Qt::AlignVCenter);
    dockLay->addWidget(rotRightBtn_, 0, Qt::AlignVCenter);
    dockLay->addWidget(starBtn_, 0, Qt::AlignVCenter);
    dockLay->addWidget(layersBtn_, 0, Qt::AlignVCenter);
    dockLay->addSpacing(6);
    dockLay->addWidget(filmstrip_, 1, Qt::AlignVCenter);

    auto* dockRow = new QHBoxLayout();
    dockRow->addStretch();
    dockRow->addWidget(dock);
    dockRow->addStretch();
    root->addLayout(dockRow);

    setCentralWidget(central);

    connect(closeBtn_, &QPushButton::clicked, this, &QWidget::close);
    connect(settingsBtn_, &QPushButton::clicked, this, [this]() {
        SettingsDialog dlg(this);
        dlg.exec();
    });
    connect(winModeBtn_, &QPushButton::clicked, this, [this]() {
        toggleWindowedMode(!windowedMode_);
    });
    // Clicking outside image NEVER closes QuickViewer — instead switches overlay to a movable window
    connect(canvas_, &ViewerCanvasWidget::backdropClicked, this, [this]() {
        if (!windowedMode_) {
            toggleWindowedMode(true);
        }
    });

    connect(canvas_, &ViewerCanvasWidget::zoomChanged, this, [this](double scale, bool fitMode) {
        const int pct = std::max(1, qRound(scale * 100.0));
        zoomPctLabel_->setText(QString("%1%").arg(pct));
        fitBtn_->setText(fitMode ? QStringLiteral("1:1") : trUi(QStringLiteral("Вписать"), QStringLiteral("Fit")));
        updateMetaAndZoomDisplay();
    });

    connect(zoomInBtn, &QPushButton::clicked, this, [this]() { canvas_->zoomBy(1.25); });
    connect(zoomOutBtn, &QPushButton::clicked, this, [this]() { canvas_->zoomBy(0.8); });
    connect(fitBtn_, &QPushButton::clicked, this, [this]() {
        if (rawImage_.isNull()) return;
        if (canvas_->isFitToWindow()) {
            canvas_->setActualSize();
        } else {
            canvas_->setFitToWindow(true);
        }
    });
    connect(bgBtn_, &QPushButton::clicked, this, &QuickViewerWindow::cycleBackdropMode);

    connect(prevBtn_, &QPushButton::clicked, this, [this]() {
        if (!siblingFiles_.isEmpty()) {
            openFileAt((currentIndex_ - 1 + siblingFiles_.size()) % siblingFiles_.size());
        }
    });
    connect(nextBtn_, &QPushButton::clicked, this, [this]() {
        if (!siblingFiles_.isEmpty()) {
            openFileAt((currentIndex_ + 1) % siblingFiles_.size());
        }
    });
    connect(rotLeftBtn_, &QPushButton::clicked, this, [this]() { rotateImage(270); });
    connect(rotRightBtn_, &QPushButton::clicked, this, [this]() { rotateImage(90); });
    connect(starBtn_, &QPushButton::clicked, this, &QuickViewerWindow::toggleStar);
    connect(layersBtn_, &QPushButton::clicked, this, [this]() {
        psdDrawer_->setVisible(!psdDrawer_->isVisible());
    });
    connect(psdLayerList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row <= 0) {
            activeLayerImage_ = QImage();
            exportLayerBtn_->setEnabled(false);
            updateRenderedImage(true);
            return;
        }
        const int layerIdx = row - 1;
        if (layerIdx >= 0 && layerIdx < psdLayerDetails_.size()) {
            const PsdLayerInfo& info = psdLayerDetails_[layerIdx];
            activeLayerImage_ = info.image;
            exportLayerBtn_->setEnabled(!info.image.isNull());
            if (!info.image.isNull()) {
                canvas_->setImage(info.image, true);
            }
        }
    });
    connect(exportLayerBtn_, &QPushButton::clicked, this, &QuickViewerWindow::exportSelectedPsdLayer);

    connect(openEditorBtn_, &QPushButton::clicked, this, [this]() {
        emit openInLibraryRequested(currentRecord_.filePath);
    });
    connect(exportBtn_, &QPushButton::clicked, this, &QuickViewerWindow::showExportFormatMenu);
    connect(filmstrip_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        const int row = filmstrip_->row(item);
        if (row >= 0 && row < siblingFiles_.size() && row != currentIndex_) {
            openFileAt(row);
        }
    });

    connect(&AppSettings::instance(), &AppSettings::languageChanged, this, [this](bool) {
        retranslateUi();
    });

    installSmoothButtons(this, true);
    retranslateUi();

    loadSiblingList(initialFilePath);
    populateMiniFilmstrip();
    openFileAt(currentIndex_);

    // Fit cleanly inside the desktop work area on Windows without clipping past taskbar or waiting for delayed maximize
    const QScreen* scr = QGuiApplication::screenAt(QCursor::pos());
    if (!scr) scr = QGuiApplication::primaryScreen();
    if (scr) {
        setGeometry(scr->availableGeometry());
    }
    show();
}

void QuickViewerWindow::toggleWindowedMode(bool windowed) {
    windowedMode_ = windowed;
    const QScreen* scr = screen() ? screen() : QGuiApplication::primaryScreen();
    const QRect screenGeom = scr ? scr->availableGeometry() : QRect(100, 100, 1280, 800);

    if (windowed) {
        showNormal();
        const int w = std::min(1140, screenGeom.width() - 80);
        const int h = std::min(780, screenGeom.height() - 80);
        resize(w, h);
        move(screenGeom.center() - QPoint(w / 2, h / 2));
        winModeBtn_->setToolTip(QStringLiteral("Развернуть на весь экран"));
        if (auto* rb = dynamic_cast<ViewerRoundButton*>(winModeBtn_)) {
            rb->setIconKind(ViewerIconKind::WindowExpand);
        }
    } else {
        showNormal();
        setGeometry(screenGeom);
        winModeBtn_->setToolTip(QStringLiteral("Свернуть в окно"));
        if (auto* rb = dynamic_cast<ViewerRoundButton*>(winModeBtn_)) {
            rb->setIconKind(ViewerIconKind::WindowShrink);
        }
    }
    QTimer::singleShot(0, this, [this]() {
        if (!rawImage_.isNull()) {
            canvas_->setFitToWindow(true);
        }
    });
    update();
}

void QuickViewerWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (windowedMode_) {
        p.setBrush(QColor(20, 22, 28, 244));
        p.setPen(QPen(QColor(255, 255, 255, 50), 1.5));
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 14, 14);
    } else {
        p.fillRect(rect(), QColor(14, 16, 20, 224));
    }
}

void QuickViewerWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    setFocus();
    QTimer::singleShot(0, this, [this]() {
        if (!rawImage_.isNull()) {
            canvas_->setFitToWindow(true);
        }
    });
}

void QuickViewerWindow::mousePressEvent(QMouseEvent* event) {
    if (windowedMode_ && event->button() == Qt::LeftButton) {
        draggingWindow_ = true;
        windowDragOffset_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QMainWindow::mousePressEvent(event);
}

void QuickViewerWindow::mouseMoveEvent(QMouseEvent* event) {
    if (draggingWindow_ && windowedMode_) {
        move(event->globalPosition().toPoint() - windowDragOffset_);
        event->accept();
        return;
    }
    QMainWindow::mouseMoveEvent(event);
}

void QuickViewerWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (draggingWindow_) {
        draggingWindow_ = false;
        event->accept();
        return;
    }
    QMainWindow::mouseReleaseEvent(event);
}

void QuickViewerWindow::loadSiblingList(const QString& currentFile) {
    const QFileInfo fi(currentFile);
    const QDir dir = fi.absoluteDir();
    const QString targetName = fi.fileName();

    siblingFiles_.clear();
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Readable, QDir::Name);
    for (const QFileInfo& entry : entries) {
        if (ImageLoader::isSupportedFile(entry.absoluteFilePath())) {
            siblingFiles_.append(entry.absoluteFilePath());
        }
    }

    currentIndex_ = 0;
    for (int i = 0; i < siblingFiles_.size(); ++i) {
        if (QFileInfo(siblingFiles_[i]).fileName().compare(targetName, Qt::CaseInsensitive) == 0) {
            currentIndex_ = i;
            break;
        }
    }
    if (siblingFiles_.isEmpty() && fi.exists()) {
        siblingFiles_.append(fi.absoluteFilePath());
    }
}

void QuickViewerWindow::populateMiniFilmstrip() {
    filmstrip_->clear();
    QPixmap blank(44, 34);
    blank.fill(QColor(45, 48, 56));
    const QIcon blankIcon(blank);

    for (int i = 0; i < siblingFiles_.size(); ++i) {
        auto* item = new QListWidgetItem();
        item->setIcon(blankIcon);
        item->setData(Qt::UserRole + 5, blank);
        item->setToolTip(QFileInfo(siblingFiles_[i]).fileName());
        filmstrip_->addItem(item);

        const QString filePath = siblingFiles_[i];
        auto* watcher = new QFutureWatcher<LoadedImageResult>(this);
        connect(watcher, &QFutureWatcher<LoadedImageResult>::finished, this, [this, watcher, i]() {
            const LoadedImageResult res = watcher->result();
            watcher->deleteLater();
            if (res.success && !res.image.isNull() && i < filmstrip_->count()) {
                const QPixmap pm = QPixmap::fromImage(res.image);
                filmstrip_->item(i)->setData(Qt::UserRole + 5, pm);
                filmstrip_->item(i)->setIcon(QIcon(pm));
            }
        });
        watcher->setFuture(QtConcurrent::run([filePath]() {
            return ImageLoader::loadImage(filePath, 96);
        }));
    }
}

void QuickViewerWindow::stopMovie() {
    if (movie_) {
        movie_->stop();
        movie_->deleteLater();
        movie_ = nullptr;
    }
    firstAnimFrame_ = false;
}

void QuickViewerWindow::openFileAt(int index) {
    if (index < 0 || index >= siblingFiles_.size()) return;
    currentIndex_ = index;
    const QString path = siblingFiles_[index];

    stopMovie();

    if (filmstrip_ && index < filmstrip_->count()) {
        const QSignalBlocker b(filmstrip_);
        filmstrip_->setCurrentRow(index);
        filmstrip_->scrollToItem(filmstrip_->item(index));
    }

    if (!QFileInfo::exists(path)) {
        db_->removePhotoFromLibrary(path);
    }
    currentRecord_ = db_->ensurePhotoIndexed(path);
    const LoadedImageResult res = ImageLoader::loadImage(path, 0);

    activeLayerImage_ = QImage();

    if (!res.success) {
        rawImage_ = QImage();
        currentMeta_ = ExifMetadata{};
        psdLayerDetails_.clear();
        currentIsPsd_ = false;
        currentIsAnimated_ = false;
        currentError_ = res.errorMessage;
        canvas_->setErrorText(res.errorMessage);
        updateMetaAndZoomDisplay();
        updateStarButton();
        psdLayerList_->clear();
        layersBtn_->setVisible(false);
        psdDrawer_->setVisible(false);
        return;
    }

    currentError_.clear();
    rawImage_ = res.image;
    currentMeta_ = res.metadata;
    psdLayerDetails_ = res.metadata.psdLayerDetails;
    currentIsPsd_ = res.isPsd;
    currentIsAnimated_ = res.isAnimated;

    updateStarButton();
    populatePsdLayers();
    updateRenderedImage(true);

    if (res.isAnimated) {
        movie_ = new QMovie(path, QByteArray(), this);
        movie_->setCacheMode(QMovie::CacheAll);
        if (movie_->isValid() && movie_->frameCount() > 1) {
            firstAnimFrame_ = true;
            connect(movie_, &QMovie::frameChanged, this, [this](int) {
                if (!movie_ || !activeLayerImage_.isNull()) return;
                const QImage frame = movie_->currentImage().convertToFormat(QImage::Format_RGBA8888);
                if (frame.isNull()) return;
                rawImage_ = frame;
                const QImage edited = currentRecord_.edits.isDefault()
                                          ? frame
                                          : ImageLoader::applyEdits(frame, currentRecord_.edits);
                canvas_->setImage(edited, firstAnimFrame_);
                firstAnimFrame_ = false;
            });
            movie_->start();
        } else {
            stopMovie();
        }
    }
}

void QuickViewerWindow::populatePsdLayers() {
    const QSignalBlocker blocker(psdLayerList_);
    psdLayerList_->clear();
    exportLayerBtn_->setEnabled(false);

    if (!currentIsPsd_ || psdLayerDetails_.isEmpty()) {
        layersBtn_->setVisible(false);
        psdDrawer_->setVisible(false);
        return;
    }

    layersBtn_->setVisible(true);

    auto* compItem = new QListWidgetItem(QIcon(QPixmap::fromImage(rawImage_.scaled(38, 38, Qt::KeepAspectRatio, Qt::SmoothTransformation))),
                                         QStringLiteral("Объединённый вид (Композит)"));
    psdLayerList_->addItem(compItem);

    for (const PsdLayerInfo& layer : psdLayerDetails_) {
        QPixmap thumb(38, 38);
        thumb.fill(QColor(48, 52, 62));
        if (!layer.image.isNull()) {
            QPixmap scaled = QPixmap::fromImage(layer.image.scaled(38, 38, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            QPainter p(&thumb);
            p.drawPixmap((38 - scaled.width()) / 2, (38 - scaled.height()) / 2, scaled);
        }
        const QString visTag = layer.visible ? QString() : QStringLiteral(" [скрыт]");
        const QString dimTag = layer.image.isNull()
                                   ? QStringLiteral(" (группа/текст)")
                                   : QString(" (%1×%2)").arg(layer.image.width()).arg(layer.image.height());
        psdLayerList_->addItem(new QListWidgetItem(QIcon(thumb), layer.name + dimTag + visTag));
    }
    psdLayerList_->setCurrentRow(0);
}

void QuickViewerWindow::exportSelectedPsdLayer() {
    if (activeLayerImage_.isNull()) return;
    const int row = psdLayerList_->currentRow();
    const QString layerName = (row > 0 && row - 1 < psdLayerDetails_.size())
                                  ? psdLayerDetails_[row - 1].name
                                  : QStringLiteral("layer");
    const QFileInfo srcFi(currentRecord_.filePath);
    const QString defaultPath = srcFi.absolutePath() + "/" + srcFi.completeBaseName() + "_" + layerName + ".png";
    QString outPath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Экспорт слоя PSD"),
        defaultPath,
        QStringLiteral("PNG с прозрачностью (*.png)")
    );
    if (outPath.isEmpty()) return;
    if (QFileInfo(outPath).suffix().isEmpty()) {
        outPath += QStringLiteral(".png");
    }

    QImageWriter writer(outPath, "PNG");
    if (writer.write(activeLayerImage_)) {
        exportLayerBtn_->setText(QStringLiteral("Сохранено ✓"));
        QTimer::singleShot(1800, exportLayerBtn_, [this]() {
            if (exportLayerBtn_) exportLayerBtn_->setText(QStringLiteral("Экспортировать слой PNG"));
        });
    } else {
        QMessageBox::warning(this, QStringLiteral("Ошибка"),
                             QStringLiteral("Не удалось сохранить слой:\n") + writer.errorString());
    }
}

void QuickViewerWindow::showExportFormatMenu() {
    if (rawImage_.isNull()) return;
    QMenu menu(this);
    if (currentIsAnimated_) {
        QAction* gifAnimAct = menu.addAction(trUi(QStringLiteral("GIF (анимация)..."), QStringLiteral("GIF (animation)...")));
        connect(gifAnimAct, &QAction::triggered, this, [this]() { exportViewerImage(QStringLiteral("gif")); });
        menu.addSeparator();
    }
    struct FmtChoice { QString label; QString ext; };
    const QList<FmtChoice> fmts = {
        {QStringLiteral("PNG (*.png)"), QStringLiteral("png")},
        {QStringLiteral("JPEG (*.jpg)"), QStringLiteral("jpg")},
        {QStringLiteral("WebP (*.webp)"), QStringLiteral("webp")},
        {QStringLiteral("BMP (*.bmp)"), QStringLiteral("bmp")},
        {QStringLiteral("TIFF (*.tif)"), QStringLiteral("tif")},
        {QStringLiteral("GIF (*.gif)"), QStringLiteral("gif")}
    };
    for (const auto& f : fmts) {
        if (currentIsAnimated_ && f.ext == QLatin1String("gif")) continue;
        QAction* act = menu.addAction(f.label);
        const QString ext = f.ext;
        connect(act, &QAction::triggered, this, [this, ext]() { exportViewerImage(ext); });
    }
    const QPoint pos = exportBtn_->mapToGlobal(QPoint(0, 0));
    menu.exec(QPoint(pos.x(), pos.y() - menu.sizeHint().height() - 4));
}

void QuickViewerWindow::exportViewerImage(const QString& forcedExt) {
    if (rawImage_.isNull()) return;
    const QFileInfo srcFi(currentRecord_.filePath);
    const QString srcSuffix = srcFi.suffix().toLower();
    QString defaultExt = forcedExt.toLower();
    if (defaultExt.isEmpty()) {
        if (currentIsAnimated_ || srcSuffix == QLatin1String("gif")) {
            defaultExt = QStringLiteral("gif");
        } else if (srcSuffix == QLatin1String("png") || rawImage_.hasAlphaChannel()) {
            defaultExt = QStringLiteral("png");
        } else if (srcSuffix == QLatin1String("webp")) {
            defaultExt = QStringLiteral("webp");
        } else {
            defaultExt = QStringLiteral("jpg");
        }
    }

    const QString fGif = QStringLiteral("GIF (*.gif)");
    const QString fPng = QStringLiteral("PNG (*.png)");
    const QString fJpg = QStringLiteral("JPEG (*.jpg *.jpeg)");
    const QString fWebp = QStringLiteral("WebP (*.webp)");
    const QString fBmp = QStringLiteral("BMP (*.bmp)");
    const QString fTiff = QStringLiteral("TIFF (*.tif *.tiff)");
    const QString allFilters = QStringList({fGif, fPng, fJpg, fWebp, fBmp, fTiff}).join(QStringLiteral(";;"));

    QString selectedFilter = fJpg;
    if (defaultExt == QLatin1String("gif")) selectedFilter = fGif;
    else if (defaultExt == QLatin1String("png")) selectedFilter = fPng;
    else if (defaultExt == QLatin1String("webp")) selectedFilter = fWebp;
    else if (defaultExt == QLatin1String("bmp")) selectedFilter = fBmp;
    else if (defaultExt == QLatin1String("tif") || defaultExt == QLatin1String("tiff")) selectedFilter = fTiff;

    const QString defaultPath = srcFi.absolutePath() + "/" + srcFi.completeBaseName() + "_export." + defaultExt;
    QString outPath = QFileDialog::getSaveFileName(
        this,
        trUi(QStringLiteral("Экспорт / конвертация изображения"), QStringLiteral("Export / convert image")),
        defaultPath,
        allFilters,
        &selectedFilter
    );
    if (outPath.isEmpty()) return;
    if (QFileInfo(outPath).suffix().isEmpty()) {
        outPath += "." + defaultExt;
    }

    const QString outExt = QFileInfo(outPath).suffix().toLower();
    const bool isViewingSingleLayer = !activeLayerImage_.isNull();
    const QImage staticImg = isViewingSingleLayer
                                 ? activeLayerImage_
                                 : ImageLoader::applyEdits(rawImage_, currentRecord_.edits);

    if (outExt == QLatin1String("gif") && currentIsAnimated_ && !isViewingSingleLayer) {
        auto* dlg = new QProgressDialog(
            trUi(QStringLiteral("Экспорт анимированного GIF..."), QStringLiteral("Exporting animated GIF...")),
            trUi(QStringLiteral("Отмена"), QStringLiteral("Cancel")),
            0, 100, this);
        dlg->setWindowModality(Qt::WindowModal);
        dlg->setMinimumDuration(120);

        auto cancelled = std::make_shared<std::atomic_bool>(false);
        connect(dlg, &QProgressDialog::canceled, this, [cancelled]() {
            cancelled->store(true);
        });

        QPointer<QProgressDialog> safeDlg(dlg);
        const QString srcPath = currentRecord_.filePath;
        const PhotoEdits edits = currentRecord_.edits;
        auto* watcher = new QFutureWatcher<QString>(this);
        connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, safeDlg, cancelled, outPath]() {
            const QString err = watcher->result();
            watcher->deleteLater();
            if (safeDlg) {
                safeDlg->close();
                safeDlg->deleteLater();
            }
            if (cancelled->load()) return;
            if (!err.isEmpty()) {
                QMessageBox::warning(this,
                                     trUi(QStringLiteral("Ошибка экспорта"), QStringLiteral("Export error")),
                                     err);
            }
        });

        watcher->setFuture(QtConcurrent::run([srcPath, outPath, edits, staticImg, cancelled, safeDlg]() -> QString {
            QString err;
            ImageLoader::saveAnimatedGif(
                srcPath,
                outPath,
                edits,
                staticImg,
                [safeDlg, cancelled](int done, int total) -> bool {
                    if (cancelled->load()) return false;
                    const int pct = (total > 0) ? std::clamp((done * 100) / total, 0, 100) : 0;
                    if (safeDlg) {
                        QMetaObject::invokeMethod(safeDlg.data(), [safeDlg, pct]() {
                            if (safeDlg) safeDlg->setValue(pct);
                        }, Qt::QueuedConnection);
                    }
                    return true;
                },
                &err);
            return err;
        }));
        return;
    }

    QByteArray fmt = "JPEG";
    if (outExt == QLatin1String("png")) fmt = "PNG";
    else if (outExt == QLatin1String("webp")) fmt = "WEBP";
    else if (outExt == QLatin1String("bmp")) fmt = "BMP";
    else if (outExt == QLatin1String("tif") || outExt == QLatin1String("tiff")) fmt = "TIFF";
    else if (outExt == QLatin1String("gif")) fmt = "GIF";

    QString err;
    if (!ImageLoader::saveEditedImage(currentRecord_.filePath,
                                      outPath,
                                      fmt,
                                      staticImg,
                                      false,
                                      &currentMeta_,
                                      &err)) {
        QMessageBox::warning(this,
                             trUi(QStringLiteral("Ошибка экспорта"), QStringLiteral("Export error")),
                             err.isEmpty() ? trUi(QStringLiteral("Не удалось сохранить файл."), QStringLiteral("Failed to save file.")) : err);
    }
}

void QuickViewerWindow::updateRenderedImage(bool resetView) {
    if (!activeLayerImage_.isNull()) {
        canvas_->setImage(activeLayerImage_, resetView);
        updateMetaAndZoomDisplay();
        return;
    }
    if (rawImage_.isNull()) return;
    const QImage edited = ImageLoader::applyEdits(rawImage_, currentRecord_.edits);
    canvas_->setImage(edited, resetView);
    updateMetaAndZoomDisplay();
}

void QuickViewerWindow::updateMetaAndZoomDisplay() {
    const QString displayName = currentRecord_.fileName.isEmpty()
                                    ? QFileInfo(currentRecord_.filePath).fileName()
                                    : currentRecord_.fileName;
    if (!currentError_.isEmpty() || rawImage_.isNull()) {
        metaLabel_->setText(QString("%1  ·  Ошибка загрузки  ·  %2 / %3")
                                .arg(displayName)
                                .arg(currentIndex_ + 1)
                                .arg(siblingFiles_.size()));
        return;
    }

    const int pct = std::max(1, qRound(canvas_->effectiveScale() * 100.0));
    metaLabel_->setText(QString("%1  (%2 × %3)  ·  %4%  ·  %5 / %6%7")
                            .arg(displayName)
                            .arg(rawImage_.width())
                            .arg(rawImage_.height())
                            .arg(pct)
                            .arg(currentIndex_ + 1)
                            .arg(siblingFiles_.size())
                            .arg(currentIsPsd_ ? "  ·  PSD" : ""));
}

void QuickViewerWindow::updateStarButton() {
    if (currentRecord_.starred) {
        starBtn_->setIcon(makeViewerIcon(ViewerIconKind::StarFilled, QColor(0xFA, 0xCC, 0x15), 18));
        starBtn_->setObjectName(QStringLiteral("starBtnActive"));
    } else {
        starBtn_->setIcon(makeViewerIcon(ViewerIconKind::StarOutline, QColor(0xF4, 0xF4, 0xF5), 18));
        starBtn_->setObjectName(QString());
    }
    starBtn_->update();
}

void QuickViewerWindow::updateBackdropButton() {
    const CanvasBackdropMode m = canvas_->backdropMode();
    if (m == CanvasBackdropMode::White) {
        bgBtn_->setChecked(true);
        bgBtn_->setText(trUi(QStringLiteral("Фон: Белый"), QStringLiteral("Bg: White")));
    } else if (m == CanvasBackdropMode::Black) {
        bgBtn_->setChecked(true);
        bgBtn_->setText(trUi(QStringLiteral("Фон: Чёрный"), QStringLiteral("Bg: Black")));
    } else {
        bgBtn_->setChecked(false);
        bgBtn_->setText(trUi(QStringLiteral("Фон: Выкл"), QStringLiteral("Bg: Off")));
    }
}

void QuickViewerWindow::retranslateUi() {
    setWindowTitle(trUi(QStringLiteral("Lumina — Просмотр"), QStringLiteral("Lumina — Viewer")));
    if (settingsBtn_) settingsBtn_->setToolTip(trUi(QStringLiteral("Настройки (тема и язык)"), QStringLiteral("Settings (theme & language)")));
    if (winModeBtn_) {
        winModeBtn_->setToolTip(windowedMode_
                                    ? trUi(QStringLiteral("Развернуть на весь экран"), QStringLiteral("Enter fullscreen"))
                                    : trUi(QStringLiteral("Свернуть в окно"), QStringLiteral("Windowed mode")));
    }
    if (closeBtn_) closeBtn_->setToolTip(trUi(QStringLiteral("Закрыть (Esc)"), QStringLiteral("Close (Esc)")));
    if (psdHeader_) psdHeader_->setText(trUi(QStringLiteral("Слои PSD (клик для просмотра)"), QStringLiteral("PSD Layers (click to preview)")));
    if (exportLayerBtn_) exportLayerBtn_->setText(trUi(QStringLiteral("Экспортировать слой PNG"), QStringLiteral("Export PNG Layer")));
    if (auto* zi = findChild<QPushButton*>(QStringLiteral("zoomInBtn"))) zi->setToolTip(trUi(QStringLiteral("Увеличить (+ / ↑)"), QStringLiteral("Zoom in (+ / ↑)")));
    if (auto* zo = findChild<QPushButton*>(QStringLiteral("zoomOutBtn"))) zo->setToolTip(trUi(QStringLiteral("Уменьшить (− / ↓)"), QStringLiteral("Zoom out (− / ↓)")));
    if (fitBtn_) {
        fitBtn_->setToolTip(trUi(QStringLiteral("Переключить масштаб 100% / Вписать в экран"), QStringLiteral("Toggle 100% / Fit to screen")));
        fitBtn_->setText(canvas_->isFitToWindow() ? QStringLiteral("1:1") : trUi(QStringLiteral("Вписать"), QStringLiteral("Fit")));
    }
    if (bgBtn_) {
        bgBtn_->setToolTip(trUi(QStringLiteral("Подложка для прозрачных PNG / SVG / ICO (Выкл → Белый → Чёрный)"),
                                QStringLiteral("Backdrop for transparent PNG / SVG / ICO (Off → White → Black)")));
    }
    if (openEditorBtn_) openEditorBtn_->setText(trUi(QStringLiteral("Открыть в Lumina"), QStringLiteral("Open in Lumina")));
    if (exportBtn_) exportBtn_->setToolTip(trUi(QStringLiteral("Экспорт / конвертация в другой формат"), QStringLiteral("Export / convert to another format")));
    if (prevBtn_) prevBtn_->setToolTip(trUi(QStringLiteral("Предыдущее фото (←)"), QStringLiteral("Previous photo (←)")));
    if (nextBtn_) nextBtn_->setToolTip(trUi(QStringLiteral("Следующее фото (→)"), QStringLiteral("Next photo (→)")));
    if (rotLeftBtn_) rotLeftBtn_->setToolTip(trUi(QStringLiteral("Повернуть влево на 90°"), QStringLiteral("Rotate 90° left")));
    if (rotRightBtn_) rotRightBtn_->setToolTip(trUi(QStringLiteral("Повернуть вправо на 90° (R)"), QStringLiteral("Rotate 90° right (R)")));
    if (starBtn_) starBtn_->setToolTip(trUi(QStringLiteral("Избранное (S)"), QStringLiteral("Favorite (S)")));
    if (layersBtn_) layersBtn_->setText(trUi(QStringLiteral("Слои PSD"), QStringLiteral("PSD Layers")));
    updateBackdropButton();
    updateMetaAndZoomDisplay();
}

void QuickViewerWindow::cycleBackdropMode() {
    const CanvasBackdropMode cur = canvas_->backdropMode();
    CanvasBackdropMode next = CanvasBackdropMode::White;
    if (cur == CanvasBackdropMode::White) {
        next = CanvasBackdropMode::Black;
    } else if (cur == CanvasBackdropMode::Black) {
        next = CanvasBackdropMode::None;
    }
    canvas_->setBackdropMode(next);
    updateBackdropButton();
}

void QuickViewerWindow::toggleStar() {
    currentRecord_.starred = !currentRecord_.starred;
    db_->setStarred(currentRecord_.filePath, currentRecord_.starred);
    updateStarButton();
}

void QuickViewerWindow::rotateImage(int deltaDeg) {
    currentRecord_.edits.rotationDeg = (currentRecord_.edits.rotationDeg + deltaDeg) % 360;
    db_->setEdits(currentRecord_.filePath, currentRecord_.edits);
    updateRenderedImage(false);
}

void QuickViewerWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        close();
    } else if (event->key() == Qt::Key_Right && !siblingFiles_.isEmpty()) {
        openFileAt((currentIndex_ + 1) % siblingFiles_.size());
    } else if (event->key() == Qt::Key_Left && !siblingFiles_.isEmpty()) {
        openFileAt((currentIndex_ - 1 + siblingFiles_.size()) % siblingFiles_.size());
    } else if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
        canvas_->zoomBy(1.20);
    } else if (event->key() == Qt::Key_Down || event->key() == Qt::Key_Minus) {
        canvas_->zoomBy(1.0 / 1.20);
    } else if (event->key() == Qt::Key_S) {
        toggleStar();
    } else if (event->key() == Qt::Key_R) {
        rotateImage(90);
    } else if (event->key() == Qt::Key_0) {
        canvas_->setFitToWindow(true);
    } else if (event->key() == Qt::Key_1) {
        canvas_->setActualSize();
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

} // namespace lumina
