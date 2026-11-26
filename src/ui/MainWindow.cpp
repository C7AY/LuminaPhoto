#include "MainWindow.h"
#include "AlbumIcons.h"
#include "FileAssociationsDialog.h"
#include "SettingsDialog.h"
#include "SmoothUi.h"
#include "../core/AppSettings.h"
#include "../core/ImageLoader.h"
#include "../core/WindowsFileAssociation.h"

#include <QApplication>
#include <QClipboard>
#include <QColorDialog>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QImageWriter>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
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
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTabBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtConcurrent>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>

namespace lumina {

namespace {

bool layerListsEqual(const QList<EffectLayer>& a, const QList<EffectLayer>& b) {
    if (a.size() != b.size()) return false;
    for (int i = 0; i < a.size(); ++i) {
        if (a[i].id != b[i].id || a[i].preset != b[i].preset ||
            a[i].intensity != b[i].intensity || a[i].param2 != b[i].param2 ||
            a[i].enabled != b[i].enabled || a[i].text != b[i].text ||
            !qFuzzyCompare(1.0 + a[i].posX, 1.0 + b[i].posX) ||
            !qFuzzyCompare(1.0 + a[i].posY, 1.0 + b[i].posY) ||
            a[i].textSize != b[i].textSize || a[i].textColor != b[i].textColor) {
            return false;
        }
    }
    return true;
}

bool editsEqual(const PhotoEdits& a, const PhotoEdits& b) {
    return a.rotationDeg == b.rotationDeg &&
           qFuzzyCompare(1.0 + a.straightenDeg, 1.0 + b.straightenDeg) &&
           qFuzzyCompare(1.0 + a.cropLeft, 1.0 + b.cropLeft) &&
           qFuzzyCompare(1.0 + a.cropTop, 1.0 + b.cropTop) &&
           qFuzzyCompare(1.0 + a.cropRight, 1.0 + b.cropRight) &&
           qFuzzyCompare(1.0 + a.cropBottom, 1.0 + b.cropBottom) &&
           a.cropZoom == b.cropZoom &&
           a.sharpen == b.sharpen &&
           a.basicEnabled == b.basicEnabled &&
           a.cropEnabled == b.cropEnabled &&
           a.brightness == b.brightness &&
           a.highlights == b.highlights &&
           a.shadows == b.shadows &&
           a.contrast == b.contrast &&
           a.saturation == b.saturation &&
           a.temperature == b.temperature &&
           a.tint == b.tint &&
           a.hueShift == b.hueShift &&
           a.lightColorEnabled == b.lightColorEnabled &&
           a.overlayText == b.overlayText &&
           layerListsEqual(a.effectLayers, b.effectLayers);
}

void drawUnifiedStarPolygon(QPainter& p, QPointF center, double outerR, bool filled, const QColor& strokeCol, const QColor& fillCol) {
    QPolygonF star;
    constexpr double pi = 3.14159265358979323846;
    const double innerR = outerR * 0.43;
    for (int i = 0; i < 10; ++i) {
        const double r = (i % 2 == 0) ? outerR : innerR;
        const double ang = -pi / 2.0 + i * (pi / 5.0);
        star << QPointF(center.x() + r * std::cos(ang), center.y() + r * std::sin(ang));
    }
    p.setPen(QPen(strokeCol, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(filled ? QBrush(fillCol) : Qt::NoBrush);
    p.drawPolygon(star);
}

QIcon makeUnifiedStarIcon(bool filled, int size = 18) {
    QPixmap pm(size * 2, size * 2);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor gold(0xF5, 0x9E, 0x0B);
    const QColor goldFill(0xFA, 0xCC, 0x15);
    const QColor muted(0x64, 0x74, 0x8B);
    drawUnifiedStarPolygon(p, QPointF(size * 0.5, size * 0.52), size * 0.37,
                           filled, filled ? gold : muted, goldFill);
    return QIcon(pm);
}

enum class UiIconKind {
    BackArrow,
    ChevronLeft,
    ChevronRight,
    Undo,
    RotateLeft,
    RotateRight,
    Folder,
    Album,
    AllPhotos,
    Plus,
    Copy,
    Trash,
    Settings
};

QIcon makeUiIcon(UiIconKind kind, const QColor& color, int size = 18) {
    QPixmap pm(size * 2, size * 2);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    const double s = static_cast<double>(size);
    switch (kind) {
        case UiIconKind::BackArrow: {
            p.drawLine(QPointF(s * 0.78, s * 0.50), QPointF(s * 0.24, s * 0.50));
            QPainterPath head;
            head.moveTo(s * 0.46, s * 0.28);
            head.lineTo(s * 0.22, s * 0.50);
            head.lineTo(s * 0.46, s * 0.72);
            p.drawPath(head);
            break;
        }
        case UiIconKind::ChevronLeft: {
            QPainterPath path;
            path.moveTo(s * 0.64, s * 0.24);
            path.lineTo(s * 0.34, s * 0.50);
            path.lineTo(s * 0.64, s * 0.76);
            p.drawPath(path);
            break;
        }
        case UiIconKind::ChevronRight: {
            QPainterPath path;
            path.moveTo(s * 0.36, s * 0.24);
            path.lineTo(s * 0.66, s * 0.50);
            path.lineTo(s * 0.36, s * 0.76);
            p.drawPath(path);
            break;
        }
        case UiIconKind::Undo: {
            p.drawArc(QRectF(s * 0.22, s * 0.26, s * 0.58, s * 0.54), 135 * 16, -210 * 16);
            QPolygonF head;
            head << QPointF(s * 0.20, s * 0.46) << QPointF(s * 0.30, s * 0.20) << QPointF(s * 0.44, s * 0.40);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawPolygon(head);
            break;
        }
        case UiIconKind::RotateLeft: {
            p.drawArc(QRectF(s * 0.20, s * 0.24, s * 0.62, s * 0.62), 90 * 16, -245 * 16);
            QPolygonF head;
            head << QPointF(s * 0.50, s * 0.10) << QPointF(s * 0.26, s * 0.25) << QPointF(s * 0.50, s * 0.40);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawPolygon(head);
            break;
        }
        case UiIconKind::RotateRight: {
            p.drawArc(QRectF(s * 0.18, s * 0.24, s * 0.62, s * 0.62), 90 * 16, 245 * 16);
            QPolygonF head;
            head << QPointF(s * 0.50, s * 0.10) << QPointF(s * 0.74, s * 0.25) << QPointF(s * 0.50, s * 0.40);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawPolygon(head);
            break;
        }
        case UiIconKind::AllPhotos: {
            p.drawRoundedRect(QRectF(s * 0.16, s * 0.20, s * 0.68, s * 0.60), 2.0, 2.0);
            p.drawLine(QPointF(s * 0.24, s * 0.66), QPointF(s * 0.44, s * 0.46));
            p.drawLine(QPointF(s * 0.44, s * 0.46), QPointF(s * 0.64, s * 0.66));
            break;
        }
        case UiIconKind::Album: {
            p.drawRoundedRect(QRectF(s * 0.20, s * 0.16, s * 0.60, s * 0.68), 2.0, 2.0);
            p.drawLine(QPointF(s * 0.35, s * 0.16), QPointF(s * 0.35, s * 0.84));
            break;
        }
        case UiIconKind::Folder: {
            QPainterPath folder;
            folder.moveTo(s * 0.14, s * 0.25);
            folder.lineTo(s * 0.42, s * 0.25);
            folder.lineTo(s * 0.52, s * 0.34);
            folder.lineTo(s * 0.86, s * 0.34);
            folder.lineTo(s * 0.86, s * 0.76);
            folder.lineTo(s * 0.14, s * 0.76);
            folder.closeSubpath();
            p.drawPath(folder);
            break;
        }
        case UiIconKind::Plus: {
            p.drawLine(QPointF(s * 0.50, s * 0.24), QPointF(s * 0.50, s * 0.76));
            p.drawLine(QPointF(s * 0.24, s * 0.50), QPointF(s * 0.76, s * 0.50));
            break;
        }
        case UiIconKind::Copy: {
            p.drawRoundedRect(QRectF(s * 0.34, s * 0.34, s * 0.48, s * 0.48), 1.5, 1.5);
            p.drawRoundedRect(QRectF(s * 0.18, s * 0.18, s * 0.48, s * 0.48), 1.5, 1.5);
            break;
        }
        case UiIconKind::Trash: {
            p.drawLine(QPointF(s * 0.22, s * 0.30), QPointF(s * 0.78, s * 0.30));
            p.drawLine(QPointF(s * 0.38, s * 0.20), QPointF(s * 0.62, s * 0.20));
            p.drawRoundedRect(QRectF(s * 0.28, s * 0.30, s * 0.44, s * 0.52), 1.5, 1.5);
            break;
        }
        case UiIconKind::Settings: {
            const double cx = s * 0.50;
            const double cy = s * 0.50;
            const double rOuter = s * 0.38;
            const double rInner = s * 0.26;
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
    }
    return QIcon(pm);
}

QIcon makeSidebarThumbIcon(const QImage& img,
                             const QColor& fallbackColor,
                             UiIconKind fallbackKind,
                             const QString& albumIconKey = QString()) {
    QPixmap pm(56, 44);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (!img.isNull()) {
        const QImage scaled = img.scaled(52, 42, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const QImage crop = scaled.copy(
            std::max(0, (scaled.width() - 52) / 2),
            std::max(0, (scaled.height() - 42) / 2),
            std::min(52, scaled.width()),
            std::min(42, scaled.height()));
        const QRectF thumbBox(0.5, 0.5, 25.0, 20.5);
        QPainterPath clip;
        clip.addRoundedRect(thumbBox, 4.0, 4.0);
        p.setClipPath(clip);
        p.drawImage(thumbBox, crop);
        p.setClipping(false);
        p.setPen(QPen(QColor(0xCB, 0xD5, 0xE1), 1.0));
        p.drawRoundedRect(thumbBox, 4.0, 4.0);

        if (!albumIconKey.isEmpty()) {
            drawAlbumPresetIcon(p, QRectF(14.0, 8.0, 13.5, 13.5), albumIconKey,
                                QColor(0x7E, 0x22, 0xCE), QColor(0xC0, 0x84, 0xFC), true);
        }
        return QIcon(pm);
    }

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(fallbackColor.red(), fallbackColor.green(), fallbackColor.blue(), 26));
    p.drawRoundedRect(QRectF(1.0, 0.5, 26.0, 21.0), 4.0, 4.0);
    if (!albumIconKey.isEmpty()) {
        drawAlbumPresetIcon(p, QRectF(5.5, 2.5, 17.0, 17.0), albumIconKey);
    } else {
        const QIcon vec = makeUiIcon(fallbackKind, fallbackColor, 16);
        vec.paint(&p, QRect(6, 3, 16, 16));
    }
    return QIcon(pm);
}

bool effectUsesParam2(EffectPreset preset) {
    return preset == EffectPreset::ColorTint ||
           preset == EffectPreset::BwFilter ||
           preset == EffectPreset::GradTint ||
           preset == EffectPreset::DuoTone;
}

QString effectParam2Label(EffectPreset preset) {
    switch (preset) {
        case EffectPreset::BwFilter:
            return trUi(QStringLiteral("Фильтр канала (Красн ↔ Син)"), QStringLiteral("Channel filter (Red ↔ Blue)"));
        case EffectPreset::ColorTint:
        case EffectPreset::GradTint:
        case EffectPreset::DuoTone:
            return trUi(QStringLiteral("Выбор оттенка / цвета"), QStringLiteral("Hue / Color choice"));
        default:
            return trUi(QStringLiteral("Тон параметра"), QStringLiteral("Parameter tone"));
    }
}

} // namespace

// ==================== PHOTO CARD DELEGATE & INTERACTIVE GRID ====================
PhotoCardDelegate::PhotoCardDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QRect PhotoCardDelegate::cardRect(const QRect& itemRect) {
    return itemRect.adjusted(5, 5, -5, -5);
}

QRect PhotoCardDelegate::thumbRect(const QRect& itemRect) {
    const QRect cardR = cardRect(itemRect);
    return QRect(cardR.left() + 6, cardR.top() + 6,
                 std::max(24, cardR.width() - 12),
                 std::max(24, cardR.height() - 32));
}

QRect PhotoCardDelegate::plusButtonRect(const QRect& itemRect) {
    const QRect tr = thumbRect(itemRect);
    return QRect(tr.left() + 6, tr.top() + 6, 26, 26);
}

QRect PhotoCardDelegate::starButtonRect(const QRect& itemRect) {
    const QRect tr = thumbRect(itemRect);
    return QRect(tr.right() - 31, tr.top() + 6, 26, 26);
}

QSize PhotoCardDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const {
    if (const auto* view = qobject_cast<const QAbstractItemView*>(option.widget)) {
        const QSize ic = view->iconSize();
        if (ic.isValid() && !ic.isEmpty()) {
            return QSize(ic.width() + 22, ic.height() + 44);
        }
    }
    return QSize(198, 176);
}

void PhotoCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    const bool dark = AppSettings::instance().effectiveDarkTheme();

    // Clear full item cell with grid background so partial hover updates never leave stale artifacts
    painter->fillRect(option.rect, dark ? QColor(0x11, 0x13, 0x18) : QColor(0xF1, 0xF5, 0xF9));

    const QRect cardR = cardRect(option.rect);
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;

    if (selected) {
        painter->setPen(QPen(QColor(0xA8, 0x55, 0xF7), 1.8));
        painter->setBrush(dark ? QColor(0x2E, 0x1B, 0x4E) : QColor(0xF3, 0xE8, 0xFF));
        painter->drawRoundedRect(cardR, 10, 10);
    } else if (hovered) {
        painter->setPen(QPen(dark ? QColor(0x4C, 0x1D, 0x95) : QColor(0xD8, 0xB4, 0xFE), 1.2));
        painter->setBrush(dark ? QColor(0x1D, 0x20, 0x2B) : QColor(0xFF, 0xFF, 0xFF));
        painter->drawRoundedRect(cardR, 10, 10);
    } else {
        painter->setPen(QPen(dark ? QColor(0x27, 0x2B, 0x38) : QColor(0xE2, 0xE8, 0xF0), 1.0));
        painter->setBrush(dark ? QColor(0x18, 0x1B, 0x24) : QColor(0xFF, 0xFF, 0xFF));
        painter->drawRoundedRect(cardR, 10, 10);
    }

    const QRect thumbR = thumbRect(option.rect);

    QPainterPath clip;
    clip.addRoundedRect(QRectF(thumbR), 7.0, 7.0);
    painter->save();
    painter->setClipPath(clip);
    painter->fillRect(thumbR, dark ? QColor(0x22, 0x25, 0x31) : QColor(0xE2, 0xE8, 0xF0));

    QPixmap pm = index.data(ThumbPixmapRole).value<QPixmap>();
    if (pm.isNull()) {
        const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        if (!icon.isNull()) {
            pm = icon.pixmap(thumbR.size());
        }
    }
    if (!pm.isNull()) {
        const QSize fitSz = pm.size().scaled(thumbR.size(), Qt::KeepAspectRatio);
        const QRect dst(thumbR.left() + (thumbR.width() - fitSz.width()) / 2,
                        thumbR.top() + (thumbR.height() - fitSz.height()) / 2,
                        fitSz.width(), fitSz.height());
        painter->drawPixmap(dst, pm);
    }
    painter->restore();

    painter->setPen(QPen(dark ? QColor(0x33, 0x38, 0x48) : QColor(0xD4, 0xD4, 0xD8), 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(thumbR, 7, 7);

    // Top-left '+' Add to Album circular button (exact centered vector coordinates)
    const auto* grid = qobject_cast<const PhotoGridWidget*>(option.widget);
    const bool isHoveredRow = grid && (grid->hoveredRow() == index.row());
    const int hoverBtn = isHoveredRow ? grid->hoveredBtn() : 0;

    const QRect plusR = plusButtonRect(option.rect);
    const QRectF plusRf = QRectF(plusR).adjusted(0.5, 0.5, -0.5, -0.5);
    painter->setPen(QPen(QColor(255, 255, 255, 115), 1.0));
    painter->setBrush(hoverBtn == 1 ? QColor(168, 85, 247, 240) : QColor(15, 23, 42, 175));
    painter->drawEllipse(plusRf);
    painter->setPen(QPen(Qt::white, 1.9, Qt::SolidLine, Qt::RoundCap));
    const QPointF pc = plusRf.center();
    painter->drawLine(QPointF(pc.x() - 4.8, pc.y()), QPointF(pc.x() + 4.8, pc.y()));
    painter->drawLine(QPointF(pc.x(), pc.y() - 4.8), QPointF(pc.x(), pc.y() + 4.8));

    // Top-right Unified Star circular button (exact centered vector coordinates)
    const bool starred = index.data(StarredRole).toBool();
    const QRect starR = starButtonRect(option.rect);
    const QRectF starRf = QRectF(starR).adjusted(0.5, 0.5, -0.5, -0.5);
    painter->setPen(QPen(starred ? QColor(250, 204, 21, 180) : QColor(255, 255, 255, 115), 1.0));
    painter->setBrush(hoverBtn == 2 ? QColor(30, 41, 59, 238)
                                    : (starred ? QColor(15, 23, 42, 205) : QColor(15, 23, 42, 155)));
    painter->drawEllipse(starRf);
    drawUnifiedStarPolygon(*painter, starRf.center(),
                           6.8, starred, starred ? QColor(0xFA, 0xCC, 0x15) : QColor(241, 245, 249),
                           QColor(0xFA, 0xCC, 0x15));

    // Bottom-left PSD badge
    if (index.data(IsPsdRole).toBool()) {
        const QRect psdR(thumbR.left() + 6, thumbR.bottom() - 21, 32, 16);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(168, 85, 247, 230));
        painter->drawRoundedRect(psdR, 4, 4);
        painter->setPen(Qt::white);
        painter->setFont(QFont(QStringLiteral("Segoe UI"), 7, QFont::Bold));
        painter->drawText(psdR, Qt::AlignCenter, QStringLiteral("PSD"));
    }

    // Bottom filename / caption
    const QRect textR(cardR.left() + 6, thumbR.bottom() + 4, cardR.width() - 12, cardR.bottom() - thumbR.bottom() - 4);
    const QColor textNormal = dark ? QColor(0xE2, 0xE8, 0xF0) : QColor(0x1F, 0x29, 0x37);
    const QColor textSel = dark ? QColor(0xE9, 0xD5, 0xFF) : QColor(0x6B, 0x21, 0xA8);
    painter->setPen(selected ? textSel : textNormal);
    painter->setFont(QFont(QStringLiteral("Segoe UI"), 9, selected ? QFont::DemiBold : QFont::Normal));
    const QString elided = painter->fontMetrics().elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, textR.width());
    painter->drawText(textR, Qt::AlignHCenter | Qt::AlignTop, elided);

    painter->restore();
}

PhotoGridWidget::PhotoGridWidget(QWidget* parent) : QListWidget(parent) {
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setDragEnabled(false);
    setUniformItemSizes(true);
    setSpacing(0);
    setLayoutMode(QListView::Batched);
    setBatchSize(48);
    setItemDelegate(new PhotoCardDelegate(this));
}

int PhotoGridWidget::hitCornerButton(const QPoint& pos, QModelIndex* idxOut) const {
    const QModelIndex idx = indexAt(pos);
    if (idxOut) *idxOut = idx;
    if (!idx.isValid()) return 0;
    const QRect ir = visualRect(idx);
    if (PhotoCardDelegate::plusButtonRect(ir).contains(pos)) return 1;
    if (PhotoCardDelegate::starButtonRect(ir).contains(pos)) return 2;
    return 0;
}

void PhotoGridWidget::mouseMoveEvent(QMouseEvent* event) {
    QModelIndex idx;
    const int newBtn = hitCornerButton(event->pos(), &idx);
    const int newRow = idx.isValid() ? idx.row() : -1;

    if (newRow != hoveredRow_ || newBtn != hoveredBtn_) {
        const int oldRow = hoveredRow_;
        hoveredRow_ = newRow;
        hoveredBtn_ = newBtn;
        setCursor(newBtn != 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        if (oldRow >= 0 && oldRow < count()) {
            viewport()->update(visualItemRect(item(oldRow)));
        }
        if (hoveredRow_ >= 0 && hoveredRow_ < count()) {
            viewport()->update(visualItemRect(item(hoveredRow_)));
        }
    }

    if (pressedBtn_ != 0) {
        event->accept();
        return;
    }
    QListWidget::mouseMoveEvent(event);
}

void PhotoGridWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QModelIndex idx;
        const int btn = hitCornerButton(event->pos(), &idx);
        if (btn != 0 && idx.isValid()) {
            pressedRow_ = idx.row();
            pressedBtn_ = btn;
            viewport()->update(visualRect(idx));
            event->accept();
            return;
        }
    }
    pressedRow_ = -1;
    pressedBtn_ = 0;
    QListWidget::mousePressEvent(event);
}

void PhotoGridWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && pressedBtn_ != 0) {
        const int btn = pressedBtn_;
        const int row = pressedRow_;
        pressedBtn_ = 0;
        pressedRow_ = -1;

        QModelIndex idx;
        const int hit = hitCornerButton(event->pos(), &idx);
        if (hit == btn && idx.isValid() && idx.row() == row) {
            if (btn == 1) {
                emit cardPlusClicked(row, viewport()->mapToGlobal(event->pos()));
            } else if (btn == 2) {
                emit cardStarClicked(row);
            }
        }
        event->accept();
        return;
    }
    QListWidget::mouseReleaseEvent(event);
}

void PhotoGridWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && hitCornerButton(event->pos()) != 0) {
        event->accept();
        return;
    }
    QListWidget::mouseDoubleClickEvent(event);
}

void PhotoGridWidget::leaveEvent(QEvent* event) {
    if (hoveredRow_ >= 0 && hoveredRow_ < count()) {
        const QRect r = visualItemRect(item(hoveredRow_));
        hoveredRow_ = -1;
        hoveredBtn_ = 0;
        viewport()->update(r);
    }
    setCursor(Qt::ArrowCursor);
    QListWidget::leaveEvent(event);
}

// ==================== LIVE RGB HISTOGRAM ====================
HistogramWidget::HistogramWidget(QWidget* parent) : QWidget(parent) {
    setFixedHeight(70);
}

void HistogramWidget::setImage(const QImage& img) {
    histR_.fill(0);
    histG_.fill(0);
    histB_.fill(0);
    maxCount_ = 1;
    hasData_ = false;

    if (img.isNull()) {
        update();
        return;
    }

    const QImage sample = img.scaled(160, 120, Qt::KeepAspectRatio, Qt::FastTransformation)
                              .convertToFormat(QImage::Format_RGBA8888);
    for (int y = 0; y < sample.height(); ++y) {
        const uint8_t* px = sample.constScanLine(y);
        for (int x = 0; x < sample.width(); ++x) {
            histR_[px[0] >> 2]++;
            histG_[px[1] >> 2]++;
            histB_[px[2] >> 2]++;
            px += 4;
        }
    }

    for (int i = 1; i < 63; ++i) {
        maxCount_ = std::max({maxCount_, histR_[i], histG_[i], histB_[i]});
    }
    hasData_ = true;
    update();
}

void HistogramWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const bool dark = AppSettings::instance().effectiveDarkTheme();
    p.fillRect(rect(), dark ? QColor(0x20, 0x23, 0x2D) : QColor(0xF8, 0xFA, 0xFC));
    p.setPen(dark ? QColor(0x33, 0x38, 0x48) : QColor(0xE2, 0xE8, 0xF0));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);

    if (!hasData_) return;

    auto drawChannel = [&](const std::array<int, 64>& bins, const QColor& fillColor) {
        QPainterPath path;
        const double w = width() - 4.0;
        const double h = height() - 6.0;
        path.moveTo(2.0, height() - 2.0);
        for (int i = 0; i < 64; ++i) {
            const double x = 2.0 + (i / 63.0) * w;
            const double norm = std::min(1.0, static_cast<double>(bins[i]) / maxCount_);
            const double y = (height() - 2.0) - norm * h;
            path.lineTo(x, y);
        }
        path.lineTo(width() - 2.0, height() - 2.0);
        path.closeSubpath();
        p.fillPath(path, fillColor);
    };

    drawChannel(histR_, QColor(239, 68, 68, 90));
    drawChannel(histG_, QColor(34, 197, 94, 90));
    drawChannel(histB_, QColor(59, 130, 246, 95));
}

// ==================== MAGNETIC SNAP SLIDER ====================
SnapSlider::SnapSlider(int minVal, int maxVal, int defaultVal, QWidget* parent)
    : QSlider(Qt::Horizontal, parent), defaultVal_(defaultVal), prevVal_(defaultVal) {
    setRange(minVal, maxVal);
    setValue(defaultVal);
    setFixedHeight(22);

    connect(this, &QSlider::sliderPressed, this, [this]() {
        prevVal_ = value();
        emit editStarted();
    });

    connect(this, &QSlider::sliderReleased, this, [this]() {
        emit commitRequested();
    });

    connect(this, &QSlider::valueChanged, this, [this](int val) {
        if (internalAdjust_) return;

        int finalVal = val;
        if (isSliderDown() && val != defaultVal_) {
            const int prevDist = std::abs(prevVal_ - defaultVal_);
            const int currDist = std::abs(val - defaultVal_);
            const int snapZone = std::max(2, (maximum() - minimum()) / 60);
            const bool crossedOrEntered = (prevDist >= snapZone + 1 && currDist <= snapZone) ||
                                          ((prevVal_ < defaultVal_ && val > defaultVal_) && prevDist <= snapZone + 2) ||
                                          ((prevVal_ > defaultVal_ && val < defaultVal_) && prevDist <= snapZone + 2);
            if (crossedOrEntered) {
                finalVal = defaultVal_;
                internalAdjust_ = true;
                setValue(finalVal);
                internalAdjust_ = false;
            }
        }
        prevVal_ = finalVal;
        emit snappedValueChanged(finalVal);
    });
}

void SnapSlider::syncValueSilently(int v) {
    internalAdjust_ = true;
    const QSignalBlocker blocker(this);
    setValue(v);
    prevVal_ = v;
    internalAdjust_ = false;
    update();
}

void SnapSlider::paintEvent(QPaintEvent* event) {
    {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        opt.sliderPosition = defaultVal_;
        opt.sliderValue = defaultVal_;
        const QRect handleRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
        const int cx = handleRect.center().x();

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(QColor(0x94, 0xA3, 0xB8), 1.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(cx, 1, cx, 5);
        p.drawLine(cx, height() - 6, cx, height() - 2);
    }

    QSlider::paintEvent(event);
}

void SnapSlider::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        prevVal_ = value();
        emit editStarted();
    }
    QSlider::mousePressEvent(event);
}

void SnapSlider::mouseReleaseEvent(QMouseEvent* event) {
    const bool wasDown = isSliderDown();
    QSlider::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton && !wasDown) {
        emit commitRequested();
    }
}

void SnapSlider::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit editStarted();
        setValue(defaultVal_);
        emit commitRequested();
        event->accept();
        return;
    }
    QSlider::mouseDoubleClickEvent(event);
}

// ==================== MAGNETIC SLIDER ROW ====================
MagneticSliderRow::MagneticSliderRow(const QString& title,
                                     int minVal,
                                     int maxVal,
                                     int defaultVal,
                                     const QString& unitSuffix,
                                     QWidget* parent)
    : QWidget(parent), defaultVal_(defaultVal), suffix_(unitSuffix) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 2);
    root->setSpacing(1);

    auto* header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(6);

    titleLabel_ = new QLabel(title, this);
    titleLabel_->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 500; color: #334155;"));

    valueLabel_ = new QLabel(this);
    valueLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    valueLabel_->setMinimumWidth(46);

    resetBtn_ = new QPushButton(this);
    resetBtn_->setObjectName(QStringLiteral("miniResetBtn"));
    resetBtn_->setFixedSize(24, 20);
    resetBtn_->setIconSize(QSize(13, 13));
    resetBtn_->setFocusPolicy(Qt::NoFocus);
    resetBtn_->setCursor(Qt::PointingHandCursor);
    resetBtn_->setToolTip(trUi(QStringLiteral("Сбросить параметр"), QStringLiteral("Reset parameter")));

    header->addWidget(titleLabel_, 1);
    header->addWidget(valueLabel_);
    header->addWidget(resetBtn_);

    slider_ = new SnapSlider(minVal, maxVal, defaultVal, this);

    root->addLayout(header);
    root->addWidget(slider_);

    connect(slider_, &SnapSlider::editStarted, this, &MagneticSliderRow::editStarted);
    connect(slider_, &SnapSlider::snappedValueChanged, this, [this](int v) {
        updateReadout(v);
        if (slider_->isSliderDown()) {
            emit liveValueChanged(v);
        } else {
            emit valueCommitted(v);
        }
    });
    connect(slider_, &SnapSlider::commitRequested, this, [this]() {
        emit valueCommitted(slider_->value());
    });
    connect(resetBtn_, &QPushButton::clicked, this, [this]() {
        if (slider_->value() == defaultVal_) return;
        emit editStarted();
        slider_->setValue(defaultVal_);
        emit valueCommitted(defaultVal_);
    });

    updateReadout(defaultVal_);
}

void MagneticSliderRow::setTitle(const QString& title) {
    titleLabel_->setText(title);
}

void MagneticSliderRow::setValueSilently(int v) {
    slider_->syncValueSilently(v);
    updateReadout(v);
}

void MagneticSliderRow::refreshTheme() {
    const bool dark = AppSettings::instance().effectiveDarkTheme();
    titleLabel_->setStyleSheet(dark
                                   ? QStringLiteral("font-size: 12px; font-weight: 500; color: #E2E8F0;")
                                   : QStringLiteral("font-size: 12px; font-weight: 500; color: #334155;"));
    resetBtn_->setToolTip(trUi(QStringLiteral("Сбросить параметр"), QStringLiteral("Reset parameter")));
    updateReadout(slider_->value());
}

void MagneticSliderRow::updateReadout(int v) {
    const QString txt = (v > 0 && slider_->minimum() < 0)
                            ? QString("+%1%2").arg(v).arg(suffix_)
                            : QString("%1%2").arg(v).arg(suffix_);
    valueLabel_->setText(txt);
    const bool isDef = (v == defaultVal_);
    const bool dark = AppSettings::instance().effectiveDarkTheme();
    resetBtn_->setEnabled(!isDef);
    resetBtn_->setIcon(makeUiIcon(UiIconKind::Undo,
                                  isDef ? (dark ? QColor(0x47, 0x55, 0x69) : QColor(0xCB, 0xD5, 0xE1))
                                        : (dark ? QColor(0xC0, 0x84, 0xFC) : QColor(0xA8, 0x55, 0xF7)),
                                  14));
    valueLabel_->setStyleSheet(isDef
                                   ? QStringLiteral("font-size: 11px; font-weight: 600; color: #64748B;")
                                   : (dark ? QStringLiteral("font-size: 11px; font-weight: 700; color: #C084FC;")
                                           : QStringLiteral("font-size: 11px; font-weight: 700; color: #7E22CE;")));
}

// ==================== INTERACTIVE EDITOR CANVAS ====================
EditorCanvasWidget::EditorCanvasWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void EditorCanvasWidget::setDisplayImage(const QImage& img, bool resetView) {
    image_ = img;
    hasAlpha_ = !img.isNull() && img.hasAlphaChannel();
    pixmap_ = img.isNull() ? QPixmap() : QPixmap::fromImage(img);
    if (!pixmap_.isNull()) {
        pixmap_.setDevicePixelRatio(1.0);
    }
    if (resetView) {
        actualSize_ = false;
        customZoom_ = false;
        zoomFactor_ = 1.0;
        panOffset_ = QPointF(0.0, 0.0);
        emit zoomStateChanged(false);
    }
    update();
}

void EditorCanvasWidget::setActualSizeMode(bool actual) {
    actualSize_ = actual;
    customZoom_ = actual;
    zoomFactor_ = actual ? 1.0 : computeFitScale();
    panOffset_ = QPointF(0.0, 0.0);
    emit zoomStateChanged(actualSize_);
    update();
}

void EditorCanvasWidget::zoomByCenter(double factor) {
    if (pixmap_.isNull() || cropMode_ || factor <= 0.0) return;
    const double oldS = effectiveScale();
    const double newS = std::clamp(oldS * factor, 0.05, 8.0);
    if (qFuzzyCompare(oldS, newS)) return;

    const QPointF center(width() * 0.5, height() * 0.5);
    const QPointF imgCenter = center + panOffset_;
    panOffset_ = center - (center - imgCenter) * (newS / oldS) - center;
    zoomFactor_ = newS;
    customZoom_ = true;
    actualSize_ = false;
    emit zoomStateChanged(false);
    update();
}

void EditorCanvasWidget::setCropMode(bool enabled, double l, double t, double r, double b) {
    cropMode_ = enabled;
    cropL_ = std::clamp(l, 0.0, 0.96);
    cropT_ = std::clamp(t, 0.0, 0.96);
    cropR_ = std::clamp(r, cropL_ + 0.04, 1.0);
    cropB_ = std::clamp(b, cropT_ + 0.04, 1.0);
    if (enabled) {
        eyedropperMode_ = false;
        actualSize_ = false;
        customZoom_ = false;
        panOffset_ = QPointF(0.0, 0.0);
        emit zoomStateChanged(false);
    }
    updateCursorForPos(mapFromGlobal(QCursor::pos()));
    update();
}

void EditorCanvasWidget::setLiveTextLayer(const EffectLayer* layer) {
    if (activeDrag_ == DragHandle::DragText) {
        if (layer && layer->enabled && layer->preset == EffectPreset::TextOverlay) {
            const double curX = liveTextLayer_.posX;
            const double curY = liveTextLayer_.posY;
            hasLiveText_ = true;
            liveTextLayer_ = *layer;
            liveTextLayer_.posX = curX;
            liveTextLayer_.posY = curY;
        }
        return;
    }
    const bool nextHas = (layer && layer->enabled && layer->preset == EffectPreset::TextOverlay);
    if (!nextHas && !hasLiveText_) {
        return;
    }
    if (nextHas && hasLiveText_ &&
        liveTextLayer_.id == layer->id &&
        liveTextLayer_.text == layer->text &&
        liveTextLayer_.textColor == layer->textColor &&
        liveTextLayer_.textSize == layer->textSize &&
        liveTextLayer_.intensity == layer->intensity &&
        qFuzzyCompare(liveTextLayer_.posX, layer->posX) &&
        qFuzzyCompare(liveTextLayer_.posY, layer->posY)) {
        return;
    }
    if (nextHas) {
        hasLiveText_ = true;
        liveTextLayer_ = *layer;
    } else {
        hasLiveText_ = false;
        liveTextLayer_ = EffectLayer{};
    }
    updateCursorForPos(mapFromGlobal(QCursor::pos()));
    update();
}

void EditorCanvasWidget::setShowGrid(bool show) {
    showGrid_ = show;
    update();
}

void EditorCanvasWidget::setEyedropperMode(bool enabled) {
    eyedropperMode_ = enabled;
    if (enabled) cropMode_ = false;
    updateCursorForPos(mapFromGlobal(QCursor::pos()));
    update();
}

void EditorCanvasWidget::setBackdropMode(CanvasBackdropMode mode) {
    if (backdropMode_ == mode) return;
    backdropMode_ = mode;
    update();
}

double EditorCanvasWidget::computeFitScale() const {
    if (pixmap_.isNull()) return 1.0;
    const double availW = std::max(64.0, static_cast<double>(width()) - 36.0);
    const double availH = std::max(64.0, static_cast<double>(height()) - 32.0);
    return std::clamp(std::min(availW / pixmap_.width(), availH / pixmap_.height()), 0.01, 4.0);
}

double EditorCanvasWidget::effectiveScale() const {
    if (pixmap_.isNull()) return 1.0;
    if (actualSize_) return 1.0;
    return customZoom_ ? zoomFactor_ : computeFitScale();
}

QRectF EditorCanvasWidget::imageScreenRect() const {
    if (pixmap_.isNull()) return QRectF();
    const double s = effectiveScale();
    const double w = pixmap_.width() * s;
    const double h = pixmap_.height() * s;
    const double cx = width() * 0.5 + panOffset_.x();
    const double cy = height() * 0.5 + panOffset_.y();
    return QRectF(cx - w * 0.5, cy - h * 0.5, w, h);
}

QRectF EditorCanvasWidget::cropScreenRect(const QRectF& imgRect) const {
    return QRectF(
        imgRect.left() + cropL_ * imgRect.width(),
        imgRect.top() + cropT_ * imgRect.height(),
        (cropR_ - cropL_) * imgRect.width(),
        (cropB_ - cropT_) * imgRect.height()
    );
}

EditorCanvasWidget::DragHandle EditorCanvasWidget::hitTestCropHandle(const QPointF& pos, const QRectF& c) const {
    constexpr double kTol = 11.0;
    const bool nearL = std::abs(pos.x() - c.left()) <= kTol;
    const bool nearR = std::abs(pos.x() - c.right()) <= kTol;
    const bool nearT = std::abs(pos.y() - c.top()) <= kTol;
    const bool nearB = std::abs(pos.y() - c.bottom()) <= kTol;

    if (nearL && nearT) return DragHandle::TopLeft;
    if (nearR && nearT) return DragHandle::TopRight;
    if (nearR && nearB) return DragHandle::BottomRight;
    if (nearL && nearB) return DragHandle::BottomLeft;
    if (nearT && pos.x() >= c.left() && pos.x() <= c.right()) return DragHandle::Top;
    if (nearB && pos.x() >= c.left() && pos.x() <= c.right()) return DragHandle::Bottom;
    if (nearL && pos.y() >= c.top() && pos.y() <= c.bottom()) return DragHandle::Left;
    if (nearR && pos.y() >= c.top() && pos.y() <= c.bottom()) return DragHandle::Right;
    if (c.contains(pos)) return DragHandle::MoveCrop;
    return DragHandle::None;
}

void EditorCanvasWidget::updateCursorForPos(const QPointF& pos) {
    if (eyedropperMode_) {
        setCursor(Qt::CrossCursor);
        return;
    }
    if (pixmap_.isNull()) {
        setCursor(Qt::ArrowCursor);
        return;
    }
    const QRectF imgR = imageScreenRect();
    if (cropMode_) {
        const DragHandle h = hitTestCropHandle(pos, cropScreenRect(imgR));
        switch (h) {
            case DragHandle::TopLeft:
            case DragHandle::BottomRight:
                setCursor(Qt::SizeFDiagCursor); return;
            case DragHandle::TopRight:
            case DragHandle::BottomLeft:
                setCursor(Qt::SizeBDiagCursor); return;
            case DragHandle::Top:
            case DragHandle::Bottom:
                setCursor(Qt::SizeVerCursor); return;
            case DragHandle::Left:
            case DragHandle::Right:
                setCursor(Qt::SizeHorCursor); return;
            case DragHandle::MoveCrop:
                setCursor(Qt::SizeAllCursor); return;
            default:
                setCursor(Qt::ArrowCursor); return;
        }
    }
    if (hasLiveText_ && imgR.contains(pos)) {
        setCursor(Qt::SizeAllCursor);
        return;
    }
    setCursor(imgR.contains(pos) ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

void EditorCanvasWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.fillRect(rect(), QColor(0x2B, 0x2E, 0x35));

    if (pixmap_.isNull()) return;

    const QRectF imgR = imageScreenRect();

    // Optional backdrop strictly behind the image (Default = OFF)
    if (backdropMode_ == CanvasBackdropMode::White) {
        p.fillRect(imgR, Qt::white);
    } else if (backdropMode_ == CanvasBackdropMode::Black) {
        p.fillRect(imgR, Qt::black);
    } else if (!hasAlpha_) {
        p.fillRect(imgR.adjusted(0, 3, 4, 8), QColor(0, 0, 0, 95));
    }

    p.drawPixmap(imgR, pixmap_, QRectF(0.0, 0.0, pixmap_.width(), pixmap_.height()));

    // Zero-lag GPU-fast live preview of the selected TextOverlay layer (never runs full pixel pipeline on drag!)
    if (hasLiveText_ && !cropMode_) {
        ImageLoader::drawTextOverlay(p, imgR, liveTextLayer_);
        const QPointF pt(imgR.left() + std::clamp(liveTextLayer_.posX, 0.02, 0.98) * imgR.width(),
                         imgR.top() + std::clamp(liveTextLayer_.posY, 0.02, 0.98) * imgR.height());
        p.setPen(QPen(QColor(168, 85, 247, 220), 1.5, Qt::DashLine));
        p.setBrush(QColor(168, 85, 247, 45));
        p.drawEllipse(pt, 9.0, 9.0);
        p.setBrush(QColor(0xA8, 0x55, 0xF7));
        p.setPen(QPen(Qt::white, 1.5));
        p.drawEllipse(pt, 3.5, 3.5);
    }

    if (showGrid_) {
        p.setPen(QPen(QColor(255, 255, 255, 110), 1.0, Qt::DashLine));
        for (int i = 1; i < 6; ++i) {
            const double x = imgR.left() + imgR.width() * (i / 6.0);
            const double y = imgR.top() + imgR.height() * (i / 6.0);
            p.drawLine(QPointF(x, imgR.top()), QPointF(x, imgR.bottom()));
            p.drawLine(QPointF(imgR.left(), y), QPointF(imgR.right(), y));
        }
    }

    if (cropMode_) {
        const QRectF cr = cropScreenRect(imgR);
        QPainterPath outer;
        outer.addRect(imgR);
        QPainterPath inner;
        inner.addRect(cr);
        p.fillPath(outer.subtracted(inner), QColor(0, 0, 0, 145));

        p.setPen(QPen(QColor(255, 255, 255, 120), 1.0));
        p.drawLine(QPointF(cr.left() + cr.width() / 3.0, cr.top()),
                   QPointF(cr.left() + cr.width() / 3.0, cr.bottom()));
        p.drawLine(QPointF(cr.left() + cr.width() * 2.0 / 3.0, cr.top()),
                   QPointF(cr.left() + cr.width() * 2.0 / 3.0, cr.bottom()));
        p.drawLine(QPointF(cr.left(), cr.top() + cr.height() / 3.0),
                   QPointF(cr.right(), cr.top() + cr.height() / 3.0));
        p.drawLine(QPointF(cr.left(), cr.top() + cr.height() * 2.0 / 3.0),
                   QPointF(cr.right(), cr.top() + cr.height() * 2.0 / 3.0));

        p.setPen(QPen(Qt::white, 2.0));
        p.drawRect(cr);

        p.setBrush(QColor(0xA8, 0x55, 0xF7));
        p.setPen(QPen(Qt::white, 1.5));
        const QList<QPointF> pts = {
            cr.topLeft(), cr.topRight(), cr.bottomRight(), cr.bottomLeft(),
            QPointF(cr.center().x(), cr.top()), QPointF(cr.center().x(), cr.bottom()),
            QPointF(cr.left(), cr.center().y()), QPointF(cr.right(), cr.center().y())
        };
        for (const QPointF& pt : pts) {
            p.drawRect(QRectF(pt.x() - 5.0, pt.y() - 5.0, 10.0, 10.0));
        }
    }
}

void EditorCanvasWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

void EditorCanvasWidget::wheelEvent(QWheelEvent* event) {
    if (pixmap_.isNull() || cropMode_) {
        event->ignore();
        return;
    }
    const int dy = event->angleDelta().y() != 0 ? event->angleDelta().y() : event->pixelDelta().y();
    if (dy != 0) {
        const double oldS = effectiveScale();
        const double newS = std::clamp(oldS * std::pow(1.0014, static_cast<double>(dy)), 0.05, 8.0);
        const QPointF center(width() * 0.5, height() * 0.5);
        const QPointF imgCenter = center + panOffset_;
        const QPointF anchor = event->position();
        panOffset_ = anchor - (anchor - imgCenter) * (newS / oldS) - center;
        zoomFactor_ = newS;
        customZoom_ = true;
        actualSize_ = false;
        emit zoomStateChanged(false);
        update();
    }
    event->accept();
}

void EditorCanvasWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || pixmap_.isNull()) {
        QWidget::mousePressEvent(event);
        return;
    }
    const QRectF imgR = imageScreenRect();
    if (eyedropperMode_) {
        if (imgR.contains(event->position()) && !image_.isNull()) {
            const int px = std::clamp(qRound(((event->position().x() - imgR.left()) / imgR.width()) * (image_.width() - 1)), 0, image_.width() - 1);
            const int py = std::clamp(qRound(((event->position().y() - imgR.top()) / imgR.height()) * (image_.height() - 1)), 0, image_.height() - 1);
            emit colorPicked(image_.pixelColor(px, py));
        }
        event->accept();
        return;
    }

    if (cropMode_) {
        activeDrag_ = hitTestCropHandle(event->position(), cropScreenRect(imgR));
        dragStartPos_ = event->pos();
        startL_ = cropL_;
        startT_ = cropT_;
        startR_ = cropR_;
        startB_ = cropB_;
        event->accept();
        return;
    }

    if (hasLiveText_ && imgR.contains(event->position())) {
        const QPointF pt(imgR.left() + std::clamp(liveTextLayer_.posX, 0.02, 0.98) * imgR.width(),
                         imgR.top() + std::clamp(liveTextLayer_.posY, 0.02, 0.98) * imgR.height());
        const QPointF delta = pt - event->position();
        textDragGrabOffset_ = (std::hypot(delta.x(), delta.y()) <= 90.0) ? delta : QPointF(0.0, 0.0);
        const QPointF targetPt = event->position() + textDragGrabOffset_;
        activeDrag_ = DragHandle::DragText;
        emit textPositionDragStarted();
        liveTextLayer_.posX = std::clamp((targetPt.x() - imgR.left()) / std::max(1.0, imgR.width()), 0.02, 0.98);
        liveTextLayer_.posY = std::clamp((targetPt.y() - imgR.top()) / std::max(1.0, imgR.height()), 0.02, 0.98);
        update();
        event->accept();
        return;
    }

    if (imgR.contains(event->position())) {
        activeDrag_ = DragHandle::PanImage;
        dragStartPos_ = event->pos();
        panStartOffset_ = panOffset_;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void EditorCanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (activeDrag_ == DragHandle::PanImage) {
        panOffset_ = panStartOffset_ + QPointF(event->pos() - dragStartPos_);
        update();
        event->accept();
        return;
    }
    if (activeDrag_ == DragHandle::DragText) {
        const QRectF imgR = imageScreenRect();
        const QPointF targetPt = event->position() + textDragGrabOffset_;
        liveTextLayer_.posX = std::clamp((targetPt.x() - imgR.left()) / std::max(1.0, imgR.width()), 0.02, 0.98);
        liveTextLayer_.posY = std::clamp((targetPt.y() - imgR.top()) / std::max(1.0, imgR.height()), 0.02, 0.98);
        update();
        event->accept();
        return;
    }
    if (cropMode_ && activeDrag_ != DragHandle::None) {
        const QRectF imgR = imageScreenRect();
        const double dx = (event->pos().x() - dragStartPos_.x()) / std::max(1.0, imgR.width());
        const double dy = (event->pos().y() - dragStartPos_.y()) / std::max(1.0, imgR.height());
        constexpr double kMinSize = 0.04;

        if (activeDrag_ == DragHandle::MoveCrop) {
            const double cw = startR_ - startL_;
            const double ch = startB_ - startT_;
            cropL_ = std::clamp(startL_ + dx, 0.0, 1.0 - cw);
            cropT_ = std::clamp(startT_ + dy, 0.0, 1.0 - ch);
            cropR_ = cropL_ + cw;
            cropB_ = cropT_ + ch;
        } else {
            if (activeDrag_ == DragHandle::Left || activeDrag_ == DragHandle::TopLeft || activeDrag_ == DragHandle::BottomLeft) {
                cropL_ = std::clamp(startL_ + dx, 0.0, startR_ - kMinSize);
            }
            if (activeDrag_ == DragHandle::Right || activeDrag_ == DragHandle::TopRight || activeDrag_ == DragHandle::BottomRight) {
                cropR_ = std::clamp(startR_ + dx, startL_ + kMinSize, 1.0);
            }
            if (activeDrag_ == DragHandle::Top || activeDrag_ == DragHandle::TopLeft || activeDrag_ == DragHandle::TopRight) {
                cropT_ = std::clamp(startT_ + dy, 0.0, startB_ - kMinSize);
            }
            if (activeDrag_ == DragHandle::Bottom || activeDrag_ == DragHandle::BottomLeft || activeDrag_ == DragHandle::BottomRight) {
                cropB_ = std::clamp(startB_ + dy, startT_ + kMinSize, 1.0);
            }
        }
        update();
        event->accept();
        return;
    }
    updateCursorForPos(event->position());
    QWidget::mouseMoveEvent(event);
}

void EditorCanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (activeDrag_ != DragHandle::None) {
        const DragHandle finished = activeDrag_;
        activeDrag_ = DragHandle::None;
        updateCursorForPos(event->position());
        if (finished == DragHandle::DragText) {
            emit textPositionCommitted(liveTextLayer_.posX, liveTextLayer_.posY);
        } else if (cropMode_ && finished != DragHandle::PanImage) {
            emit cropRectAdjusted(cropL_, cropT_, cropR_, cropB_);
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

// ==================== MAIN WINDOW ====================
MainWindow::MainWindow(LibraryDatabase* db, QWidget* parent)
    : QMainWindow(parent), db_(db) {
    setWindowIcon(QIcon(QStringLiteral(":/icons/lumina.png")));
    setFocusPolicy(Qt::StrongFocus);
    resize(1440, 900);

    previewCoalesceTimer_ = new QTimer(this);
    previewCoalesceTimer_->setSingleShot(true);
    previewCoalesceTimer_->setInterval(18);
    connect(previewCoalesceTimer_, &QTimer::timeout, this, [this]() {
        dragPreviewActive_ = true;
        refreshEditorPreview(false);
    });

    dbSaveTimer_ = new QTimer(this);
    dbSaveTimer_->setSingleShot(true);
    dbSaveTimer_->setInterval(260);
    connect(dbSaveTimer_, &QTimer::timeout, this, [this]() {
        if (!editingRecord_.filePath.isEmpty()) {
            db_->setEdits(editingRecord_.filePath, editingRecord_.edits);
        }
    });

    {
        QSettings st(QStringLiteral("Lumina"), QStringLiteral("LuminaPhoto"));
        collectionCollapsed_ = st.value(QStringLiteral("sidebar/collectionCollapsed"), false).toBool();
        albumsCollapsed_ = st.value(QStringLiteral("sidebar/albumsCollapsed"), false).toBool();
        foldersCollapsed_ = st.value(QStringLiteral("sidebar/foldersCollapsed"), false).toBool();
    }

    setupUi();

    connect(&AppSettings::instance(), &AppSettings::themeChanged, this, [this](bool) {
        applyTheme();
    });
    connect(&AppSettings::instance(), &AppSettings::languageChanged, this, [this](bool) {
        retranslateUi();
    });

    reloadSidebar();
    reloadPhotoGrid();
    QTimer::singleShot(120, this, &MainWindow::pruneMissingFilesAsync);
}

MainWindow::~MainWindow() {
    stopEditorMovie();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == edLeftPane_ &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        adaptLeftPaneLayout(edLeftPane_->width());
    } else if (watched == sidebarList_ &&
               (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        updateSidebarCompactMode();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::ActivationChange && isActiveWindow() && mainStack_ && mainStack_->currentIndex() == 0) {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (!pruneInFlight_ && (nowMs - lastPruneMs_ > 2500)) {
            pruneMissingFilesAsync();
        }
    }
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    updateEditorPaneConstraints();
    if (mainStack_ && mainStack_->currentIndex() == 0) {
        prioritizeVisibleThumbnails();
    } else if (mainStack_ && mainStack_->currentIndex() == 1) {
        if (edLeftPane_) {
            adaptLeftPaneLayout(edLeftPane_->width());
        }
        if (!editingRawImage_.isNull()) {
            refreshEditorPreview(false);
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (mainStack_ && mainStack_->currentIndex() == 1) {
        QWidget* fw = QApplication::focusWidget();
        const bool typingText = qobject_cast<QLineEdit*>(fw) != nullptr ||
                                qobject_cast<QPlainTextEdit*>(fw) != nullptr;
        if (!typingText) {
            if (event->key() == Qt::Key_Left) {
                openRelativePhotoInEditor(-1);
                event->accept();
                return;
            }
            if (event->key() == Qt::Key_Right) {
                openRelativePhotoInEditor(1);
                event->accept();
                return;
            }
            if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
                editorCanvas_->zoomByCenter(1.20);
                event->accept();
                return;
            }
            if (event->key() == Qt::Key_Down || event->key() == Qt::Key_Minus) {
                editorCanvas_->zoomByCenter(1.0 / 1.20);
                event->accept();
                return;
            }
            if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && editorCanvas_->isCropMode()) {
                setCropEditingEnabled(false, true);
                event->accept();
                return;
            }
            if (event->key() == Qt::Key_Escape && editorCanvas_->isCropMode()) {
                setCropEditingEnabled(false, true);
                event->accept();
                return;
            }
        }
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::setupUi() {
    mainStack_ = new QStackedWidget(this);
    setCentralWidget(mainStack_);

    // ==================== PAGE 0: LIBRARY ====================
    auto* libraryPage = new QWidget(mainStack_);
    auto* libRoot = new QVBoxLayout(libraryPage);
    libRoot->setContentsMargins(0, 0, 0, 0);
    libRoot->setSpacing(0);

    topBar_ = new QWidget(libraryPage);
    topBar_->setObjectName(QStringLiteral("topBar"));
    topBar_->setFixedHeight(58);
    auto* topLayout = new QHBoxLayout(topBar_);
    topLayout->setContentsMargins(16, 0, 16, 0);
    topLayout->setSpacing(10);

    brandLabel_ = new QLabel(QStringLiteral("Lumina Photo"), topBar_);

    constexpr int kBtnH = 34;

    addFolderBtn_ = new QPushButton(topBar_);
    addFolderBtn_->setObjectName(QStringLiteral("primaryBtn"));
    addFolderBtn_->setFixedHeight(kBtnH);
    newAlbumBtn_ = new QPushButton(topBar_);
    newAlbumBtn_->setFixedHeight(kBtnH);
    assocBtn_ = new QPushButton(topBar_);
    assocBtn_->setFixedHeight(kBtnH);

    libSettingsBtn_ = new QPushButton(topBar_);
    libSettingsBtn_->setObjectName(QStringLiteral("iconSquareBtn"));
    libSettingsBtn_->setFixedSize(kBtnH, kBtnH);
    libSettingsBtn_->setFocusPolicy(Qt::NoFocus);

    searchEdit_ = new QLineEdit(topBar_);
    searchEdit_->setFixedHeight(kBtnH);
    searchEdit_->setMinimumWidth(210);
    searchEdit_->setMaximumWidth(280);

    starredFilterBtn_ = new QPushButton(topBar_);
    starredFilterBtn_->setObjectName(QStringLiteral("starBtn"));
    starredFilterBtn_->setFixedHeight(kBtnH);
    starredFilterBtn_->setCheckable(true);
    starredFilterBtn_->setIcon(makeUnifiedStarIcon(false, 16));
    starredFilterBtn_->setIconSize(QSize(16, 16));

    zoomLabel_ = new QLabel(topBar_);
    thumbSlider_ = new QSlider(Qt::Horizontal, topBar_);
    thumbSlider_->setRange(120, 280);
    thumbSlider_->setValue(thumbSize_);
    thumbSlider_->setFixedWidth(100);

    statusCountLabel_ = new QLabel(topBar_);

    topLayout->addWidget(brandLabel_);
    topLayout->addSpacing(8);
    topLayout->addWidget(searchEdit_);
    topLayout->addWidget(starredFilterBtn_);
    topLayout->addWidget(statusCountLabel_);
    topLayout->addStretch();
    topLayout->addWidget(zoomLabel_);
    topLayout->addWidget(thumbSlider_);
    topLayout->addWidget(assocBtn_);
    topLayout->addWidget(newAlbumBtn_);
    topLayout->addWidget(addFolderBtn_);
    topLayout->addWidget(libSettingsBtn_);
    libRoot->addWidget(topBar_);

    libSplitter_ = new QSplitter(Qt::Horizontal, libraryPage);
    libSplitter_->setHandleWidth(4);
    libSplitter_->setChildrenCollapsible(false);

    sidebarList_ = new QListWidget(libSplitter_);
    sidebarList_->setObjectName(QStringLiteral("sidebarList"));
    sidebarList_->setFocusPolicy(Qt::NoFocus);
    sidebarList_->setIconSize(QSize(28, 22));
    sidebarList_->setMinimumWidth(68);
    sidebarList_->setMaximumWidth(420);
    sidebarList_->setTextElideMode(Qt::ElideRight);
    sidebarList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sidebarList_->setContextMenuPolicy(Qt::CustomContextMenu);
    sidebarList_->installEventFilter(this);

    auto* gridContainer = new QWidget(libSplitter_);
    auto* gridVBox = new QVBoxLayout(gridContainer);
    gridVBox->setContentsMargins(0, 0, 0, 0);
    gridVBox->setSpacing(0);

    gridContentStack_ = new QStackedWidget(gridContainer);

    auto* emptyHost = new QWidget(gridContentStack_);
    auto* emptyOuterLay = new QVBoxLayout(emptyHost);
    emptyOuterLay->setAlignment(Qt::AlignCenter);
    auto* emptyCard = new QWidget(emptyHost);
    emptyCard->setObjectName(QStringLiteral("emptyCard"));
    emptyCard->setFixedWidth(480);
    auto* emptyCardLay = new QVBoxLayout(emptyCard);
    emptyCardLay->setContentsMargins(32, 32, 32, 32);
    emptyCardLay->setSpacing(12);
    emptyCardLay->setAlignment(Qt::AlignCenter);

    emptyTitle_ = new QLabel(emptyCard);
    emptyTitle_->setAlignment(Qt::AlignCenter);

    emptyDesc_ = new QLabel(emptyCard);
    emptyDesc_->setWordWrap(true);
    emptyDesc_->setAlignment(Qt::AlignCenter);

    auto* emptyBtnRow = new QHBoxLayout();
    emptyBtnRow->setSpacing(10);
    emptyAddBtn_ = new QPushButton(emptyCard);
    emptyAddBtn_->setObjectName(QStringLiteral("primaryBtn"));
    emptyAddBtn_->setFixedHeight(kBtnH);
    emptyScanPicsBtn_ = new QPushButton(emptyCard);
    emptyScanPicsBtn_->setFixedHeight(kBtnH);
    emptyBtnRow->addWidget(emptyAddBtn_);
    emptyBtnRow->addWidget(emptyScanPicsBtn_);

    emptyCardLay->addWidget(emptyTitle_);
    emptyCardLay->addWidget(emptyDesc_);
    emptyCardLay->addSpacing(6);
    emptyCardLay->addLayout(emptyBtnRow);
    emptyOuterLay->addWidget(emptyCard);

    photoGrid_ = new PhotoGridWidget(gridContentStack_);
    photoGrid_->setObjectName(QStringLiteral("photoGrid"));
    photoGrid_->setViewMode(QListView::IconMode);
    photoGrid_->setResizeMode(QListView::Adjust);
    photoGrid_->setMovement(QListView::Static);
    photoGrid_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    photoGrid_->setContextMenuPolicy(Qt::CustomContextMenu);
    photoGrid_->setSpacing(0);
    photoGrid_->setIconSize(QSize(thumbSize_, thumbSize_ * 3 / 4));
    photoGrid_->setGridSize(QSize(thumbSize_ + 22, thumbSize_ * 3 / 4 + 44));

    gridContentStack_->addWidget(emptyHost);
    gridContentStack_->addWidget(photoGrid_);
    gridVBox->addWidget(gridContentStack_, 1);

    trayBar_ = new QWidget(gridContainer);
    trayBar_->setObjectName(QStringLiteral("trayBar"));
    trayBar_->setFixedHeight(56);
    trayBar_->setVisible(false);
    auto* trayLayout = new QHBoxLayout(trayBar_);
    trayLayout->setContentsMargins(16, 0, 16, 0);
    trayLayout->setSpacing(8);

    trayInfoLabel_ = new QLabel(trayBar_);

    trayStarBtn_ = new QPushButton(trayBar_);
    trayStarBtn_->setObjectName(QStringLiteral("starBtnActive"));
    trayStarBtn_->setFixedHeight(kBtnH);
    trayStarBtn_->setIcon(makeUnifiedStarIcon(true, 16));
    trayUnstarBtn_ = new QPushButton(trayBar_);
    trayUnstarBtn_->setFixedHeight(kBtnH);
    trayUnstarBtn_->setIcon(makeUnifiedStarIcon(false, 16));

    albumLbl_ = new QLabel(trayBar_);
    trayAlbumCombo_ = new QComboBox(trayBar_);
    trayAlbumCombo_->setMinimumWidth(140);
    trayAlbumCombo_->setFixedHeight(kBtnH);
    trayAddToAlbumBtn_ = new QPushButton(trayBar_);
    trayAddToAlbumBtn_->setFixedHeight(kBtnH);
    trayNewAlbBtn_ = new QPushButton(trayBar_);
    trayNewAlbBtn_->setFixedHeight(kBtnH);

    openSelectedBtn_ = new QPushButton(trayBar_);
    openSelectedBtn_->setObjectName(QStringLiteral("primaryBtn"));
    openSelectedBtn_->setFixedHeight(kBtnH);
    clearSelBtn_ = new QPushButton(trayBar_);
    clearSelBtn_->setFixedHeight(kBtnH);

    trayLayout->addWidget(trayInfoLabel_);
    trayLayout->addSpacing(6);
    trayLayout->addWidget(trayStarBtn_);
    trayLayout->addWidget(trayUnstarBtn_);
    trayLayout->addStretch();
    trayLayout->addWidget(albumLbl_);
    trayLayout->addWidget(trayAlbumCombo_);
    trayLayout->addWidget(trayAddToAlbumBtn_);
    trayLayout->addWidget(trayNewAlbBtn_);
    trayLayout->addSpacing(6);
    trayLayout->addWidget(openSelectedBtn_);
    trayLayout->addWidget(clearSelBtn_);
    gridVBox->addWidget(trayBar_);

    libSplitter_->addWidget(sidebarList_);
    libSplitter_->addWidget(gridContainer);
    libSplitter_->setCollapsible(0, false);
    libSplitter_->setCollapsible(1, false);
    libSplitter_->setStretchFactor(0, 0);
    libSplitter_->setStretchFactor(1, 1);
    {
        QSettings st(QStringLiteral("Lumina"), QStringLiteral("LuminaPhoto"));
        const int savedW = std::clamp(st.value(QStringLiteral("sidebar/width"), 256).toInt(), 68, 420);
        libSplitter_->setSizes({savedW, std::max(400, 1440 - savedW)});
    }
    connect(libSplitter_, &QSplitter::splitterMoved, this, [this](int, int) {
        if (sidebarList_) {
            QSettings st(QStringLiteral("Lumina"), QStringLiteral("LuminaPhoto"));
            st.setValue(QStringLiteral("sidebar/width"), sidebarList_->width());
        }
        updateSidebarCompactMode();
    });
    libRoot->addWidget(libSplitter_, 1);
    mainStack_->addWidget(libraryPage);

    // ==================== PAGE 1: PICASA-STYLE + PHOTOSHOP LAYER EDITOR ====================
    auto* editorPage = new QWidget(mainStack_);
    auto* edRoot = new QVBoxLayout(editorPage);
    edRoot->setContentsMargins(0, 0, 0, 0);
    edRoot->setSpacing(0);

    edTop_ = new QWidget(editorPage);
    edTop_->setObjectName(QStringLiteral("edTop"));
    edTop_->setFixedHeight(58);
    auto* edTopLayout = new QHBoxLayout(edTop_);
    edTopLayout->setContentsMargins(14, 6, 14, 6);
    edTopLayout->setSpacing(6);

    backBtn_ = new QPushButton(edTop_);
    backBtn_->setObjectName(QStringLiteral("backBtn"));
    backBtn_->setIconSize(QSize(16, 16));
    backBtn_->setFixedHeight(kBtnH);
    backBtn_->setFocusPolicy(Qt::NoFocus);

    edPrevBtn_ = new QPushButton(edTop_);
    edPrevBtn_->setObjectName(QStringLiteral("iconSquareBtn"));
    edPrevBtn_->setFixedSize(kBtnH, kBtnH);
    edPrevBtn_->setFocusPolicy(Qt::NoFocus);

    edNextBtn_ = new QPushButton(edTop_);
    edNextBtn_->setObjectName(QStringLiteral("iconSquareBtn"));
    edNextBtn_->setFixedSize(kBtnH, kBtnH);
    edNextBtn_->setFocusPolicy(Qt::NoFocus);

    editorFilmstrip_ = new QListWidget(edTop_);
    editorFilmstrip_->setObjectName(QStringLiteral("edFilmstrip"));
    editorFilmstrip_->setFocusPolicy(Qt::NoFocus);
    editorFilmstrip_->setFlow(QListView::LeftToRight);
    editorFilmstrip_->setViewMode(QListView::IconMode);
    editorFilmstrip_->setWrapping(false);
    editorFilmstrip_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editorFilmstrip_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editorFilmstrip_->setIconSize(QSize(44, 32));
    editorFilmstrip_->setGridSize(QSize(52, 40));
    editorFilmstrip_->setUniformItemSizes(true);
    editorFilmstrip_->setFixedHeight(40);
    editorFilmstrip_->setMinimumWidth(110);
    editorFilmstrip_->setMaximumWidth(360);
    editorFilmstripDelegate_ = new FilmstripDelegate(AppSettings::instance().effectiveDarkTheme(), editorFilmstrip_);
    editorFilmstrip_->setItemDelegate(editorFilmstripDelegate_);

    editorCompareBtn_ = new QPushButton(edTop_);
    editorCompareBtn_->setCheckable(true);
    editorCompareBtn_->setChecked(false);
    editorCompareBtn_->setFixedHeight(kBtnH);
    editorCompareBtn_->setFocusPolicy(Qt::NoFocus);

    editorZoomBtn_ = new QPushButton(QStringLiteral("1:1"), edTop_);
    editorZoomBtn_->setFixedHeight(kBtnH);
    editorZoomBtn_->setFocusPolicy(Qt::NoFocus);

    edBackdropBtn_ = new QPushButton(edTop_);
    edBackdropBtn_->setFixedHeight(kBtnH);
    edBackdropBtn_->setFocusPolicy(Qt::NoFocus);
    edBackdropBtn_->setCheckable(true);
    edBackdropBtn_->setChecked(false);

    undoBtn_ = new QPushButton(edTop_);
    undoBtn_->setFixedHeight(kBtnH);
    undoBtn_->setFocusPolicy(Qt::NoFocus);
    undoBtn_->setEnabled(false);

    resetEditsBtn_ = new QPushButton(edTop_);
    resetEditsBtn_->setFixedHeight(kBtnH);
    resetEditsBtn_->setFocusPolicy(Qt::NoFocus);

    edRightToggleBtn_ = new QPushButton(edTop_);
    edRightToggleBtn_->setCheckable(true);
    edRightToggleBtn_->setChecked(true);
    edRightToggleBtn_->setFixedHeight(kBtnH);
    edRightToggleBtn_->setFocusPolicy(Qt::NoFocus);

    exportBtn_ = new QPushButton(edTop_);
    exportBtn_->setObjectName(QStringLiteral("primaryBtn"));
    exportBtn_->setFixedHeight(kBtnH);
    exportBtn_->setFocusPolicy(Qt::NoFocus);

    edSettingsBtn_ = new QPushButton(edTop_);
    edSettingsBtn_->setObjectName(QStringLiteral("iconSquareBtn"));
    edSettingsBtn_->setFixedSize(kBtnH, kBtnH);
    edSettingsBtn_->setFocusPolicy(Qt::NoFocus);

    edTopLayout->addWidget(backBtn_);
    edTopLayout->addStretch();
    edTopLayout->addWidget(edPrevBtn_);
    edTopLayout->addWidget(editorFilmstrip_);
    edTopLayout->addWidget(edNextBtn_);
    edTopLayout->addStretch();
    edTopLayout->addWidget(editorCompareBtn_);
    edTopLayout->addWidget(editorZoomBtn_);
    edTopLayout->addWidget(edBackdropBtn_);
    edTopLayout->addWidget(undoBtn_);
    edTopLayout->addWidget(resetEditsBtn_);
    edTopLayout->addWidget(edRightToggleBtn_);
    edTopLayout->addWidget(exportBtn_);
    edTopLayout->addWidget(edSettingsBtn_);
    edRoot->addWidget(edTop_);

    edBody_ = new QSplitter(Qt::Horizontal, editorPage);
    edBody_->setHandleWidth(5);
    edBody_->setChildrenCollapsible(false);

    // ---- LEFT PANE: 5 Picasa Tabs + Contextual Active Layer Settings (Resizable 8%..40%) ----
    edLeftPane_ = new QWidget(edBody_);
    edLeftPane_->setObjectName(QStringLiteral("edLeftPane"));
    edLeftPane_->installEventFilter(this);
    auto* edLeftLay = new QVBoxLayout(edLeftPane_);
    edLeftLay->setContentsMargins(0, 0, 0, 0);
    edLeftLay->setSpacing(0);

    edTabs_ = new QTabWidget(edLeftPane_);
    edTabs_->setObjectName(QStringLiteral("edTabs"));
    edTabs_->setDocumentMode(true);
    edTabs_->tabBar()->setFocusPolicy(Qt::NoFocus);
    edTabs_->tabBar()->setExpanding(true);
    edTabs_->tabBar()->setUsesScrollButtons(true);
    edTabs_->tabBar()->setElideMode(Qt::ElideNone);

    auto makeScrollableTab = [&](QWidget*& innerOut, QVBoxLayout*& layOut) {
        auto* scroll = new QScrollArea(edTabs_);
        scroll->setObjectName(QStringLiteral("tabScroll"));
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        innerOut = new QWidget(scroll);
        innerOut->setObjectName(QStringLiteral("tabInner"));
        layOut = new QVBoxLayout(innerOut);
        layOut->setContentsMargins(8, 8, 8, 12);
        layOut->setSpacing(6);
        scroll->setWidget(innerOut);
        return scroll;
    };

    // Tab 0: Базовые
    QVBoxLayout* basicLay = nullptr;
    QScrollArea* tabBasicScroll = makeScrollableTab(tabBasicInner_, basicLay);

    basicTilesList_ = new QListWidget(tabBasicInner_);
    basicTilesList_->setObjectName(QStringLiteral("edVisualGrid"));
    basicTilesList_->setFocusPolicy(Qt::NoFocus);
    basicTilesList_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    basicTilesList_->setViewMode(QListView::IconMode);
    basicTilesList_->setResizeMode(QListView::Adjust);
    basicTilesList_->setMovement(QListView::Static);
    basicTilesList_->setWrapping(true);
    basicTilesList_->setWordWrap(true);
    basicTilesList_->setTextElideMode(Qt::ElideNone);
    basicTilesList_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    basicTilesList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    basicTilesList_->setIconSize(QSize(78, 48));
    basicTilesList_->setGridSize(QSize(102, 82));
    basicTilesList_->setSpacing(0);
    basicTilesList_->setFixedHeight(252);
    basicLay->addWidget(basicTilesList_);

    cropBarWidget_ = new QWidget(tabBasicInner_);
    cropBarWidget_->setObjectName(QStringLiteral("cropBarWidget"));
    cropBarWidget_->setVisible(false);
    auto* cropBarLay = new QVBoxLayout(cropBarWidget_);
    cropBarLay->setContentsMargins(8, 8, 8, 8);
    cropBarLay->setSpacing(6);
    cropHint_ = new QLabel(cropBarWidget_);
    cropHint_->setWordWrap(true);
    auto* cropBtnsRow = new QHBoxLayout();
    applyCropBtn_ = new QPushButton(cropBarWidget_);
    applyCropBtn_->setObjectName(QStringLiteral("primaryBtn"));
    applyCropBtn_->setFixedHeight(30);
    resetCropBtn_ = new QPushButton(cropBarWidget_);
    resetCropBtn_->setFixedHeight(30);
    cropBtnsRow->addWidget(applyCropBtn_);
    cropBtnsRow->addWidget(resetCropBtn_);
    cropBarLay->addWidget(cropHint_);
    cropBarLay->addLayout(cropBtnsRow);
    basicLay->addWidget(cropBarWidget_);

    basicAdjustWidget_ = new QWidget(tabBasicInner_);
    basicAdjustWidget_->setVisible(false);
    auto* basicAdjLay = new QVBoxLayout(basicAdjustWidget_);
    basicAdjLay->setContentsMargins(0, 4, 0, 0);
    basicAdjLay->setSpacing(6);

    straightenRow_ = new MagneticSliderRow(QString(), -25, 25, 0, QStringLiteral("°"), basicAdjustWidget_);
    resetBasicBtn_ = new QPushButton(basicAdjustWidget_);
    resetBasicBtn_->setFixedHeight(32);

    basicAdjLay->addWidget(straightenRow_);
    basicAdjLay->addSpacing(2);
    basicAdjLay->addWidget(resetBasicBtn_);
    basicLay->addWidget(basicAdjustWidget_);
    basicLay->addStretch(1);

    // Tab 1: Свет и цвет
    QVBoxLayout* tuningLay = nullptr;
    QScrollArea* tabTuningScroll = makeScrollableTab(tabTuningInner_, tuningLay);

    brightnessRow_ = new MagneticSliderRow(QString(), -100, 150, 0, QString(), tabTuningInner_);
    highlightsRow_ = new MagneticSliderRow(QString(), -100, 100, 0, QString(), tabTuningInner_);
    shadowsRow_ = new MagneticSliderRow(QString(), -100, 100, 0, QString(), tabTuningInner_);
    contrastRow_ = new MagneticSliderRow(QString(), -100, 100, 0, QString(), tabTuningInner_);
    saturationRow_ = new MagneticSliderRow(QString(), -100, 150, 0, QString(), tabTuningInner_);
    tempRow_ = new MagneticSliderRow(QString(), -100, 100, 0, QString(), tabTuningInner_);
    tintRow_ = new MagneticSliderRow(QString(), -100, 100, 0, QString(), tabTuningInner_);
    hueShiftRow_ = new MagneticSliderRow(QString(), -180, 180, 0, QStringLiteral("°"), tabTuningInner_);

    auto* wbRow = new QHBoxLayout();
    eyedropperBtn_ = new QPushButton(tabTuningInner_);
    eyedropperBtn_->setCheckable(true);
    eyedropperBtn_->setFixedHeight(32);
    autoColorBtn_ = new QPushButton(tabTuningInner_);
    autoColorBtn_->setFixedHeight(32);
    wbRow->addWidget(eyedropperBtn_, 1);
    wbRow->addWidget(autoColorBtn_);

    resetTuningBtn_ = new QPushButton(tabTuningInner_);
    resetTuningBtn_->setFixedHeight(32);

    tuningLay->addWidget(brightnessRow_);
    tuningLay->addWidget(highlightsRow_);
    tuningLay->addWidget(shadowsRow_);
    tuningLay->addWidget(contrastRow_);
    tuningLay->addWidget(saturationRow_);
    tuningLay->addWidget(tempRow_);
    tuningLay->addWidget(tintRow_);
    tuningLay->addWidget(hueShiftRow_);
    tuningLay->addLayout(wbRow);
    tuningLay->addWidget(resetTuningBtn_);
    tuningLay->addStretch(1);

    auto makeEffectsGridTab = [&](QWidget*& innerOut, QListWidget*& listOut) {
        QVBoxLayout* lay = nullptr;
        QScrollArea* scroll = makeScrollableTab(innerOut, lay);
        listOut = new QListWidget(innerOut);
        listOut->setObjectName(QStringLiteral("edVisualGrid"));
        listOut->setFocusPolicy(Qt::NoFocus);
        listOut->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        listOut->setViewMode(QListView::IconMode);
        listOut->setResizeMode(QListView::Adjust);
        listOut->setMovement(QListView::Static);
        listOut->setWrapping(true);
        listOut->setWordWrap(true);
        listOut->setTextElideMode(Qt::ElideNone);
        listOut->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        listOut->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        listOut->setIconSize(QSize(78, 48));
        listOut->setGridSize(QSize(102, 86));
        listOut->setSpacing(0);
        lay->addWidget(listOut);
        lay->addStretch(1);
        return scroll;
    };

    QScrollArea* tabProcessScroll = makeEffectsGridTab(tabProcessInner_, processEffectsList_);
    QScrollArea* tabArtScroll = makeEffectsGridTab(tabArtInner_, artEffectsList_);
    QScrollArea* tabFramesScroll = makeEffectsGridTab(tabFramesInner_, frameEffectsList_);

    edTabs_->addTab(tabBasicScroll, QString());
    edTabs_->addTab(tabTuningScroll, QString());
    edTabs_->addTab(tabProcessScroll, QString());
    edTabs_->addTab(tabArtScroll, QString());
    edTabs_->addTab(tabFramesScroll, QString());
    edLeftLay->addWidget(edTabs_, 0);
    connect(edTabs_, &QTabWidget::currentChanged, this, [this](int) {
        if (edLeftPane_) {
            QTimer::singleShot(0, this, [this]() {
                if (edLeftPane_) adaptLeftPaneLayout(edLeftPane_->width());
            });
        }
    });

    // Contextual Selected Effect / Text Settings Box
    activeEffectBox_ = new QWidget(edLeftPane_);
    activeEffectBox_->setObjectName(QStringLiteral("activeEffectBox"));
    activeEffectBox_->setVisible(false);
    auto* effBoxLay = new QVBoxLayout(activeEffectBox_);
    effBoxLay->setContentsMargins(12, 10, 12, 12);
    effBoxLay->setSpacing(6);

    auto* effHead = new QHBoxLayout();
    activeEffectTitle_ = new QLabel(activeEffectBox_);
    removeSelectedEffBtn_ = new QPushButton(activeEffectBox_);
    removeSelectedEffBtn_->setFixedHeight(28);
    effHead->addWidget(activeEffectTitle_, 1);
    effHead->addWidget(removeSelectedEffBtn_);

    textControlsWidget_ = new QWidget(activeEffectBox_);
    textControlsWidget_->setVisible(false);
    auto* textCtrlLay = new QVBoxLayout(textControlsWidget_);
    textCtrlLay->setContentsMargins(0, 0, 0, 0);
    textCtrlLay->setSpacing(6);
    textDragHint_ = new QLabel(textControlsWidget_);
    textDragHint_->setWordWrap(true);
    textLayerEdit_ = new QLineEdit(textControlsWidget_);
    textLayerEdit_->setFixedHeight(32);
    textLayerSizeRow_ = new MagneticSliderRow(QString(), 12, 120, 36, QStringLiteral(" px"), textControlsWidget_);
    auto* textBtnsRow = new QHBoxLayout();
    textBtnsRow->setSpacing(6);
    textColorBtn_ = new QPushButton(textControlsWidget_);
    textColorBtn_->setFixedHeight(30);
    addAnotherTextBtn_ = new QPushButton(textControlsWidget_);
    addAnotherTextBtn_->setFixedHeight(30);
    textBtnsRow->addWidget(textColorBtn_, 1);
    textBtnsRow->addWidget(addAnotherTextBtn_, 1);
    textCtrlLay->addWidget(textDragHint_);
    textCtrlLay->addWidget(textLayerEdit_);
    textCtrlLay->addWidget(textLayerSizeRow_);
    textCtrlLay->addLayout(textBtnsRow);

    effectIntensityRow_ = new MagneticSliderRow(QString(), 0, 100, 80, QStringLiteral("%"), activeEffectBox_);
    effectParam2Row_ = new MagneticSliderRow(QString(), 0, 100, 50, QString(), activeEffectBox_);
    effectParam2Row_->setVisible(false);

    effBoxLay->addLayout(effHead);
    effBoxLay->addWidget(textControlsWidget_);
    effBoxLay->addWidget(effectIntensityRow_);
    effBoxLay->addWidget(effectParam2Row_);
    edLeftLay->addWidget(activeEffectBox_, 0);
    edLeftLay->addStretch(1);

    // ---- CENTER: INTERACTIVE CANVAS + CAPTION + BOTTOM STATUS BAR ----
    auto* centerWrapper = new QWidget(edBody_);
    auto* centerVBox = new QVBoxLayout(centerWrapper);
    centerVBox->setContentsMargins(0, 0, 0, 0);
    centerVBox->setSpacing(0);

    auto* canvasPane = new QWidget(centerWrapper);
    canvasPane->setObjectName(QStringLiteral("canvasPane"));
    auto* canvasLay = new QVBoxLayout(canvasPane);
    canvasLay->setContentsMargins(14, 14, 14, 10);
    canvasLay->setSpacing(10);

    editorCanvas_ = new EditorCanvasWidget(canvasPane);

    cropCanvasBar_ = new QWidget(canvasPane);
    cropCanvasBar_->setObjectName(QStringLiteral("cropCanvasBar"));
    cropCanvasBar_->setVisible(false);
    cropCanvasBar_->setStyleSheet(QStringLiteral(
        "QWidget#cropCanvasBar { background-color: rgba(15, 23, 42, 235); border: 1px solid #A855F7; border-radius: 10px; }"
    ));
    auto* ccLay = new QHBoxLayout(cropCanvasBar_);
    ccLay->setContentsMargins(12, 8, 12, 8);
    ccLay->setSpacing(8);
    ccTitle_ = new QLabel(cropCanvasBar_);
    ccTitle_->setStyleSheet(QStringLiteral("color: #F8FAFC; font-weight: 700; font-size: 12px;"));
    cropCanvasInfoLabel_ = new QLabel(cropCanvasBar_);
    cropCanvasInfoLabel_->setStyleSheet(QStringLiteral("color: #E9D5FF; font-size: 12px; font-weight: 600;"));
    canvasApplyCropBtn_ = new QPushButton(cropCanvasBar_);
    canvasApplyCropBtn_->setObjectName(QStringLiteral("primaryBtn"));
    canvasApplyCropBtn_->setFixedHeight(30);
    canvasApplyCropBtn_->setFocusPolicy(Qt::NoFocus);
    canvasResetCropBtn_ = new QPushButton(cropCanvasBar_);
    canvasResetCropBtn_->setFixedHeight(30);
    canvasResetCropBtn_->setFocusPolicy(Qt::NoFocus);
    canvasCloseCropBtn_ = new QPushButton(cropCanvasBar_);
    canvasCloseCropBtn_->setFixedHeight(30);
    canvasCloseCropBtn_->setFocusPolicy(Qt::NoFocus);
    ccLay->addWidget(ccTitle_);
    ccLay->addSpacing(6);
    ccLay->addWidget(cropCanvasInfoLabel_, 1);
    ccLay->addWidget(canvasApplyCropBtn_);
    ccLay->addWidget(canvasResetCropBtn_);
    ccLay->addWidget(canvasCloseCropBtn_);

    editorCaptionEdit_ = new QLineEdit(canvasPane);
    editorCaptionEdit_->setAlignment(Qt::AlignCenter);
    editorCaptionEdit_->setStyleSheet(QStringLiteral(
        "background-color: rgba(255, 255, 255, 18); color: #F4F4F5; border: 1px solid rgba(255, 255, 255, 28); border-radius: 8px; padding: 7px 12px; font-size: 13px;"
    ));

    canvasLay->addWidget(editorCanvas_, 1);
    canvasLay->addWidget(cropCanvasBar_);
    canvasLay->addWidget(editorCaptionEdit_);
    centerVBox->addWidget(canvasPane, 1);

    edBottomBar_ = new QWidget(centerWrapper);
    edBottomBar_->setObjectName(QStringLiteral("edBottomBar"));
    edBottomBar_->setFixedHeight(48);
    auto* edBotLay = new QHBoxLayout(edBottomBar_);
    edBotLay->setContentsMargins(16, 0, 16, 0);
    edBotLay->setSpacing(8);

    editorStarBtn_ = new QPushButton(edBottomBar_);
    editorStarBtn_->setObjectName(QStringLiteral("starBtn"));
    editorStarBtn_->setCheckable(true);
    editorStarBtn_->setFixedHeight(32);
    editorStarBtn_->setFocusPolicy(Qt::NoFocus);
    editorStarBtn_->setIcon(makeUnifiedStarIcon(false, 16));

    rotLeftBtn_ = new QPushButton(QStringLiteral("-90°"), edBottomBar_);
    rotLeftBtn_->setFixedHeight(32);
    rotLeftBtn_->setFocusPolicy(Qt::NoFocus);
    rotRightBtn_ = new QPushButton(QStringLiteral("+90°"), edBottomBar_);
    rotRightBtn_->setFixedHeight(32);
    rotRightBtn_->setFocusPolicy(Qt::NoFocus);
    editorMetaStatus_ = new QLabel(edBottomBar_);

    edBotLay->addWidget(editorStarBtn_);
    edBotLay->addWidget(rotLeftBtn_);
    edBotLay->addWidget(rotRightBtn_);
    edBotLay->addSpacing(8);
    edBotLay->addWidget(editorMetaStatus_);
    edBotLay->addStretch();
    centerVBox->addWidget(edBottomBar_);

    // ---- RIGHT PANE: PHOTOSHOP EFFECT STACK + HISTOGRAM + EXIF & AI METADATA + PSD LAYERS ----
    edRightPane_ = new QWidget(edBody_);
    edRightPane_->setObjectName(QStringLiteral("edRightPane"));
    auto* edRightOuterLay = new QVBoxLayout(edRightPane_);
    edRightOuterLay->setContentsMargins(0, 0, 0, 0);
    edRightOuterLay->setSpacing(0);

    auto* rightScroll = new QScrollArea(edRightPane_);
    rightScroll->setObjectName(QStringLiteral("rightInspectorScroll"));
    rightScroll->setWidgetResizable(true);
    rightScroll->setFrameShape(QFrame::NoFrame);
    rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    rightInner_ = new QWidget(rightScroll);
    rightInner_->setObjectName(QStringLiteral("rightInner"));
    auto* edRightLay = new QVBoxLayout(rightInner_);
    edRightLay->setContentsMargins(12, 12, 12, 12);
    edRightLay->setSpacing(7);

    stackHeader_ = new QLabel(rightInner_);
    stackHint_ = new QLabel(rightInner_);
    stackHint_->setWordWrap(true);

    effectStackList_ = new QListWidget(rightInner_);
    effectStackList_->setObjectName(QStringLiteral("effectStackList"));
    effectStackList_->setFocusPolicy(Qt::NoFocus);
    effectStackList_->setMinimumHeight(130);

    auto* stackBtnsRow = new QHBoxLayout();
    layerUpBtn_ = new QPushButton(rightInner_);
    layerUpBtn_->setFixedHeight(30);
    layerUpBtn_->setFocusPolicy(Qt::NoFocus);
    layerDownBtn_ = new QPushButton(rightInner_);
    layerDownBtn_->setFixedHeight(30);
    layerDownBtn_->setFocusPolicy(Qt::NoFocus);
    layerDeleteBtn_ = new QPushButton(rightInner_);
    layerDeleteBtn_->setFixedHeight(30);
    layerDeleteBtn_->setFocusPolicy(Qt::NoFocus);
    stackBtnsRow->addWidget(layerUpBtn_);
    stackBtnsRow->addWidget(layerDownBtn_);
    stackBtnsRow->addWidget(layerDeleteBtn_);

    histTitle_ = new QLabel(rightInner_);
    histogramWidget_ = new HistogramWidget(rightInner_);

    editorExifLabel_ = new QLabel(rightInner_);
    editorExifLabel_->setWordWrap(true);

    metadataTextEdit_ = new QPlainTextEdit(rightInner_);
    metadataTextEdit_->setObjectName(QStringLiteral("metaBox"));
    metadataTextEdit_->setReadOnly(true);
    metadataTextEdit_->setFixedHeight(96);

    auto* metaBtnsRow = new QHBoxLayout();
    copyMetaBtn_ = new QPushButton(rightInner_);
    copyMetaBtn_->setFixedHeight(32);
    stripMetaBtn_ = new QPushButton(rightInner_);
    stripMetaBtn_->setObjectName(QStringLiteral("dangerBtn"));
    stripMetaBtn_->setCheckable(true);
    stripMetaBtn_->setChecked(false);
    stripMetaBtn_->setFixedHeight(32);
    metaBtnsRow->addWidget(copyMetaBtn_);
    metaBtnsRow->addWidget(stripMetaBtn_);

    metaHintLabel_ = new QLabel(rightInner_);
    metaHintLabel_->setWordWrap(true);

    editorLayersTitle_ = new QLabel(rightInner_);
    editorLayersTitle_->setVisible(false);
    editorPsdLayers_ = new QListWidget(rightInner_);
    editorPsdLayers_->setObjectName(QStringLiteral("edPsdLayers"));
    editorPsdLayers_->setFocusPolicy(Qt::NoFocus);
    editorPsdLayers_->setIconSize(QSize(34, 34));
    editorPsdLayers_->setMaximumHeight(140);
    editorPsdLayers_->setVisible(false);
    editorExportPsdLayerBtn_ = new QPushButton(rightInner_);
    editorExportPsdLayerBtn_->setFixedHeight(32);
    editorExportPsdLayerBtn_->setFocusPolicy(Qt::NoFocus);
    editorExportPsdLayerBtn_->setVisible(false);
    editorExportPsdLayerBtn_->setEnabled(false);

    edRightLay->addWidget(stackHeader_);
    edRightLay->addWidget(stackHint_);
    edRightLay->addWidget(effectStackList_, 1);
    edRightLay->addLayout(stackBtnsRow);
    edRightLay->addSpacing(4);
    edRightLay->addWidget(histTitle_);
    edRightLay->addWidget(histogramWidget_);
    edRightLay->addWidget(editorExifLabel_);
    edRightLay->addWidget(metadataTextEdit_);
    edRightLay->addLayout(metaBtnsRow);
    edRightLay->addWidget(metaHintLabel_);
    edRightLay->addWidget(editorLayersTitle_);
    edRightLay->addWidget(editorPsdLayers_);
    edRightLay->addWidget(editorExportPsdLayerBtn_);

    rightScroll->setWidget(rightInner_);
    edRightOuterLay->addWidget(rightScroll);

    edBody_->addWidget(edLeftPane_);
    edBody_->addWidget(centerWrapper);
    edBody_->addWidget(edRightPane_);
    edBody_->setStretchFactor(0, 0);
    edBody_->setStretchFactor(1, 1);
    edBody_->setStretchFactor(2, 0);
    updateEditorPaneConstraints();
    edRoot->addWidget(edBody_, 1);
    mainStack_->addWidget(editorPage);

    // ==================== SIGNALS ====================
    connect(addFolderBtn_, &QPushButton::clicked, this, &MainWindow::onAddFolderClicked);
    connect(emptyAddBtn_, &QPushButton::clicked, this, &MainWindow::onAddFolderClicked);
    connect(emptyScanPicsBtn_, &QPushButton::clicked, this, &MainWindow::onScanPicturesClicked);
    connect(newAlbumBtn_, &QPushButton::clicked, this, &MainWindow::onCreateAlbumClicked);
    connect(trayNewAlbBtn_, &QPushButton::clicked, this, &MainWindow::onCreateAlbumClicked);
    connect(assocBtn_, &QPushButton::clicked, this, &MainWindow::onRegisterAssociationsClicked);
    connect(libSettingsBtn_, &QPushButton::clicked, this, &MainWindow::openSettingsDialog);
    connect(edSettingsBtn_, &QPushButton::clicked, this, &MainWindow::openSettingsDialog);

    connect(sidebarList_, &QListWidget::itemClicked, this, &MainWindow::onSidebarItemActivated);
    connect(sidebarList_, &QListWidget::customContextMenuRequested, this, &MainWindow::showSidebarContextMenu);

    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString& txt) {
        searchQuery_ = txt;
        reloadPhotoGrid();
    });
    connect(starredFilterBtn_, &QPushButton::toggled, this, [this](bool on) {
        onlyStarredFilter_ = on;
        starredFilterBtn_->setIcon(makeUnifiedStarIcon(on, 16));
        reloadSidebar();
        reloadPhotoGrid();
    });
    connect(thumbSlider_, &QSlider::valueChanged, this, [this](int val) {
        thumbSize_ = val;
        photoGrid_->setIconSize(QSize(thumbSize_, thumbSize_ * 3 / 4));
        photoGrid_->setGridSize(QSize(thumbSize_ + 22, thumbSize_ * 3 / 4 + 44));
        prioritizeVisibleThumbnails();
    });

    connect(photoGrid_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        prioritizeVisibleThumbnails();
    });
    connect(photoGrid_, &PhotoGridWidget::cardStarClicked, this, &MainWindow::toggleRowStarred);
    connect(photoGrid_, &PhotoGridWidget::cardPlusClicked, this, &MainWindow::showRowAlbumMenu);
    connect(photoGrid_, &QListWidget::itemSelectionChanged, this, [this]() {
        const int cnt = photoGrid_->selectedItems().size();
        trayBar_->setVisible(cnt > 0);
        trayInfoLabel_->setText(trUi(QString("Выбрано: %1").arg(cnt), QString("Selected: %1").arg(cnt)));
    });
    connect(photoGrid_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        const int row = photoGrid_->row(item);
        if (row >= 0 && row < currentPhotos_.size()) {
            onOpenEditorForRecord(currentPhotos_[row]);
        }
    });
    connect(photoGrid_, &QListWidget::customContextMenuRequested, this, &MainWindow::showPhotoGridContextMenu);

    connect(trayStarBtn_, &QPushButton::clicked, this, [this]() { setSelectedStarred(true); });
    connect(trayUnstarBtn_, &QPushButton::clicked, this, [this]() { setSelectedStarred(false); });
    connect(trayAddToAlbumBtn_, &QPushButton::clicked, this, &MainWindow::onAddSelectedToAlbum);
    connect(openSelectedBtn_, &QPushButton::clicked, this, [this]() {
        const auto sel = photoGrid_->selectedItems();
        if (!sel.isEmpty()) {
            const int row = photoGrid_->row(sel.first());
            if (row >= 0 && row < currentPhotos_.size()) {
                onOpenEditorForRecord(currentPhotos_[row]);
            }
        }
    });
    connect(clearSelBtn_, &QPushButton::clicked, photoGrid_, &QListWidget::clearSelection);

    // Editor navigation & filmstrip
    connect(backBtn_, &QPushButton::clicked, this, [this]() {
        stopEditorMovie();
        ++editorLoadSeq_;
        ++thumbJobSeq_;
        if (dbSaveTimer_ && dbSaveTimer_->isActive()) {
            dbSaveTimer_->stop();
            if (!editingRecord_.filePath.isEmpty()) {
                db_->setEdits(editingRecord_.filePath, editingRecord_.edits);
            }
        }
        setCropEditingEnabled(false, false);
        editorCanvas_->setLiveTextLayer(nullptr);
        editorCanvas_->setEyedropperMode(false);
        eyedropperBtn_->setChecked(false);
        mainStack_->setCurrentIndex(0);
        reloadSidebar();
        reloadPhotoGrid();
    });
    connect(edPrevBtn_, &QPushButton::clicked, this, [this]() { openRelativePhotoInEditor(-1); });
    connect(edNextBtn_, &QPushButton::clicked, this, [this]() { openRelativePhotoInEditor(1); });
    connect(editorFilmstrip_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < currentPhotos_.size() &&
            currentPhotos_[row].filePath != editingRecord_.filePath) {
            onOpenEditorForRecord(currentPhotos_[row]);
        }
        this->setFocus();
    });
    connect(editorFilmstrip_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        const int row = editorFilmstrip_->row(item);
        if (row >= 0 && row < currentPhotos_.size() &&
            currentPhotos_[row].filePath != editingRecord_.filePath) {
            onOpenEditorForRecord(currentPhotos_[row]);
        }
        this->setFocus();
    });

    connect(edBody_, &QSplitter::splitterMoved, this, [this](int, int) {
        if (edLeftPane_) {
            adaptLeftPaneLayout(edLeftPane_->width());
        }
    });
    connect(edRightToggleBtn_, &QPushButton::toggled, this, [this](bool visible) {
        if (edRightPane_) {
            edRightPane_->setVisible(visible);
        }
    });

    connect(editorCompareBtn_, &QPushButton::toggled, this, [this](bool checked) {
        compareOriginal_ = checked;
        refreshEditorPreview(false);
        this->setFocus();
    });
    connect(editorZoomBtn_, &QPushButton::clicked, this, [this]() {
        const bool next = !editorCanvas_->isActualSize();
        editorCanvas_->setActualSizeMode(next);
        refreshEditorPreview(false);
        this->setFocus();
    });
    connect(editorCanvas_, &EditorCanvasWidget::zoomStateChanged, this, [this](bool actual) {
        editorZoomBtn_->setText(actual ? trUi(QStringLiteral("Вписать"), QStringLiteral("Fit")) : QStringLiteral("1:1"));
    });
    connect(edBackdropBtn_, &QPushButton::clicked, this, [this]() {
        cycleBackdropMode();
        this->setFocus();
    });

    connect(editorStarBtn_, &QPushButton::toggled, this, [this](bool checked) {
        if (editingRecord_.filePath.isEmpty()) return;
        editingRecord_.starred = checked;
        db_->setStarred(editingRecord_.filePath, checked);
        editorStarBtn_->setIcon(makeUnifiedStarIcon(checked, 16));
        editorStarBtn_->setText(checked
                                    ? trUi(QStringLiteral("В избранном"), QStringLiteral("Favorited"))
                                    : trUi(QStringLiteral("Избранное"), QStringLiteral("Favorite")));
    });
    connect(rotLeftBtn_, &QPushButton::clicked, this, [this]() {
        pushUndoState();
        editingRecord_.edits.rotationDeg = (editingRecord_.edits.rotationDeg + 270) % 360;
        saveEditorEdits();
    });
    connect(rotRightBtn_, &QPushButton::clicked, this, [this]() {
        pushUndoState();
        editingRecord_.edits.rotationDeg = (editingRecord_.edits.rotationDeg + 90) % 360;
        saveEditorEdits();
    });

    connect(undoBtn_, &QPushButton::clicked, this, &MainWindow::undoLastEdit);
    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    connect(undoShortcut, &QShortcut::activated, this, [this]() {
        if (mainStack_->currentIndex() == 1) {
            undoLastEdit();
        }
    });

    connect(resetEditsBtn_, &QPushButton::clicked, this, [this]() {
        if (editingRecord_.edits.isDefault()) return;
        pushUndoState();
        editingRecord_.edits = PhotoEdits{};
        selectedLayerId_.clear();
        {
            const QSignalBlocker b(editorCompareBtn_);
            editorCompareBtn_->setChecked(false);
        }
        compareOriginal_ = false;
        if (basicAdjustWidget_) basicAdjustWidget_->setVisible(false);
        setCropEditingEnabled(false, true);
        editorCanvas_->setLiveTextLayer(nullptr);
        syncEditorSliders();
        populateEffectStackList();
        updateActiveEffectControls();
        saveEditorEdits();
    });
    connect(exportBtn_, &QPushButton::clicked, this, &MainWindow::exportEditedPhoto);
    connect(copyMetaBtn_, &QPushButton::clicked, this, &MainWindow::copyCurrentMetadata);
    connect(stripMetaBtn_, &QPushButton::toggled, this, [this](bool checked) {
        stripMetadataOnExport_ = checked;
        const bool dark = AppSettings::instance().effectiveDarkTheme();
        stripMetaBtn_->setIcon(makeUiIcon(
            UiIconKind::Trash,
            checked ? Qt::white : (dark ? QColor(0xFD, 0xA4, 0xAF) : QColor(0xBE, 0x12, 0x3C)),
            15));
        stripMetaBtn_->setText(checked
                                   ? trUi(QStringLiteral("Без метаданных ✓"), QStringLiteral("Metadata Stripped ✓"))
                                   : trUi(QStringLiteral("Удалить метаданные"), QStringLiteral("Strip Metadata")));
    });

    connect(editorCaptionEdit_, &QLineEdit::editingFinished, this, [this]() {
        editingRecord_.caption = editorCaptionEdit_->text();
        db_->setCaption(editingRecord_.filePath, editingRecord_.caption);
        this->setFocus();
    });

    // Interactive Crop & Zero-Lag Free-Text Drag callbacks
    connect(editorCanvas_, &EditorCanvasWidget::cropRectAdjusted, this, [this](double l, double t, double r, double b) {
        pushUndoState();
        editingRecord_.edits.cropLeft = l;
        editingRecord_.edits.cropTop = t;
        editingRecord_.edits.cropRight = r;
        editingRecord_.edits.cropBottom = b;
        editingRecord_.edits.cropEnabled = true;
        const double w = std::max(0.05, r - l);
        editingRecord_.edits.cropZoom = std::clamp(qRound((1.0 / w - 1.0) * 100.0), 0, 300);
        syncEditorSliders();
        updateCropReadout();
        populateEffectStackList();
        db_->setEdits(editingRecord_.filePath, editingRecord_.edits);
    });
    connect(editorCanvas_, &EditorCanvasWidget::textPositionDragStarted, this, &MainWindow::pushUndoState);
    connect(editorCanvas_, &EditorCanvasWidget::textPositionCommitted, this, [this](double nx, double ny) {
        const int idx = selectedEffectLayerIndex();
        if (idx >= 0 && editingRecord_.edits.effectLayers[idx].preset == EffectPreset::TextOverlay) {
            editingRecord_.edits.effectLayers[idx].posX = nx;
            editingRecord_.edits.effectLayers[idx].posY = ny;
            db_->setEdits(editingRecord_.filePath, editingRecord_.edits);
        }
    });

    auto applyCropAction = [this]() {
        setCropEditingEnabled(false, true);
        this->setFocus();
    };
    auto resetCropAction = [this]() {
        pushUndoState();
        editingRecord_.edits.cropLeft = 0.0;
        editingRecord_.edits.cropTop = 0.0;
        editingRecord_.edits.cropRight = 1.0;
        editingRecord_.edits.cropBottom = 1.0;
        editingRecord_.edits.cropZoom = 0;
        editingRecord_.edits.cropEnabled = true;
        editorCanvas_->setCropMode(true, 0.0, 0.0, 1.0, 1.0);
        syncEditorSliders();
        updateCropReadout();
        populateEffectStackList();
        saveEditorEdits();
    };
    connect(applyCropBtn_, &QPushButton::clicked, this, applyCropAction);
    connect(canvasApplyCropBtn_, &QPushButton::clicked, this, applyCropAction);
    connect(resetCropBtn_, &QPushButton::clicked, this, resetCropAction);
    connect(canvasResetCropBtn_, &QPushButton::clicked, this, resetCropAction);
    connect(canvasCloseCropBtn_, &QPushButton::clicked, this, applyCropAction);

    // Neutral White Balance Eyedropper & AutoBalance layer
    connect(eyedropperBtn_, &QPushButton::toggled, this, [this](bool on) {
        eyedropperActive_ = on;
        if (on) setCropEditingEnabled(false, false);
        editorCanvas_->setEyedropperMode(on);
    });
    connect(editorCanvas_, &EditorCanvasWidget::colorPicked, this, [this](const QColor& c) {
        pushUndoState();
        const double r = c.red();
        const double g = c.green();
        const double b = c.blue();
        editingRecord_.edits.temperature = std::clamp(editingRecord_.edits.temperature + qRound((b - r) * 0.45), -100, 100);
        editingRecord_.edits.tint = std::clamp(editingRecord_.edits.tint + qRound((g - (r + b) * 0.5) * 0.45), -100, 100);
        editingRecord_.edits.lightColorEnabled = true;
        eyedropperBtn_->setChecked(false);
        syncEditorSliders();
        populateEffectStackList();
        saveEditorEdits();
    });
    connect(autoColorBtn_, &QPushButton::clicked, this, [this]() {
        addOrSelectEffectLayer(EffectPreset::AutoBalance, 85);
    });

    // Per-tab reset buttons
    connect(resetBasicBtn_, &QPushButton::clicked, this, [this]() {
        pushUndoState();
        editingRecord_.edits.straightenDeg = 0.0;
        editingRecord_.edits.basicEnabled = true;
        editorCanvas_->setShowGrid(false);
        syncEditorSliders();
        populateEffectStackList();
        saveEditorEdits();
    });
    connect(resetTuningBtn_, &QPushButton::clicked, this, [this]() {
        pushUndoState();
        editingRecord_.edits.brightness = 0;
        editingRecord_.edits.highlights = 0;
        editingRecord_.edits.shadows = 0;
        editingRecord_.edits.contrast = 0;
        editingRecord_.edits.saturation = 0;
        editingRecord_.edits.temperature = 0;
        editingRecord_.edits.tint = 0;
        editingRecord_.edits.hueShift = 0;
        editingRecord_.edits.lightColorEnabled = true;
        for (int i = editingRecord_.edits.effectLayers.size() - 1; i >= 0; --i) {
            if (editingRecord_.edits.effectLayers[i].preset == EffectPreset::AutoBalance) {
                if (editingRecord_.edits.effectLayers[i].id == selectedLayerId_) {
                    selectedLayerId_.clear();
                }
                editingRecord_.edits.effectLayers.removeAt(i);
            }
        }
        syncEditorSliders();
        populateEffectStackList();
        updateActiveEffectControls();
        saveEditorEdits();
    });

    // Basic Tab Action Tiles (Auto-Fixes & Text now create real stackable layers!)
    connect(basicTilesList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        const int action = basicTilesList_->row(item);
        if (action == 0) { // Кадрирование
            setCropEditingEnabled(!editorCanvas_->isCropMode(), false);
            return;
        }
        if (action == 1) { // Выравнивание -> раскрывает/скрывает ползунки горизонта, приближения и резкости
            const bool nextShow = basicAdjustWidget_ ? !basicAdjustWidget_->isVisible() : false;
            if (basicAdjustWidget_) {
                basicAdjustWidget_->setVisible(nextShow);
            }
            const bool hasAngle = !qFuzzyIsNull(editingRecord_.edits.straightenDeg);
            editorCanvas_->setShowGrid(nextShow || hasAngle);
            if (nextShow && !hasAngle) {
                QTimer::singleShot(2200, editorCanvas_, [this]() {
                    if (editorCanvas_ && qFuzzyIsNull(editingRecord_.edits.straightenDeg)) {
                        editorCanvas_->setShowGrid(false);
                    }
                });
            }
            updateLeftTabsHeight();
            return;
        }
        if (action == 6) { // Текст -> stackable, free-draggable TextOverlay layer (always adds new layer!)
            addNewTextLayer();
            return;
        }

        switch (action) {
            case 2: // Мне повезёт!
                addOrSelectEffectLayer(EffectPreset::AutoLucky, 85);
                return;
            case 3: // Автоконтраст
                addOrSelectEffectLayer(EffectPreset::AutoContrast, 85);
                return;
            case 4: // Автоцвет
                addOrSelectEffectLayer(EffectPreset::AutoColor, 85);
                return;
            case 5: // Поворот 90° (не добавляем в стек слоёв эффектов)
                pushUndoState();
                editingRecord_.edits.rotationDeg = (editingRecord_.edits.rotationDeg + 90) % 360;
                break;
            case 7: // Оригинал
                pushUndoState();
                editingRecord_.edits = PhotoEdits{};
                selectedLayerId_.clear();
                if (basicAdjustWidget_) basicAdjustWidget_->setVisible(false);
                setCropEditingEnabled(false, true);
                editorCanvas_->setLiveTextLayer(nullptr);
                populateEffectStackList();
                updateActiveEffectControls();
                break;
            default:
                return;
        }
        syncEditorSliders();
        saveEditorEdits();
    });

    // Bind Basic & Light/Color sliders (coalesced ultra-fast drag preview + crisp release commit)
    auto bindRow = [this](MagneticSliderRow* row, auto setter, bool isLightColor) {
        connect(row, &MagneticSliderRow::editStarted, this, &MainWindow::pushUndoState);
        connect(row, &MagneticSliderRow::liveValueChanged, this, [this, setter, isLightColor](int v) {
            setter(v);
            if (isLightColor) {
                editingRecord_.edits.lightColorEnabled = true;
            } else {
                editingRecord_.edits.basicEnabled = true;
            }
            syncEditorSliders();
            scheduleLivePreview();
        });
        connect(row, &MagneticSliderRow::valueCommitted, this, [this, setter, isLightColor](int v) {
            if (previewCoalesceTimer_) previewCoalesceTimer_->stop();
            dragPreviewActive_ = false;
            setter(v);
            if (isLightColor) {
                editingRecord_.edits.lightColorEnabled = true;
            } else {
                editingRecord_.edits.basicEnabled = true;
            }
            syncEditorSliders();
            populateEffectStackList();
            refreshEditorPreview(false);
            if (dbSaveTimer_) dbSaveTimer_->start();
        });
    };

    bindRow(straightenRow_, [this](int v) {
        editingRecord_.edits.straightenDeg = v;
        editorCanvas_->setShowGrid(v != 0);
    }, false);

    bindRow(brightnessRow_, [this](int v) { editingRecord_.edits.brightness = v; }, true);
    bindRow(highlightsRow_, [this](int v) { editingRecord_.edits.highlights = v; }, true);
    bindRow(shadowsRow_, [this](int v) { editingRecord_.edits.shadows = v; }, true);
    bindRow(contrastRow_, [this](int v) { editingRecord_.edits.contrast = v; }, true);
    bindRow(saturationRow_, [this](int v) { editingRecord_.edits.saturation = v; }, true);
    bindRow(tempRow_, [this](int v) { editingRecord_.edits.temperature = v; }, true);
    bindRow(tintRow_, [this](int v) { editingRecord_.edits.tint = v; }, true);
    bindRow(hueShiftRow_, [this](int v) { editingRecord_.edits.hueShift = v; }, true);

    // Bind 3 Picasa Effect Grids (Обработка / Арт / Рамки)
    auto bindEffectGrid = [this](QListWidget* grid, int categoryIdx) {
        connect(grid, &QListWidget::itemClicked, this, [this, grid, categoryIdx](QListWidgetItem* item) {
            const int row = grid->row(item);
            const QList<EffectPreset> presets = ImageLoader::effectsForCategory(categoryIdx);
            if (row >= 0 && row < presets.size()) {
                addOrSelectEffectLayer(presets[row]);
            }
        });
    };
    bindEffectGrid(processEffectsList_, 0);
    bindEffectGrid(artEffectsList_, 1);
    bindEffectGrid(frameEffectsList_, 2);

    // Bind Contextual Active Effect & Text Controls (Zero-lag live preview when editing text & coalesced sliders!)
    connect(addAnotherTextBtn_, &QPushButton::clicked, this, &MainWindow::addNewTextLayer);
    connect(textLayerEdit_, &QLineEdit::textEdited, this, [this](const QString& txt) {
        const int idx = selectedEffectLayerIndex();
        if (idx >= 0 && editingRecord_.edits.effectLayers[idx].preset == EffectPreset::TextOverlay) {
            editingRecord_.edits.effectLayers[idx].text = txt;
            updateActiveLayerLabelInStack();
            if (editingRecord_.edits.effectLayers[idx].enabled) {
                editorCanvas_->setLiveTextLayer(&editingRecord_.edits.effectLayers[idx]);
                if (dbSaveTimer_) dbSaveTimer_->start();
            } else {
                saveEditorEdits();
            }
        }
    });
    connect(textLayerSizeRow_, &MagneticSliderRow::editStarted, this, &MainWindow::pushUndoState);
    connect(textLayerSizeRow_, &MagneticSliderRow::liveValueChanged, this, [this](int v) {
        const int idx = selectedEffectLayerIndex();
        if (idx >= 0 && editingRecord_.edits.effectLayers[idx].preset == EffectPreset::TextOverlay) {
            editingRecord_.edits.effectLayers[idx].textSize = v;
            if (editingRecord_.edits.effectLayers[idx].enabled) {
                editorCanvas_->setLiveTextLayer(&editingRecord_.edits.effectLayers[idx]);
            }
        }
    });
    connect(textLayerSizeRow_, &MagneticSliderRow::valueCommitted, this, [this](int v) {
        const int idx = selectedEffectLayerIndex();
        if (idx >= 0 && editingRecord_.edits.effectLayers[idx].preset == EffectPreset::TextOverlay) {
            editingRecord_.edits.effectLayers[idx].textSize = v;
            if (editingRecord_.edits.effectLayers[idx].enabled) {
                editorCanvas_->setLiveTextLayer(&editingRecord_.edits.effectLayers[idx]);
                if (dbSaveTimer_) dbSaveTimer_->start();
            } else {
                saveEditorEdits();
            }
        }
    });
    connect(textColorBtn_, &QPushButton::clicked, this, [this]() {
        const int idx = selectedEffectLayerIndex();
        if (idx < 0 || editingRecord_.edits.effectLayers[idx].preset != EffectPreset::TextOverlay) return;
        const QColor init(editingRecord_.edits.effectLayers[idx].textColor);
        const QColor picked = QColorDialog::getColor(init.isValid() ? init : Qt::white, this,
                                                   trUi(QStringLiteral("Цвет текста"), QStringLiteral("Text Color")));
        if (picked.isValid()) {
            pushUndoState();
            editingRecord_.edits.effectLayers[idx].textColor = picked.name();
            textColorBtn_->setProperty("swatchColor", picked.name());
            textColorBtn_->setStyleSheet(QString());
            textColorBtn_->update();
            if (editingRecord_.edits.effectLayers[idx].enabled) {
                editorCanvas_->setLiveTextLayer(&editingRecord_.edits.effectLayers[idx]);
                if (dbSaveTimer_) dbSaveTimer_->start();
            } else {
                saveEditorEdits();
            }
        }
    });

    connect(effectIntensityRow_, &MagneticSliderRow::editStarted, this, &MainWindow::pushUndoState);
    connect(effectIntensityRow_, &MagneticSliderRow::liveValueChanged, this, [this](int v) {
        const int idx = selectedEffectLayerIndex();
        if (idx >= 0) {
            editingRecord_.edits.effectLayers[idx].intensity = v;
            updateActiveLayerLabelInStack();
            if (editingRecord_.edits.effectLayers[idx].preset == EffectPreset::TextOverlay &&
                editingRecord_.edits.effectLayers[idx].enabled) {
                editorCanvas_->setLiveTextLayer(&editingRecord_.edits.effectLayers[idx]);
            } else {
                scheduleLivePreview();
            }
        }
    });
    connect(effectIntensityRow_, &MagneticSliderRow::valueCommitted, this, [this](int v) {
        if (previewCoalesceTimer_) previewCoalesceTimer_->stop();
        dragPreviewActive_ = false;
        const int idx = selectedEffectLayerIndex();
        if (idx >= 0) {
            editingRecord_.edits.effectLayers[idx].intensity = v;
            updateActiveLayerLabelInStack();
            if (editingRecord_.edits.effectLayers[idx].preset == EffectPreset::TextOverlay &&
                editingRecord_.edits.effectLayers[idx].enabled) {
                editorCanvas_->setLiveTextLayer(&editingRecord_.edits.effectLayers[idx]);
            } else {
                refreshEditorPreview(false);
            }
            if (dbSaveTimer_) dbSaveTimer_->start();
        }
    });
    connect(effectParam2Row_, &MagneticSliderRow::editStarted, this, &MainWindow::pushUndoState);
    connect(effectParam2Row_, &MagneticSliderRow::liveValueChanged, this, [this](int v) {
        const int idx = selectedEffectLayerIndex();
        if (idx >= 0) {
            editingRecord_.edits.effectLayers[idx].param2 = v;
            scheduleLivePreview();
        }
    });
    connect(effectParam2Row_, &MagneticSliderRow::valueCommitted, this, [this](int v) {
        if (previewCoalesceTimer_) previewCoalesceTimer_->stop();
        dragPreviewActive_ = false;
        const int idx = selectedEffectLayerIndex();
        if (idx >= 0) {
            editingRecord_.edits.effectLayers[idx].param2 = v;
            refreshEditorPreview(false);
            if (dbSaveTimer_) dbSaveTimer_->start();
        }
    });

    auto removeSelectedLayerOrGroup = [this]() {
        const int idx = selectedEffectLayerIndex();
        if (idx >= 0) {
            pushUndoState();
            editingRecord_.edits.effectLayers.removeAt(idx);
            selectedLayerId_.clear();
            if (!editingRecord_.edits.effectLayers.isEmpty()) {
                selectedLayerId_ = editingRecord_.edits.effectLayers.last().id;
            }
            populateEffectStackList();
            updateActiveEffectControls();
            saveEditorEdits();
            return;
        }
        if (selectedLayerId_ == QStringLiteral("__lightcolor__")) {
            pushUndoState();
            editingRecord_.edits.brightness = 0;
            editingRecord_.edits.highlights = 0;
            editingRecord_.edits.shadows = 0;
            editingRecord_.edits.contrast = 0;
            editingRecord_.edits.saturation = 0;
            editingRecord_.edits.temperature = 0;
            editingRecord_.edits.tint = 0;
            editingRecord_.edits.hueShift = 0;
            editingRecord_.edits.lightColorEnabled = true;
            selectedLayerId_.clear();
            syncEditorSliders();
            populateEffectStackList();
            updateActiveEffectControls();
            saveEditorEdits();
            return;
        }
        if (selectedLayerId_ == QStringLiteral("__crop__")) {
            pushUndoState();
            editingRecord_.edits.cropLeft = 0.0;
            editingRecord_.edits.cropTop = 0.0;
            editingRecord_.edits.cropRight = 1.0;
            editingRecord_.edits.cropBottom = 1.0;
            editingRecord_.edits.cropZoom = 0;
            editingRecord_.edits.cropEnabled = true;
            selectedLayerId_.clear();
            setCropEditingEnabled(false, true);
            syncEditorSliders();
            populateEffectStackList();
            updateActiveEffectControls();
            saveEditorEdits();
            return;
        }
        if (selectedLayerId_ == QStringLiteral("__basic__")) {
            pushUndoState();
            editingRecord_.edits.straightenDeg = 0.0;
            editingRecord_.edits.cropZoom = 0;
            editingRecord_.edits.sharpen = 0;
            editingRecord_.edits.basicEnabled = true;
            selectedLayerId_.clear();
            syncEditorSliders();
            populateEffectStackList();
            updateActiveEffectControls();
            saveEditorEdits();
        }
    };
    connect(removeSelectedEffBtn_, &QPushButton::clicked, this, removeSelectedLayerOrGroup);
    connect(layerDeleteBtn_, &QPushButton::clicked, this, removeSelectedLayerOrGroup);

    // Right Pane Photoshop-style Layer Stack interaction
    connect(effectStackList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        const QString id = item->data(Qt::UserRole).toString();
        if (id.isEmpty()) {
            selectedLayerId_.clear();
            edTabs_->setCurrentIndex(0);
            updateActiveEffectControls();
            populateEffectStackList();
            if (editorCanvas_->isCropMode()) {
                setCropEditingEnabled(false, false);
            } else {
                refreshEditorPreview(false);
            }
            return;
        }
        if (id == QStringLiteral("__lightcolor__")) {
            selectedLayerId_ = id;
            edTabs_->setCurrentIndex(1);
            updateActiveEffectControls();
            populateEffectStackList();
            if (editorCanvas_->isCropMode()) {
                setCropEditingEnabled(false, false);
            } else {
                refreshEditorPreview(false);
            }
            return;
        }
        if (id == QStringLiteral("__crop__")) {
            selectedLayerId_ = id;
            edTabs_->setCurrentIndex(0);
            updateActiveEffectControls();
            populateEffectStackList();
            setCropEditingEnabled(true, false);
            return;
        }
        if (id == QStringLiteral("__basic__")) {
            selectedLayerId_ = id;
            edTabs_->setCurrentIndex(0);
            if (basicAdjustWidget_) basicAdjustWidget_->setVisible(true);
            updateActiveEffectControls();
            populateEffectStackList();
            if (editorCanvas_->isCropMode()) {
                setCropEditingEnabled(false, false);
            } else {
                refreshEditorPreview(false);
            }
            return;
        }
        selectEffectLayerById(id);
    });
    connect(effectStackList_, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        const QString id = item->data(Qt::UserRole).toString();
        if (id.isEmpty()) return;
        const bool checked = (item->checkState() == Qt::Checked);
        if (id == QStringLiteral("__lightcolor__")) {
            if (editingRecord_.edits.lightColorEnabled != checked) {
                pushUndoState();
                editingRecord_.edits.lightColorEnabled = checked;
                saveEditorEdits();
            }
            return;
        }
        if (id == QStringLiteral("__crop__")) {
            if (editingRecord_.edits.cropEnabled != checked) {
                pushUndoState();
                editingRecord_.edits.cropEnabled = checked;
                saveEditorEdits();
            }
            return;
        }
        if (id == QStringLiteral("__basic__")) {
            if (editingRecord_.edits.basicEnabled != checked) {
                pushUndoState();
                editingRecord_.edits.basicEnabled = checked;
                saveEditorEdits();
            }
            return;
        }
        for (EffectLayer& layer : editingRecord_.edits.effectLayers) {
            if (layer.id == id) {
                if (layer.enabled != checked) {
                    pushUndoState();
                    layer.enabled = checked;
                    updateActiveEffectControls();
                    saveEditorEdits();
                }
                break;
            }
        }
    });
    connect(layerUpBtn_, &QPushButton::clicked, this, [this]() {
        const int idx = selectedEffectLayerIndex();
        if (idx >= 0 && idx + 1 < editingRecord_.edits.effectLayers.size()) {
            pushUndoState();
            editingRecord_.edits.effectLayers.swapItemsAt(idx, idx + 1);
            populateEffectStackList();
            saveEditorEdits();
        }
    });
    connect(layerDownBtn_, &QPushButton::clicked, this, [this]() {
        const int idx = selectedEffectLayerIndex();
        if (idx > 0) {
            pushUndoState();
            editingRecord_.edits.effectLayers.swapItemsAt(idx, idx - 1);
            populateEffectStackList();
            saveEditorEdits();
        }
    });

    // Editor Right-Pane PSD Layer inspection & export
    connect(editorPsdLayers_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row <= 0) {
            editingPreviewLayerImage_ = QImage();
            editorExportPsdLayerBtn_->setEnabled(false);
            refreshEditorPreview(false);
            return;
        }
        const int idx = row - 1;
        if (idx >= 0 && idx < editingPsdLayers_.size()) {
            editingPreviewLayerImage_ = editingPsdLayers_[idx].image;
            editorExportPsdLayerBtn_->setEnabled(!editingPreviewLayerImage_.isNull());
            refreshEditorPreview(true);
        }
    });
    connect(editorExportPsdLayerBtn_, &QPushButton::clicked, this, &MainWindow::exportSelectedEditorPsdLayer);

    btnFilter_ = installSmoothButtons(this, AppSettings::instance().effectiveDarkTheme());
    applyTheme();
    retranslateUi();
}

void MainWindow::openSettingsDialog() {
    SettingsDialog dlg(this);
    dlg.exec();
}

void MainWindow::updateThemeIcons() {
    const bool dark = AppSettings::instance().effectiveDarkTheme();
    const QColor iconCol = dark ? QColor(0xE2, 0xE8, 0xF0) : QColor(0x1E, 0x29, 0x3B);

    if (libSettingsBtn_) libSettingsBtn_->setIcon(makeUiIcon(UiIconKind::Settings, iconCol, 16));
    if (edSettingsBtn_) edSettingsBtn_->setIcon(makeUiIcon(UiIconKind::Settings, iconCol, 16));
    if (backBtn_) backBtn_->setIcon(makeUiIcon(UiIconKind::BackArrow, iconCol, 16));
    if (edPrevBtn_) edPrevBtn_->setIcon(makeUiIcon(UiIconKind::ChevronLeft, iconCol, 16));
    if (edNextBtn_) edNextBtn_->setIcon(makeUiIcon(UiIconKind::ChevronRight, iconCol, 16));
    if (undoBtn_) undoBtn_->setIcon(makeUiIcon(UiIconKind::Undo, iconCol, 16));
    if (rotLeftBtn_) rotLeftBtn_->setIcon(makeUiIcon(UiIconKind::RotateLeft, iconCol, 16));
    if (rotRightBtn_) rotRightBtn_->setIcon(makeUiIcon(UiIconKind::RotateRight, iconCol, 16));
    if (copyMetaBtn_) copyMetaBtn_->setIcon(makeUiIcon(UiIconKind::Copy, iconCol, 15));
    if (stripMetaBtn_) {
        stripMetaBtn_->setIcon(makeUiIcon(UiIconKind::Trash,
                                          stripMetadataOnExport_ ? Qt::white : (dark ? QColor(0xFD, 0xA4, 0xAF) : QColor(0xBE, 0x12, 0x3C)),
                                          15));
    }
}

void MainWindow::applyTheme() {
    const bool dark = AppSettings::instance().effectiveDarkTheme();
    if (qApp) {
        AppSettings::applyApplicationPalette(*qApp, dark);
    }
    if (btnFilter_) btnFilter_->setDarkTheme(dark);
    if (editorFilmstripDelegate_) editorFilmstripDelegate_->setDarkTheme(dark);

    if (dark) {
        setStyleSheet(QStringLiteral(
            "QMainWindow { background-color: #111318; color: #F1F5F9; }"
            "QWidget { color: #E2E8F0; }"
            "QWidget#topBar, QWidget#edTop, QWidget#trayBar, QWidget#edBottomBar { background-color: #181A22; border-bottom: 1px solid #2A2E3B; }"
            "QWidget#trayBar, QWidget#edBottomBar { border-bottom: none; border-top: 1px solid #2A2E3B; }"
            "QWidget#edLeftPane { background-color: #181A22; border-right: 1px solid #2A2E3B; }"
            "QWidget#edRightPane { background-color: #181A22; border-left: 1px solid #2A2E3B; }"
            "QWidget#tabInner, QWidget#rightInner { background-color: #181A22; }"
            "QWidget#canvasPane { background-color: #13151B; }"
            "QWidget#emptyCard { background-color: #181A22; border: 1px dashed #383D4E; border-radius: 16px; }"
            "QWidget#activeEffectBox { background-color: #1D202A; border-top: 1px solid #2E3342; }"
            "QWidget#cropBarWidget { background-color: #281B3F; border: 1px solid #7E22CE; border-radius: 10px; }"
            "QScrollArea#tabScroll, QScrollArea#rightInspectorScroll { background: transparent; border: none; }"
            "QSplitter::handle { background-color: #2A2E3B; }"
            "QSplitter::handle:hover { background-color: #A855F7; }"
            "QListWidget#sidebarList { background-color: #181A22; color: #E2E8F0; border: none; border-right: 1px solid #2A2E3B; padding: 8px 5px; font-size: 13px; outline: 0; }"
            "QListWidget#sidebarList::item { padding: 6px 6px; border-radius: 8px; color: #E2E8F0; border: none; margin: 1px 0px; }"
            "QListWidget#sidebarList::item:hover { background-color: #222633; }"
            "QListWidget#sidebarList::item:selected { background-color: #2E1B4E; color: #E9D5FF; font-weight: 600; }"
            "QListWidget#photoGrid { background-color: #111318; color: #F1F5F9; border: none; padding: 0px; outline: 0; }"
            "QListWidget#edVisualGrid { background-color: #181A22; color: #E2E8F0; border: none; outline: 0; }"
            "QListWidget#edVisualGrid::item { color: #CBD5E1; border-radius: 8px; padding: 3px 1px; font-size: 11px; border: 1px solid transparent; }"
            "QListWidget#edVisualGrid::item:hover { background-color: #232634; border-color: #4C1D95; }"
            "QListWidget#edVisualGrid::item:selected { background-color: #2E1B4E; color: #E9D5FF; border: 1px solid #A855F7; font-weight: 600; }"
            "QListWidget#edFilmstrip { background-color: #111318; border: 1px solid #2A2E3B; border-radius: 8px; outline: 0; padding: 0px; }"
            "QListWidget#effectStackList, QListWidget#edPsdLayers { background-color: #1E212B; color: #E2E8F0; border: 1px solid #2E3342; border-radius: 8px; outline: 0; padding: 4px; }"
            "QListWidget#effectStackList::item, QListWidget#edPsdLayers::item { padding: 6px 6px; border-radius: 6px; font-size: 12px; border: 1px solid transparent; }"
            "QListWidget#effectStackList::item:hover, QListWidget#edPsdLayers::item:hover { background-color: #282C3A; }"
            "QListWidget#effectStackList::item:selected, QListWidget#edPsdLayers::item:selected { background-color: #2E1B4E; color: #E9D5FF; border-color: #A855F7; font-weight: 600; }"
            "QPushButton { background-color: #232634; color: #F1F5F9; border: none; border-radius: 8px; padding: 6px 12px; font-size: 12px; font-weight: 600; }"
            "QPushButton#primaryBtn { background-color: #A855F7; color: #FFFFFF; }"
            "QPushButton#iconSquareBtn { padding: 4px; }"
            "QPushButton#miniResetBtn { padding: 0px; background: transparent; border: none; }"
            "QLineEdit { background-color: #232634; color: #F8FAFC; border: 1.5px solid #303546; border-radius: 8px; padding: 5px 10px; font-size: 12px; }"
            "QLineEdit:focus { background-color: #1B1E28; border: 1.5px solid #A855F7; }"
            "QPlainTextEdit#metaBox { background-color: #1E212B; color: #CBD5E1; border: 1px solid #2E3342; border-radius: 8px; font-size: 11px; padding: 5px; }"
            "QLabel { color: #E2E8F0; background: transparent; }"
            "QComboBox { background-color: #232634; color: #F8FAFC; border: 1px solid #303546; border-radius: 8px; padding: 5px 10px; font-size: 12px; }"
            "QComboBox:hover { border-color: #A855F7; }"
            "QComboBox QAbstractItemView { background-color: #1E212B; color: #F8FAFC; selection-background-color: #A855F7; selection-color: #FFFFFF; border: 1px solid #303546; }"
            "QTabWidget#edTabs::pane { border: none; border-top: 1px solid #2A2E3B; background-color: #181A22; }"
            "QTabWidget#edTabs::tab-bar { alignment: left; }"
            "QTabBar { background-color: #14161D; border: none; }"
            "QTabBar::tab { background-color: #14161D; color: #94A3B8; padding: 9px 7px; margin: 0px; border: none; border-bottom: 2px solid transparent; font-size: 11px; font-weight: 600; min-height: 20px; }"
            "QTabBar::tab:hover { color: #F8FAFC; background-color: #1E212B; }"
            "QTabBar::tab:selected { background-color: #181A22; color: #C084FC; border-bottom: 2px solid #A855F7; }"
            "QTabBar::scroller { width: 18px; }"
            "QSlider::groove:horizontal { height: 4px; background: #333848; border-radius: 2px; }"
            "QSlider::handle:horizontal { background: #A855F7; width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; border: 2px solid #F8FAFC; }"
        ) + modernScrollBarQss(true));

        brandLabel_->setStyleSheet(QStringLiteral("font-weight: 800; font-size: 16px; color: #F8FAFC;"));
        zoomLabel_->setStyleSheet(QStringLiteral("font-size: 12px; color: #94A3B8;"));
        statusCountLabel_->setStyleSheet(QStringLiteral("font-size: 12px; color: #94A3B8; font-weight: 500;"));
        emptyTitle_->setStyleSheet(QStringLiteral("font-size: 17px; font-weight: 700; color: #F8FAFC;"));
        emptyDesc_->setStyleSheet(QStringLiteral("font-size: 13px; color: #94A3B8;"));
        trayInfoLabel_->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 13px; color: #F8FAFC;"));
        albumLbl_->setStyleSheet(QStringLiteral("font-size: 12px; color: #94A3B8;"));
        cropHint_->setStyleSheet(QStringLiteral("font-size: 11px; color: #E9D5FF; font-weight: 600;"));
        activeEffectTitle_->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 12px; color: #C084FC;"));
        textDragHint_->setStyleSheet(QStringLiteral("font-size: 11px; color: #C084FC; font-weight: 600;"));
        editorMetaStatus_->setStyleSheet(QStringLiteral("font-size: 12px; color: #94A3B8;"));
        stackHeader_->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 12px; color: #F8FAFC;"));
        stackHint_->setStyleSheet(QStringLiteral("font-size: 11px; color: #94A3B8;"));
        histTitle_->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 12px; color: #F8FAFC;"));
        editorExifLabel_->setStyleSheet(QStringLiteral("font-size: 11px; color: #CBD5E1;"));
        metaHintLabel_->setStyleSheet(QStringLiteral("font-size: 11px; color: #94A3B8;"));
        editorLayersTitle_->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 12px; color: #F8FAFC;"));
    } else {
        setStyleSheet(QStringLiteral(
            "QMainWindow { background-color: #F1F5F9; color: #0F172A; }"
            "QWidget { color: #0F172A; }"
            "QWidget#topBar, QWidget#edTop, QWidget#trayBar, QWidget#edBottomBar { background-color: #FFFFFF; border-bottom: 1px solid #E2E8F0; }"
            "QWidget#trayBar, QWidget#edBottomBar { border-bottom: none; border-top: 1px solid #E2E8F0; }"
            "QWidget#edLeftPane { background-color: #FFFFFF; border-right: 1px solid #E2E8F0; }"
            "QWidget#edRightPane { background-color: #FFFFFF; border-left: 1px solid #E2E8F0; }"
            "QWidget#tabInner, QWidget#rightInner { background-color: #FFFFFF; }"
            "QWidget#canvasPane { background-color: #2B2E35; }"
            "QWidget#emptyCard { background-color: #FFFFFF; border: 1px dashed #CBD5E1; border-radius: 16px; }"
            "QWidget#activeEffectBox { background-color: #F8FAFC; border-top: 1px solid #E2E8F0; }"
            "QWidget#cropBarWidget { background-color: #FAF5FF; border: 1px solid #D8B4FE; border-radius: 10px; }"
            "QScrollArea#tabScroll, QScrollArea#rightInspectorScroll { background: transparent; border: none; }"
            "QSplitter::handle { background-color: #E2E8F0; }"
            "QSplitter::handle:hover { background-color: #A855F7; }"
            "QListWidget#sidebarList { background-color: #FFFFFF; color: #1E293B; border: none; border-right: 1px solid #E2E8F0; padding: 8px 5px; font-size: 13px; outline: 0; }"
            "QListWidget#sidebarList::item { padding: 6px 6px; border-radius: 8px; color: #1E293B; border: none; margin: 1px 0px; }"
            "QListWidget#sidebarList::item:hover { background-color: #FAF5FF; }"
            "QListWidget#sidebarList::item:selected { background-color: #F3E8FF; color: #7E22CE; font-weight: 600; }"
            "QListWidget#photoGrid { background-color: #F1F5F9; color: #0F172A; border: none; padding: 0px; outline: 0; }"
            "QListWidget#edVisualGrid { background-color: #FFFFFF; color: #1E293B; border: none; outline: 0; }"
            "QListWidget#edVisualGrid::item { color: #334155; border-radius: 8px; padding: 3px 1px; font-size: 11px; border: 1px solid transparent; }"
            "QListWidget#edVisualGrid::item:hover { background-color: #FAF5FF; border-color: #D8B4FE; }"
            "QListWidget#edVisualGrid::item:selected { background-color: #F3E8FF; color: #7E22CE; border: 1px solid #A855F7; font-weight: 600; }"
            "QListWidget#edFilmstrip { background-color: #F1F5F9; border: 1px solid #E2E8F0; border-radius: 8px; outline: 0; padding: 0px; }"
            "QListWidget#effectStackList, QListWidget#edPsdLayers { background-color: #F8FAFC; border: 1px solid #E2E8F0; border-radius: 8px; outline: 0; padding: 4px; }"
            "QListWidget#effectStackList::item, QListWidget#edPsdLayers::item { padding: 6px 6px; border-radius: 6px; font-size: 12px; border: 1px solid transparent; }"
            "QListWidget#effectStackList::item:hover, QListWidget#edPsdLayers::item:hover { background-color: #FAF5FF; }"
            "QListWidget#effectStackList::item:selected, QListWidget#edPsdLayers::item:selected { background-color: #F3E8FF; color: #7E22CE; border-color: #C084FC; font-weight: 600; }"
            "QPushButton { background-color: #FFFFFF; color: #1E293B; border: none; border-radius: 8px; padding: 6px 12px; font-size: 12px; font-weight: 600; }"
            "QPushButton#primaryBtn { background-color: #A855F7; color: #FFFFFF; }"
            "QPushButton#backBtn { background-color: #F8FAFC; color: #0F172A; padding: 6px 14px; font-weight: 600; }"
            "QPushButton#starBtn { background-color: #FFFFFF; color: #334155; padding: 6px 12px; font-weight: 600; }"
            "QPushButton#starBtnActive { background-color: #FEF3C7; color: #92400E; font-weight: 600; }"
            "QPushButton#dangerBtn { background-color: #FFF1F2; color: #BE123C; padding: 6px 10px; font-weight: 600; }"
            "QPushButton#iconSquareBtn { padding: 4px; }"
            "QPushButton#miniResetBtn { padding: 0px; background: transparent; border: none; }"
            "QLineEdit { background-color: #F1F5F9; color: #0F172A; border: 1.5px solid transparent; border-radius: 8px; padding: 5px 10px; font-size: 12px; }"
            "QLineEdit:focus { background-color: #FFFFFF; border: 1.5px solid #A855F7; }"
            "QPlainTextEdit#metaBox { background-color: #F8FAFC; color: #334155; border: 1px solid #E2E8F0; border-radius: 8px; font-size: 11px; padding: 5px; }"
            "QLabel { color: #1E293B; background: transparent; }"
            "QComboBox { background-color: #F1F5F9; color: #0F172A; border: 1.5px solid transparent; border-radius: 8px; padding: 5px 10px; font-size: 12px; }"
            "QComboBox:hover { background-color: #F3E8FF; border-color: #D8B4FE; }"
            "QComboBox QAbstractItemView { background-color: #FFFFFF; color: #0F172A; selection-background-color: #A855F7; selection-color: #FFFFFF; border: 1px solid #E2E8F0; }"
            "QTabWidget#edTabs::pane { border: none; border-top: 1px solid #E2E8F0; background-color: #FFFFFF; }"
            "QTabWidget#edTabs::tab-bar { alignment: left; }"
            "QTabBar { background-color: #F8FAFC; border: none; }"
            "QTabBar::tab { background-color: #F8FAFC; color: #64748B; padding: 9px 7px; margin: 0px; border: none; border-bottom: 2px solid transparent; font-size: 11px; font-weight: 600; min-height: 20px; }"
            "QTabBar::tab:hover { color: #1E293B; background-color: #FAF5FF; }"
            "QTabBar::tab:selected { background-color: #FFFFFF; color: #7E22CE; border-bottom: 2px solid #A855F7; }"
            "QTabBar::scroller { width: 18px; }"
            "QSlider::groove:horizontal { height: 4px; background: #CBD5E1; border-radius: 2px; }"
            "QSlider::handle:horizontal { background: #A855F7; width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; border: 2px solid #FFFFFF; }"
        ) + modernScrollBarQss(false));

        brandLabel_->setStyleSheet(QStringLiteral("font-weight: 800; font-size: 16px; color: #0F172A;"));
        zoomLabel_->setStyleSheet(QStringLiteral("font-size: 12px; color: #64748B;"));
        statusCountLabel_->setStyleSheet(QStringLiteral("font-size: 12px; color: #64748B; font-weight: 500;"));
        emptyTitle_->setStyleSheet(QStringLiteral("font-size: 17px; font-weight: 700; color: #0F172A;"));
        emptyDesc_->setStyleSheet(QStringLiteral("font-size: 13px; color: #64748B;"));
        trayInfoLabel_->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 13px; color: #0F172A;"));
        albumLbl_->setStyleSheet(QStringLiteral("font-size: 12px; color: #64748B;"));
        cropHint_->setStyleSheet(QStringLiteral("font-size: 11px; color: #7E22CE; font-weight: 600;"));
        activeEffectTitle_->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 12px; color: #7E22CE;"));
        textDragHint_->setStyleSheet(QStringLiteral("font-size: 11px; color: #7E22CE; font-weight: 600;"));
        editorMetaStatus_->setStyleSheet(QStringLiteral("font-size: 12px; color: #64748B;"));
        stackHeader_->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 12px; color: #0F172A;"));
        stackHint_->setStyleSheet(QStringLiteral("font-size: 11px; color: #64748B;"));
        histTitle_->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 12px; color: #0F172A;"));
        editorExifLabel_->setStyleSheet(QStringLiteral("font-size: 11px; color: #475569;"));
        metaHintLabel_->setStyleSheet(QStringLiteral("font-size: 11px; color: #64748B;"));
        editorLayersTitle_->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 12px; color: #0F172A;"));
    }

    updateThemeIcons();

    const QList<MagneticSliderRow*> sliderRows = {
        straightenRow_,
        brightnessRow_, highlightsRow_, shadowsRow_, contrastRow_,
        saturationRow_, tempRow_, tintRow_, hueShiftRow_,
        textLayerSizeRow_, effectIntensityRow_, effectParam2Row_
    };
    for (MagneticSliderRow* r : sliderRows) {
        if (r) r->refreshTheme();
    }
    if (histogramWidget_) histogramWidget_->update();
    if (photoGrid_ && photoGrid_->viewport()) photoGrid_->viewport()->update();
    if (editorFilmstrip_ && editorFilmstrip_->viewport()) editorFilmstrip_->viewport()->update();
    for (QPushButton* b : findChildren<QPushButton*>()) {
        if (b) b->update();
    }
}

void MainWindow::retranslateUi() {
    setWindowTitle(trUi(QStringLiteral("Lumina Photo — Фотоархив и редактор"),
                        QStringLiteral("Lumina Photo — Archive & Editor")));

    addFolderBtn_->setText(trUi(QStringLiteral("+ Добавить папку с ПК"), QStringLiteral("+ Add Folder")));
    newAlbumBtn_->setText(trUi(QStringLiteral("+ Новый альбом"), QStringLiteral("+ New Album")));
    assocBtn_->setText(trUi(QStringLiteral("Ассоциации Windows"), QStringLiteral("Windows Associations")));
    libSettingsBtn_->setToolTip(trUi(QStringLiteral("Настройки темы и языка"), QStringLiteral("Theme & Language Settings")));

    searchEdit_->setPlaceholderText(trUi(QStringLiteral("Поиск по имени, подписи, камере..."),
                                         QStringLiteral("Search by name, caption, camera...")));
    starredFilterBtn_->setText(trUi(QStringLiteral("Избранное"), QStringLiteral("Favorites")));
    starredFilterBtn_->setToolTip(trUi(QStringLiteral("Показать только фотографии со звёздочкой"),
                                       QStringLiteral("Show only starred photos")));
    zoomLabel_->setText(trUi(QStringLiteral("Масштаб:"), QStringLiteral("Zoom:")));
    statusCountLabel_->setText(trUi(QString("%1 фото").arg(currentPhotos_.size()),
                                    QString("%1 photos").arg(currentPhotos_.size())));

    emptyTitle_->setText(trUi(QStringLiteral("Фотографии не найдены"), QStringLiteral("No photos found")));
    emptyDesc_->setText(trUi(
        QStringLiteral("Добавьте папку с фотографиями или проверьте фильтр «Избранное» / поиск.\nПоддерживаются JPG, PNG, WebP, SVG, GIF, ICO, AVIF, HEIC, TIFF и PSD со слоями."),
        QStringLiteral("Add a folder with photos or check the Favorites filter / search query.\nSupports JPG, PNG, WebP, SVG, GIF, ICO, AVIF, HEIC, TIFF and layered PSD.")));
    emptyAddBtn_->setText(trUi(QStringLiteral("+ Выбрать папку с ПК"), QStringLiteral("+ Choose Folder")));
    emptyScanPicsBtn_->setText(trUi(QStringLiteral("Сканировать «Изображения»"), QStringLiteral("Scan Pictures")));

    trayInfoLabel_->setText(trUi(QString("Выбрано: %1").arg(photoGrid_->selectedItems().size()),
                                 QString("Selected: %1").arg(photoGrid_->selectedItems().size())));
    trayStarBtn_->setText(trUi(QStringLiteral("В избранное"), QStringLiteral("Favorite")));
    trayUnstarBtn_->setText(trUi(QStringLiteral("Снять отметку"), QStringLiteral("Unfavorite")));
    albumLbl_->setText(trUi(QStringLiteral("Альбом:"), QStringLiteral("Album:")));
    trayAddToAlbumBtn_->setText(trUi(QStringLiteral("Добавить в альбом"), QStringLiteral("Add to Album")));
    trayNewAlbBtn_->setText(trUi(QStringLiteral("+ Альбом"), QStringLiteral("+ Album")));
    openSelectedBtn_->setText(trUi(QStringLiteral("Открыть в редакторе"), QStringLiteral("Open in Editor")));
    clearSelBtn_->setText(trUi(QStringLiteral("Снять выделение"), QStringLiteral("Clear Selection")));

    backBtn_->setText(trUi(QStringLiteral("Назад в библиотеку"), QStringLiteral("Back to Library")));
    editorCompareBtn_->setText(trUi(QStringLiteral("До / После"), QStringLiteral("Before / After")));
    editorCompareBtn_->setToolTip(trUi(QStringLiteral("Переключить просмотр оригинала / результата"),
                                       QStringLiteral("Toggle original / edited preview")));
    editorZoomBtn_->setText(editorCanvas_->isActualSize()
                                ? trUi(QStringLiteral("Вписать"), QStringLiteral("Fit"))
                                : QStringLiteral("1:1"));
    updateBackdropButton();
    edBackdropBtn_->setToolTip(trUi(QStringLiteral("Подложка для прозрачных PNG / SVG / PSD (Выкл / Белый / Чёрный)"),
                                    QStringLiteral("Backdrop for transparent PNG / SVG / PSD (Off / White / Black)")));
    undoBtn_->setText(trUi(QStringLiteral("Отменить"), QStringLiteral("Undo")));
    undoBtn_->setToolTip(trUi(QStringLiteral("Отменить последнее изменение (Ctrl+Z)"),
                              QStringLiteral("Undo last change (Ctrl+Z)")));
    resetEditsBtn_->setText(trUi(QStringLiteral("Сбросить всё"), QStringLiteral("Reset All")));
    edRightToggleBtn_->setText(trUi(QStringLiteral("Слои"), QStringLiteral("Layers")));
    edRightToggleBtn_->setToolTip(trUi(QStringLiteral("Показать / скрыть правую панель слоёв и метаданных"),
                                       QStringLiteral("Show / hide right layers and metadata panel")));
    exportBtn_->setText(trUi(QStringLiteral("Экспорт"), QStringLiteral("Export")));
    exportBtn_->setToolTip(trUi(QStringLiteral("Экспортировать изображение (PNG, JPG, GIF, WebP...)"),
                                QStringLiteral("Export image (PNG, JPG, GIF, WebP...)")));
    edSettingsBtn_->setToolTip(trUi(QStringLiteral("Настройки темы и языка"), QStringLiteral("Theme & Language Settings")));

    edTabs_->setTabText(0, trUi(QStringLiteral("Базовые"), QStringLiteral("Basic")));
    edTabs_->setTabText(1, trUi(QStringLiteral("Свет и цвет"), QStringLiteral("Light && Color")));
    edTabs_->setTabText(2, trUi(QStringLiteral("Обработка"), QStringLiteral("Effects")));
    edTabs_->setTabText(3, trUi(QStringLiteral("Арт"), QStringLiteral("Art")));
    edTabs_->setTabText(4, trUi(QStringLiteral("Рамки"), QStringLiteral("Frames")));

    cropHint_->setText(trUi(QStringLiteral("Перетягивайте рамку и углы на фото в нужное место:"),
                            QStringLiteral("Drag crop box and handles anywhere on the photo:")));
    applyCropBtn_->setText(trUi(QStringLiteral("Применить кадр"), QStringLiteral("Apply Crop")));
    resetCropBtn_->setText(trUi(QStringLiteral("Сбросить кадр"), QStringLiteral("Reset Crop")));

    straightenRow_->setTitle(trUi(QStringLiteral("Выравнивание горизонта"), QStringLiteral("Straighten")));
    resetBasicBtn_->setText(trUi(QStringLiteral("Сбросить выравнивание"), QStringLiteral("Reset Straighten")));

    brightnessRow_->setTitle(trUi(QStringLiteral("Выравнивающее осветление"), QStringLiteral("Fill Light")));
    highlightsRow_->setTitle(trUi(QStringLiteral("Осветление (Света)"), QStringLiteral("Highlights")));
    shadowsRow_->setTitle(trUi(QStringLiteral("Затенение (Тени)"), QStringLiteral("Shadows")));
    contrastRow_->setTitle(trUi(QStringLiteral("Контраст"), QStringLiteral("Contrast")));
    saturationRow_->setTitle(trUi(QStringLiteral("Насыщенность"), QStringLiteral("Saturation")));
    tempRow_->setTitle(trUi(QStringLiteral("Цветовая температура"), QStringLiteral("Color Temperature")));
    tintRow_->setTitle(trUi(QStringLiteral("Оттенок (Зел / Пурп)"), QStringLiteral("Tint (Green / Magenta)")));
    hueShiftRow_->setTitle(trUi(QStringLiteral("Цветовой тон (все цвета)"), QStringLiteral("Hue Shift (All Colors)")));

    eyedropperBtn_->setText(trUi(QStringLiteral("Пипетка баланса белого"), QStringLiteral("White Balance Picker")));
    eyedropperBtn_->setToolTip(trUi(QStringLiteral("Нажмите на нейтрально-серую или белую область фото"),
                                    QStringLiteral("Click a neutral gray or white area on the photo")));
    autoColorBtn_->setText(trUi(QStringLiteral("Автобаланс"), QStringLiteral("Auto Balance")));
    resetTuningBtn_->setText(trUi(QStringLiteral("Сбросить свет и цвет"), QStringLiteral("Reset Light && Color")));

    removeSelectedEffBtn_->setText(trUi(QStringLiteral("Удалить слой"), QStringLiteral("Remove Layer")));
    textDragHint_->setText(trUi(QStringLiteral("Перетаскивайте текст прямо по фото мышью в любое место."),
                                QStringLiteral("Drag text directly across the photo to place it anywhere.")));
    textLayerEdit_->setPlaceholderText(trUi(QStringLiteral("Текст на фото..."), QStringLiteral("Photo text...")));
    textLayerSizeRow_->setTitle(trUi(QStringLiteral("Размер текста"), QStringLiteral("Text Size")));
    textColorBtn_->setText(trUi(QStringLiteral("Цвет текста"), QStringLiteral("Text Color")));
    addAnotherTextBtn_->setText(trUi(QStringLiteral("+ Добавить ещё текст"), QStringLiteral("+ Add Another Text")));

    ccTitle_->setText(trUi(QStringLiteral("✂ Режим кадрирования — двигайте рамку мышью"),
                           QStringLiteral("✂ Crop Mode — drag frame handles on the image")));
    canvasApplyCropBtn_->setText(trUi(QStringLiteral("✓ Применить кадрирование"), QStringLiteral("✓ Apply Crop")));
    canvasResetCropBtn_->setText(trUi(QStringLiteral("Сбросить"), QStringLiteral("Reset")));
    canvasCloseCropBtn_->setText(trUi(QStringLiteral("Закрыть"), QStringLiteral("Close")));
    editorCaptionEdit_->setPlaceholderText(trUi(QStringLiteral("Создать подпись к фотографии..."),
                                                QStringLiteral("Add a photo caption...")));

    editorStarBtn_->setText(editingRecord_.starred
                                ? trUi(QStringLiteral("В избранном"), QStringLiteral("Favorited"))
                                : trUi(QStringLiteral("Избранное"), QStringLiteral("Favorite")));

    stackHeader_->setText(trUi(QStringLiteral("Слои эффектов и коррекций"), QStringLiteral("Effects & Adjustments Stack")));
    stackHint_->setText(trUi(QStringLiteral("Галочка включает/выключает слой; клик открывает его параметры."),
                             QStringLiteral("Checkbox toggles layer; click edits its settings.")));
    layerUpBtn_->setText(trUi(QStringLiteral("▲ Выше"), QStringLiteral("▲ Up")));
    layerDownBtn_->setText(trUi(QStringLiteral("▼ Ниже"), QStringLiteral("▼ Down")));
    layerDeleteBtn_->setText(trUi(QStringLiteral("✕ Удалить"), QStringLiteral("✕ Delete")));

    histTitle_->setText(trUi(QStringLiteral("Гистограмма и метаданные"), QStringLiteral("Histogram & Metadata")));
    metadataTextEdit_->setPlaceholderText(trUi(QStringLiteral("Промпт и метаданные генерации не обнаружены"),
                                               QStringLiteral("No prompt or generation metadata detected")));
    copyMetaBtn_->setText(trUi(QStringLiteral("Копировать"), QStringLiteral("Copy")));
    stripMetaBtn_->setText(stripMetadataOnExport_
                               ? trUi(QStringLiteral("Без метаданных ✓"), QStringLiteral("Metadata Stripped ✓"))
                               : trUi(QStringLiteral("Удалить метаданные"), QStringLiteral("Strip Metadata")));
    metaHintLabel_->setText(trUi(
        QStringLiteral("Исходный файл на диске не изменяется. При экспорте метаданные сохраняются автоматически, пока кнопка удаления не нажата."),
        QStringLiteral("Original file on disk is untouched. Export preserves metadata automatically unless stripping is toggled.")));
    editorLayersTitle_->setText(trUi(QStringLiteral("Слои PSD (клик для просмотра):"),
                                     QStringLiteral("PSD Layers (click to inspect):")));
    editorExportPsdLayerBtn_->setText(trUi(QStringLiteral("Экспортировать слой PNG"),
                                           QStringLiteral("Export Layer PNG")));

    const QList<MagneticSliderRow*> sliderRows = {
        straightenRow_,
        brightnessRow_, highlightsRow_, shadowsRow_, contrastRow_,
        saturationRow_, tempRow_, tintRow_, hueShiftRow_,
        textLayerSizeRow_, effectIntensityRow_, effectParam2Row_
    };
    for (MagneticSliderRow* r : sliderRows) {
        if (r) r->refreshTheme();
    }

    reloadSidebar();
    if (!editingRecord_.filePath.isEmpty()) {
        populateEditorThumbnails();
        populateEffectStackList();
        updateActiveEffectControls();
        refreshMetadataInspector();
        populateEditorPsdLayers();
        updateCropReadout();
    }
}

void MainWindow::reloadSidebar() {
    const int oldScroll = sidebarList_->verticalScrollBar()->value();
    const QSignalBlocker blocker(sidebarList_);
    sidebarList_->clear();
    trayAlbumCombo_->clear();

    const bool dark = AppSettings::instance().effectiveDarkTheme();
    auto addHeader = [&](const QString& sectionKey, const QString& title, int count, bool collapsed) {
        const QString arrow = collapsed ? QStringLiteral("▸") : QStringLiteral("▾");
        const QString label = (count >= 0)
                                  ? QString("%1 %2 (%3)").arg(arrow, title.toUpper()).arg(count)
                                  : QString("%1 %2").arg(arrow, title.toUpper());
        auto* hdr = new QListWidgetItem(label);
        hdr->setFlags(Qt::ItemIsEnabled);
        QFont f = hdr->font();
        f.setPointSize(8);
        f.setBold(true);
        hdr->setFont(f);
        hdr->setForeground(dark ? QColor(0x94, 0xA3, 0xB8) : QColor(0x64, 0x74, 0x8B));
        hdr->setData(Qt::UserRole, QStringLiteral("section"));
        hdr->setData(Qt::UserRole + 1, sectionKey);
        hdr->setData(Qt::UserRole + 3, label);
        hdr->setToolTip(label + QStringLiteral("\n") +
                        (collapsed
                             ? trUi(QStringLiteral("Нажмите, чтобы развернуть список"), QStringLiteral("Click to expand"))
                             : trUi(QStringLiteral("Нажмите, чтобы свернуть список"), QStringLiteral("Click to collapse"))));
        sidebarList_->addItem(hdr);
    };

    QListWidgetItem* activeItem = nullptr;

    const QPair<int, int> collStats = db_->countCollectionStats();
    addHeader(QStringLiteral("collection"),
              trUi(QStringLiteral("Коллекция"), QStringLiteral("Collection")),
              -1, collectionCollapsed_);
    if (!collectionCollapsed_) {
        const QString allLabel = QString("%1 (%2)")
                                     .arg(trUi(QStringLiteral("Все фотографии"), QStringLiteral("All Photos")))
                                     .arg(collStats.first);
        auto* allItem = new QListWidgetItem(
            makeSidebarThumbIcon(QImage(), QColor(0xA8, 0x55, 0xF7), UiIconKind::AllPhotos),
            allLabel);
        allItem->setData(Qt::UserRole, QStringLiteral("all"));
        allItem->setData(Qt::UserRole + 3, allLabel);
        allItem->setToolTip(allLabel);
        sidebarList_->addItem(allItem);

        const QString starLabel = QString("%1 (%2)")
                                      .arg(trUi(QStringLiteral("Избранное"), QStringLiteral("Favorites")))
                                      .arg(collStats.second);
        auto* starItem = new QListWidgetItem(
            makeUnifiedStarIcon(true, 18),
            starLabel);
        starItem->setData(Qt::UserRole, QStringLiteral("starred"));
        starItem->setData(Qt::UserRole + 3, starLabel);
        starItem->setToolTip(starLabel);
        sidebarList_->addItem(starItem);

        if (onlyStarredFilter_) {
            activeItem = starItem;
        } else if (currentAlbumFilter_ == 0 && currentFolderFilter_.isEmpty()) {
            activeItem = allItem;
        }
    }

    const QList<AlbumRecord> albums = db_->listAlbums();
    addHeader(QStringLiteral("albums"),
              trUi(QStringLiteral("Альбомы"), QStringLiteral("Albums")),
              albums.size(), albumsCollapsed_);
    for (const AlbumRecord& alb : albums) {
        trayAlbumCombo_->addItem(alb.name, alb.id);
        if (albumsCollapsed_) continue;

        const QImage thumb = (!alb.previewFilePath.isEmpty() && thumbImageCache_.contains(alb.previewFilePath))
                                 ? thumbImageCache_.value(alb.previewFilePath)
                                 : QImage();
        if (!alb.previewFilePath.isEmpty() && thumb.isNull()) {
            enqueueThumbnail(alb.previewFilePath, false);
        }
        const QString albLabel = QString("%1 (%2)").arg(alb.name).arg(alb.photoCount);
        auto* item = new QListWidgetItem(
            makeSidebarThumbIcon(thumb, QColor(0x7C, 0x3A, 0xED), UiIconKind::Album, alb.iconKey),
            albLabel);
        item->setData(Qt::UserRole, QStringLiteral("album"));
        item->setData(Qt::UserRole + 1, alb.id);
        item->setData(Qt::UserRole + 2, alb.previewFilePath);
        item->setData(Qt::UserRole + 3, albLabel);
        item->setData(Qt::UserRole + 4, alb.iconKey);
        item->setToolTip(albLabel);
        sidebarList_->addItem(item);
        if (currentAlbumFilter_ == alb.id) {
            activeItem = item;
        }
    }

    if (trayAlbumCombo_->count() == 0) {
        trayAlbumCombo_->addItem(trUi(QStringLiteral("— Нет альбомов —"), QStringLiteral("— No Albums —")), 0);
        trayAlbumCombo_->setEnabled(false);
        trayAddToAlbumBtn_->setEnabled(false);
    } else {
        trayAlbumCombo_->setEnabled(true);
        trayAddToAlbumBtn_->setEnabled(true);
    }

    const QList<FolderSummary> folders = db_->listFolders();
    addHeader(QStringLiteral("folders"),
              trUi(QStringLiteral("Папки на диске"), QStringLiteral("Disk Folders")),
              folders.size(), foldersCollapsed_);
    if (!foldersCollapsed_) {
        for (const FolderSummary& fld : folders) {
            const QImage thumb = (!fld.previewFilePath.isEmpty() && thumbImageCache_.contains(fld.previewFilePath))
                                     ? thumbImageCache_.value(fld.previewFilePath)
                                     : QImage();
            if (!fld.previewFilePath.isEmpty() && thumb.isNull()) {
                enqueueThumbnail(fld.previewFilePath, false);
            }
            const QString fldLabel = QString("%1 (%2)").arg(fld.displayName).arg(fld.photoCount);
            auto* item = new QListWidgetItem(
                makeSidebarThumbIcon(thumb, QColor(0x02, 0x84, 0xC7), UiIconKind::Folder),
                fldLabel);
            item->setData(Qt::UserRole, QStringLiteral("folder"));
            item->setData(Qt::UserRole + 1, fld.folderPath);
            item->setData(Qt::UserRole + 2, fld.previewFilePath);
            item->setData(Qt::UserRole + 3, fldLabel);
            item->setToolTip(fldLabel + QStringLiteral("\n") + fld.folderPath);
            sidebarList_->addItem(item);
            if (!currentFolderFilter_.isEmpty() &&
                QString::compare(currentFolderFilter_, fld.folderPath, Qt::CaseInsensitive) == 0) {
                activeItem = item;
            }
        }
    }

    if (activeItem) {
        sidebarList_->setCurrentItem(activeItem);
    }
    updateSidebarCompactMode();
    sidebarList_->verticalScrollBar()->setValue(oldScroll);
}

void MainWindow::updateSidebarCompactMode() {
    if (!sidebarList_) return;
    const bool compact = (sidebarList_->width() <= 115);
    sidebarCompact_ = compact;
    const QSignalBlocker blocker(sidebarList_);
    for (int i = 0; i < sidebarList_->count(); ++i) {
        QListWidgetItem* item = sidebarList_->item(i);
        if (!item) continue;
        const QString kind = item->data(Qt::UserRole).toString();
        const QString fullText = item->data(Qt::UserRole + 3).toString();
        if (kind == QStringLiteral("section")) {
            item->setText(compact ? QStringLiteral("•••") : fullText);
            item->setTextAlignment(compact ? Qt::AlignCenter : (Qt::AlignLeft | Qt::AlignVCenter));
        } else {
            item->setText(compact ? QString() : fullText);
            item->setTextAlignment(compact ? Qt::AlignCenter : (Qt::AlignLeft | Qt::AlignVCenter));
        }
    }
}

void MainWindow::updateSidebarThumbnailsForPath(const QString& filePath) {
    const QImage img = thumbImageCache_.value(filePath);
    if (img.isNull()) return;
    for (int i = 0; i < sidebarList_->count(); ++i) {
        QListWidgetItem* item = sidebarList_->item(i);
        if (!item) continue;
        if (item->data(Qt::UserRole + 2).toString() == filePath) {
            const QString kind = item->data(Qt::UserRole).toString();
            const bool isAlb = (kind == QStringLiteral("album"));
            const QString albIconKey = isAlb ? item->data(Qt::UserRole + 4).toString() : QString();
            item->setIcon(makeSidebarThumbIcon(img, isAlb ? QColor(0x7C, 0x3A, 0xED) : QColor(0x02, 0x84, 0xC7),
                                               isAlb ? UiIconKind::Album : UiIconKind::Folder,
                                               albIconKey));
        }
    }
}

void MainWindow::reloadPhotoGrid() {
    photoGrid_->setUpdatesEnabled(false);
    photoGrid_->clear();
    thumbQueue_.clear();
    pendingThumbs_.clear();

    QList<PhotoRecord> rawList = db_->queryPhotos(currentFolderFilter_, currentAlbumFilter_, onlyStarredFilter_, searchQuery_);
    if (rawList.size() <= 500) {
        QList<qint64> missingIds;
        QList<PhotoRecord> existing;
        existing.reserve(rawList.size());
        for (const PhotoRecord& rec : rawList) {
            if (QFileInfo::exists(rec.filePath)) {
                existing.append(rec);
            } else {
                missingIds.append(rec.id);
            }
        }
        if (!missingIds.isEmpty()) {
            const QStringList removedPaths = db_->removePhotosByIds(missingIds);
            purgePathCaches(removedPaths);
            rawList = existing;
            reloadSidebar();
        }
    }
    currentPhotos_ = std::move(rawList);
    statusCountLabel_->setText(trUi(QString("%1 фото").arg(currentPhotos_.size()),
                                    QString("%1 photos").arg(currentPhotos_.size())));
    gridContentStack_->setCurrentIndex(currentPhotos_.isEmpty() ? 0 : 1);

    const int cardW = thumbSize_;
    const int cardH = thumbSize_ * 3 / 4;
    QPixmap placeholderPix(cardW, cardH);
    placeholderPix.fill(AppSettings::instance().effectiveDarkTheme()
                            ? QColor(0x23, 0x26, 0x34)
                            : QColor(0xE2, 0xE8, 0xF0));
    const QIcon placeholderIcon(placeholderPix);

    for (int i = 0; i < currentPhotos_.size(); ++i) {
        const PhotoRecord& rec = currentPhotos_[i];
        const QString title = rec.caption.isEmpty() ? rec.fileName : rec.caption;
        auto* item = new QListWidgetItem(title);
        item->setData(Qt::UserRole, rec.filePath);
        item->setData(PhotoCardDelegate::StarredRole, rec.starred);
        item->setData(PhotoCardDelegate::IsPsdRole, rec.filePath.endsWith(QStringLiteral(".psd"), Qt::CaseInsensitive));

        if (thumbImageCache_.contains(rec.filePath)) {
            const QPixmap pm = QPixmap::fromImage(thumbImageCache_.value(rec.filePath));
            item->setData(PhotoCardDelegate::ThumbPixmapRole, pm);
            item->setIcon(QIcon(pm));
        } else {
            item->setData(PhotoCardDelegate::ThumbPixmapRole, placeholderPix);
            item->setIcon(placeholderIcon);
        }
        photoGrid_->addItem(item);
    }

    photoGrid_->setUpdatesEnabled(true);
    prioritizeVisibleThumbnails();

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!pruneInFlight_ && (nowMs - lastPruneMs_ > 2500)) {
        pruneMissingFilesAsync();
    }
}

void MainWindow::refreshGridItemIcon(int row) {
    if (row < 0 || row >= currentPhotos_.size() || row >= photoGrid_->count()) return;
    const PhotoRecord& rec = currentPhotos_[row];
    QListWidgetItem* item = photoGrid_->item(row);
    item->setData(PhotoCardDelegate::StarredRole, rec.starred);
    item->setData(PhotoCardDelegate::IsPsdRole, rec.filePath.endsWith(QStringLiteral(".psd"), Qt::CaseInsensitive));
    if (thumbImageCache_.contains(rec.filePath)) {
        const QPixmap pm = QPixmap::fromImage(thumbImageCache_.value(rec.filePath));
        item->setData(PhotoCardDelegate::ThumbPixmapRole, pm);
        item->setIcon(QIcon(pm));
    }
    const QString title = rec.caption.isEmpty() ? rec.fileName : rec.caption;
    item->setText(title);
}

void MainWindow::enqueueThumbnail(const QString& filePath, bool highPriority) {
    if (filePath.isEmpty() || thumbImageCache_.contains(filePath) || inFlightThumbs_.contains(filePath)) return;
    if (pendingThumbs_.contains(filePath)) {
        if (highPriority) {
            thumbQueue_.removeOne(filePath);
            thumbQueue_.prepend(filePath);
        }
        return;
    }
    pendingThumbs_.insert(filePath);
    if (highPriority) {
        thumbQueue_.prepend(filePath);
    } else {
        thumbQueue_.append(filePath);
    }
    pumpThumbnailQueue();
}

void MainWindow::prioritizeVisibleThumbnails() {
    if (currentPhotos_.isEmpty()) return;
    const QRect vp = photoGrid_->viewport()->rect();
    const int total = photoGrid_->count();
    QList<QString> visiblePaths;
    for (int i = 0; i < total; ++i) {
        const QRect r = photoGrid_->visualItemRect(photoGrid_->item(i));
        if (r.intersects(vp)) {
            const QString fp = currentPhotos_[i].filePath;
            if (!thumbImageCache_.contains(fp)) {
                visiblePaths.append(fp);
            }
        }
    }
    for (int i = visiblePaths.size() - 1; i >= 0; --i) {
        enqueueThumbnail(visiblePaths[i], true);
    }
    // Also queue a small buffer around visible items rather than blocking with 600+ immediately
    int queued = 0;
    for (int i = 0; i < total && queued < 60; ++i) {
        const QString fp = currentPhotos_[i].filePath;
        if (!thumbImageCache_.contains(fp) && !pendingThumbs_.contains(fp) && !inFlightThumbs_.contains(fp)) {
            enqueueThumbnail(fp, false);
            ++queued;
        }
    }
}

void MainWindow::pumpThumbnailQueue() {
    constexpr int kMaxWorkers = 4;
    while (inFlightThumbs_.size() < kMaxWorkers && !thumbQueue_.isEmpty()) {
        const QString filePath = thumbQueue_.takeFirst();
        pendingThumbs_.remove(filePath);
        if (thumbImageCache_.contains(filePath) || inFlightThumbs_.contains(filePath)) continue;
        if (!QFileInfo::exists(filePath)) {
            removeMissingPhotoFromView(filePath);
            continue;
        }
        inFlightThumbs_.insert(filePath);

        auto* watcher = new QFutureWatcher<LoadedImageResult>(this);
        connect(watcher, &QFutureWatcher<LoadedImageResult>::finished, this, [this, watcher, filePath]() {
            inFlightThumbs_.remove(filePath);
            const LoadedImageResult res = watcher->result();
            watcher->deleteLater();

            if (res.success && !res.image.isNull()) {
                thumbImageCache_.insert(filePath, res.image);
                updateSidebarThumbnailsForPath(filePath);

                for (int i = 0; i < currentPhotos_.size(); ++i) {
                    if (currentPhotos_[i].filePath == filePath) {
                        refreshGridItemIcon(i);
                    }
                }
                const QPixmap filmPm = QPixmap::fromImage(res.image.scaled(44, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                const QIcon filmIcon(filmPm);
                for (int i = 0; i < editorFilmstrip_->count(); ++i) {
                    QListWidgetItem* item = editorFilmstrip_->item(i);
                    if (item && item->data(Qt::UserRole).toString() == filePath) {
                        item->setData(Qt::UserRole + 5, filmPm);
                        item->setIcon(filmIcon);
                    }
                }
            } else if (!QFileInfo::exists(filePath)) {
                removeMissingPhotoFromView(filePath);
            }

            if (!thumbQueue_.isEmpty()) {
                pumpThumbnailQueue();
            } else if (inFlightThumbs_.isEmpty()) {
                // Continue loading remaining unseen items lazily in small batches
                prioritizeVisibleThumbnails();
            }
        });

        watcher->setFuture(QtConcurrent::run([filePath]() {
            return ImageLoader::loadImage(filePath, 320);
        }));
    }
}

void MainWindow::startFolderImport(const QString& dirPath, const QString& progressTitle) {
    auto cancelled = std::make_shared<std::atomic<bool>>(false);

    auto* progress = new QProgressDialog(
        trUi(QStringLiteral("Поиск изображений в папке..."), QStringLiteral("Scanning folder for images...")),
        trUi(QStringLiteral("Отмена"), QStringLiteral("Cancel")),
        0, 0, this);
    progress->setWindowTitle(progressTitle);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setMinimumWidth(420);
    progress->show();

    connect(progress, &QProgressDialog::canceled, this, [cancelled]() {
        cancelled->store(true);
    });

    auto* watcher = new QFutureWatcher<QList<ScannedPhotoMeta>>(this);
    connect(watcher, &QFutureWatcher<QList<ScannedPhotoMeta>>::finished, this, [this, watcher, progress, cancelled, dirPath, progressTitle]() {
        const QList<ScannedPhotoMeta> items = watcher->result();
        watcher->deleteLater();
        progress->setLabelText(trUi(
            QString("Сохранение %1 изображений в библиотеку...").arg(items.size()),
            QString("Saving %1 images to library...").arg(items.size())));
        QApplication::processEvents();

        const int count = db_->indexScannedBatch(dirPath, items);
        progress->close();
        progress->deleteLater();

        currentFolderFilter_ = QDir::toNativeSeparators(QFileInfo(dirPath).absoluteFilePath());
        currentAlbumFilter_ = 0;
        onlyStarredFilter_ = false;
        {
            const QSignalBlocker b(starredFilterBtn_);
            starredFilterBtn_->setChecked(false);
            starredFilterBtn_->setIcon(makeUnifiedStarIcon(false, 16));
        }
        reloadSidebar();
        reloadPhotoGrid();

        if (!cancelled->load()) {
            QMessageBox::information(this, progressTitle,
                                     trUi(QString("Проиндексировано изображений: %1").arg(count),
                                          QString("Indexed images: %1").arg(count)));
        }
    });

    QPointer<QProgressDialog> safeProg(progress);
    watcher->setFuture(QtConcurrent::run([dirPath, cancelled, safeProg]() {
        return LibraryDatabase::collectFolderImages(dirPath, [cancelled, safeProg](int found, const QString& fname) {
            if (cancelled->load()) return false;
            if (found == 1 || found % 12 == 0) {
                QMetaObject::invokeMethod(qApp, [safeProg, found, fname]() {
                    if (safeProg && safeProg->isVisible()) {
                        safeProg->setLabelText(trUi(
                            QString("Найдено и обработано: %1\n%2").arg(found).arg(fname),
                            QString("Found and processed: %1\n%2").arg(found).arg(fname)));
                    }
                }, Qt::QueuedConnection);
            }
            return true;
        });
    }));
}

void MainWindow::onAddFolderClicked() {
    const QString dir = QFileDialog::getExistingDirectory(
        this, trUi(QStringLiteral("Выберите папку с фотографиями"), QStringLiteral("Select Photos Folder")));
    if (dir.isEmpty()) return;
    startFolderImport(dir, trUi(QStringLiteral("Импорт папки"), QStringLiteral("Import Folder")));
}

void MainWindow::onScanPicturesClicked() {
    const QString picsDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (picsDir.isEmpty()) {
        onAddFolderClicked();
        return;
    }
    startFolderImport(picsDir, trUi(QStringLiteral("Сканирование «Изображения»"), QStringLiteral("Scan Pictures")));
}

void MainWindow::onCreateAlbumClicked() {
    AlbumPropertiesDialog dlg(true, QString(), QString(), this);
    if (dlg.exec() == QDialog::Accepted && !dlg.albumName().isEmpty()) {
        db_->createAlbum(dlg.albumName(), dlg.selectedIconKey());
        reloadSidebar();
    }
}

void MainWindow::onRegisterAssociationsClicked() {
    FileAssociationsDialog dlg(this);
    dlg.exec();
}

void MainWindow::onSidebarItemActivated(QListWidgetItem* current) {
    if (!current) return;
    const QString kind = current->data(Qt::UserRole).toString();
    if (kind == QStringLiteral("section")) {
        const QString sec = current->data(Qt::UserRole + 1).toString();
        QSettings st(QStringLiteral("Lumina"), QStringLiteral("LuminaPhoto"));
        if (sec == QStringLiteral("collection")) {
            collectionCollapsed_ = !collectionCollapsed_;
            st.setValue(QStringLiteral("sidebar/collectionCollapsed"), collectionCollapsed_);
        } else if (sec == QStringLiteral("albums")) {
            albumsCollapsed_ = !albumsCollapsed_;
            st.setValue(QStringLiteral("sidebar/albumsCollapsed"), albumsCollapsed_);
        } else if (sec == QStringLiteral("folders")) {
            foldersCollapsed_ = !foldersCollapsed_;
            st.setValue(QStringLiteral("sidebar/foldersCollapsed"), foldersCollapsed_);
        }
        reloadSidebar();
        return;
    }
    if (kind == QStringLiteral("all")) {
        currentFolderFilter_.clear();
        currentAlbumFilter_ = 0;
        onlyStarredFilter_ = false;
    } else if (kind == QStringLiteral("starred")) {
        currentFolderFilter_.clear();
        currentAlbumFilter_ = 0;
        onlyStarredFilter_ = true;
    } else if (kind == QStringLiteral("folder")) {
        currentFolderFilter_ = current->data(Qt::UserRole + 1).toString();
        currentAlbumFilter_ = 0;
        onlyStarredFilter_ = false;
    } else if (kind == QStringLiteral("album")) {
        currentFolderFilter_.clear();
        currentAlbumFilter_ = current->data(Qt::UserRole + 1).toLongLong();
        onlyStarredFilter_ = false;
    } else {
        return;
    }

    {
        const QSignalBlocker b(starredFilterBtn_);
        starredFilterBtn_->setChecked(onlyStarredFilter_);
        starredFilterBtn_->setIcon(makeUnifiedStarIcon(onlyStarredFilter_, 16));
    }

    mainStack_->setCurrentIndex(0);
    reloadPhotoGrid();
}

void MainWindow::showSidebarContextMenu(const QPoint& pos) {
    QListWidgetItem* item = sidebarList_->itemAt(pos);
    if (!item) return;
    const QString kind = item->data(Qt::UserRole).toString();

    if (kind == QStringLiteral("section")) {
        QMenu menu(this);
        const QString sec = item->data(Qt::UserRole + 1).toString();
        const bool collapsed = (sec == QStringLiteral("collection")) ? collectionCollapsed_
                               : (sec == QStringLiteral("albums") ? albumsCollapsed_ : foldersCollapsed_);
        QAction* toggleAct = menu.addAction(
            collapsed ? trUi(QStringLiteral("Развернуть раздел"), QStringLiteral("Expand Section"))
                      : trUi(QStringLiteral("Свернуть раздел"), QStringLiteral("Collapse Section")));
        if (menu.exec(sidebarList_->viewport()->mapToGlobal(pos)) == toggleAct) {
            onSidebarItemActivated(item);
        }
        return;
    }

    if (kind == QStringLiteral("folder")) {
        const QString folderPath = item->data(Qt::UserRole + 1).toString();
        if (folderPath.isEmpty()) return;

        QMenu menu(this);
        QAction* openExplorerAct = menu.addAction(
            makeUiIcon(UiIconKind::Folder, QColor(0x02, 0x84, 0xC7), 15),
            trUi(QStringLiteral("Открыть в Проводнике"), QStringLiteral("Open in Explorer")));
        menu.addSeparator();
        QAction* removeOneAct = menu.addAction(
            makeUiIcon(UiIconKind::Trash, QColor(0xEF, 0x44, 0x44), 15),
            trUi(QStringLiteral("Убрать папку из библиотеки"), QStringLiteral("Remove Folder from Library")));
        QAction* removeTreeAct = nullptr;
        if (db_->hasSubfoldersInLibrary(folderPath)) {
            removeTreeAct = menu.addAction(
                makeUiIcon(UiIconKind::Trash, QColor(0xEF, 0x44, 0x44), 15),
                trUi(QStringLiteral("Убрать папку и все подпапки"), QStringLiteral("Remove Folder & Subfolders")));
        }

        QAction* chosen = menu.exec(sidebarList_->viewport()->mapToGlobal(pos));
        if (!chosen) return;

        if (chosen == openExplorerAct) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
            return;
        }
        const bool includeSubs = (chosen == removeTreeAct);
        if (chosen == removeOneAct || includeSubs) {
            const auto ans = QMessageBox::question(
                this,
                trUi(QStringLiteral("Убрать папку из Lumina Photo"), QStringLiteral("Remove Folder from Lumina Photo")),
                trUi(QString("Убрать «%1» из библиотеки?\n\nФайлы на диске останутся нетронутыми — удалится только запись из индекса программы.")
                         .arg(QFileInfo(folderPath).fileName().isEmpty() ? folderPath : QFileInfo(folderPath).fileName()),
                     QString("Remove \"%1\" from library?\n\nFiles on disk will NOT be deleted — only the index entry will be removed.")
                         .arg(QFileInfo(folderPath).fileName().isEmpty() ? folderPath : QFileInfo(folderPath).fileName())),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if (ans != QMessageBox::Yes) return;

            const QStringList removedPaths = db_->removeFolderFromLibrary(folderPath, includeSubs);
            purgePathCaches(removedPaths);
            if (!currentFolderFilter_.isEmpty()) {
                const QString normCur = QDir::toNativeSeparators(QFileInfo(currentFolderFilter_).absoluteFilePath()).toLower();
                const QString normDel = QDir::toNativeSeparators(QFileInfo(folderPath).absoluteFilePath()).toLower();
                if (normCur == normDel || (includeSubs && normCur.startsWith(normDel + QDir::separator()))) {
                    currentFolderFilter_.clear();
                }
            }
            reloadSidebar();
            reloadPhotoGrid();
        }
        return;
    }

    if (kind == QStringLiteral("album")) {
        const qint64 albId = item->data(Qt::UserRole + 1).toLongLong();
        if (albId <= 0) return;
        const QString curIconKey = item->data(Qt::UserRole + 4).toString();

        QMenu menu(this);
        QAction* pickIconAct = menu.addAction(
            curIconKey.isEmpty()
                ? makeUiIcon(UiIconKind::Album, QColor(0xA8, 0x55, 0xF7), 16)
                : makeAlbumPresetIcon(curIconKey, 16),
            trUi(QStringLiteral("Выбрать иконку..."), QStringLiteral("Choose Icon...")));
        QAction* clearIconAct = nullptr;
        if (!curIconKey.isEmpty()) {
            clearIconAct = menu.addAction(
                makeUiIcon(UiIconKind::Undo, QColor(0x94, 0xA3, 0xB8), 15),
                trUi(QStringLiteral("Убрать иконку"), QStringLiteral("Remove Icon")));
        }
        menu.addSeparator();
        QAction* delAlbAct = menu.addAction(
            makeUiIcon(UiIconKind::Trash, QColor(0xEF, 0x44, 0x44), 15),
            trUi(QStringLiteral("Удалить альбом"), QStringLiteral("Delete Album")));

        QAction* chosen = menu.exec(sidebarList_->viewport()->mapToGlobal(pos));
        if (!chosen) return;
        if (chosen == pickIconAct) {
            AlbumPropertiesDialog dlg(false, QString(), curIconKey, this);
            if (dlg.exec() == QDialog::Accepted) {
                db_->setAlbumIcon(albId, dlg.selectedIconKey());
                reloadSidebar();
            }
        } else if (clearIconAct && chosen == clearIconAct) {
            db_->setAlbumIcon(albId, QString());
            reloadSidebar();
        } else if (chosen == delAlbAct) {
            db_->deleteAlbum(albId);
            if (currentAlbumFilter_ == albId) {
                currentAlbumFilter_ = 0;
            }
            reloadSidebar();
            reloadPhotoGrid();
        }
    }
}

void MainWindow::purgePathCaches(const QStringList& paths) {
    for (const QString& p : paths) {
        thumbImageCache_.remove(p);
        editorCache_.remove(p);
        editorCacheOrder_.removeAll(p);
        pendingThumbs_.remove(p);
        thumbQueue_.removeAll(p);
    }
}

void MainWindow::removeMissingPhotoFromView(const QString& filePath) {
    db_->removePhotoFromLibrary(filePath);
    purgePathCaches({filePath});

    bool changed = false;
    for (int i = currentPhotos_.size() - 1; i >= 0; --i) {
        if (QString::compare(currentPhotos_[i].filePath, filePath, Qt::CaseInsensitive) == 0) {
            currentPhotos_.removeAt(i);
            if (i < photoGrid_->count()) {
                delete photoGrid_->takeItem(i);
            }
            changed = true;
        }
    }
    if (changed) {
        statusCountLabel_->setText(trUi(QString("%1 фото").arg(currentPhotos_.size()),
                                        QString("%1 photos").arg(currentPhotos_.size())));
        gridContentStack_->setCurrentIndex(currentPhotos_.isEmpty() ? 0 : 1);
        reloadSidebar();
    }
}

void MainWindow::pruneMissingFilesAsync() {
    if (pruneInFlight_ || !db_) return;
    pruneInFlight_ = true;
    lastPruneMs_ = QDateTime::currentMSecsSinceEpoch();

    const QList<QPair<qint64, QString>> entries = db_->listAllPhotoIdsAndPaths();
    if (entries.isEmpty()) {
        pruneInFlight_ = false;
        return;
    }

    auto* watcher = new QFutureWatcher<QList<qint64>>(this);
    connect(watcher, &QFutureWatcher<QList<qint64>>::finished, this, [this, watcher]() {
        const QList<qint64> missingIds = watcher->result();
        watcher->deleteLater();
        pruneInFlight_ = false;
        if (!missingIds.isEmpty()) {
            const QStringList removedPaths = db_->removePhotosByIds(missingIds);
            purgePathCaches(removedPaths);
            if (mainStack_ && mainStack_->currentIndex() == 0) {
                reloadSidebar();
                reloadPhotoGrid();
            }
        }
    });

    watcher->setFuture(QtConcurrent::run([entries]() {
        QList<qint64> missing;
        for (const auto& pair : entries) {
            if (!QFileInfo::exists(pair.second)) {
                missing.append(pair.first);
            }
        }
        return missing;
    }));
}

void MainWindow::toggleRowStarred(int row) {
    if (row < 0 || row >= currentPhotos_.size()) return;
    PhotoRecord& rec = currentPhotos_[row];
    rec.starred = !rec.starred;
    db_->setStarred(rec.filePath, rec.starred);
    refreshGridItemIcon(row);
    reloadSidebar();
    if (onlyStarredFilter_ && !rec.starred) {
        reloadPhotoGrid();
    }
}

void MainWindow::showRowAlbumMenu(int row, const QPoint& globalPos) {
    if (row < 0 || row >= currentPhotos_.size()) return;
    const PhotoRecord& rec = currentPhotos_[row];

    QMenu menu(this);
    const QList<AlbumRecord> albums = db_->listAlbums();
    for (const AlbumRecord& alb : albums) {
        const QIcon albIcon = alb.iconKey.isEmpty()
                                  ? makeUiIcon(UiIconKind::Album, QColor(0x7C, 0x3A, 0xED), 16)
                                  : makeAlbumPresetIcon(alb.iconKey, 16);
        QAction* act = menu.addAction(albIcon,
                                      trUi(QString("Добавить в «%1»").arg(alb.name),
                                           QString("Add to \"%1\"").arg(alb.name)));
        act->setData(alb.id);
    }
    if (!albums.isEmpty()) menu.addSeparator();
    QAction* createAct = menu.addAction(
        makeUiIcon(UiIconKind::Plus, QColor(0xA8, 0x55, 0xF7), 16),
        trUi(QStringLiteral("Создать новый альбом и добавить..."),
             QStringLiteral("Create new album and add...")));

    QAction* chosen = menu.exec(globalPos);
    if (!chosen) return;

    if (chosen == createAct) {
        AlbumPropertiesDialog dlg(true, QString(), QString(), this);
        if (dlg.exec() == QDialog::Accepted && !dlg.albumName().isEmpty()) {
            const qint64 newId = db_->createAlbum(dlg.albumName(), dlg.selectedIconKey());
            if (newId > 0) {
                db_->addPhotoToAlbum(rec.id, newId);
                reloadSidebar();
            }
        }
        return;
    }

    const qint64 albId = chosen->data().toLongLong();
    if (albId > 0) {
        db_->addPhotoToAlbum(rec.id, albId);
        reloadSidebar();
    }
}

void MainWindow::setSelectedStarred(bool starred) {
    for (QListWidgetItem* item : photoGrid_->selectedItems()) {
        const int row = photoGrid_->row(item);
        if (row >= 0 && row < currentPhotos_.size()) {
            currentPhotos_[row].starred = starred;
            db_->setStarred(currentPhotos_[row].filePath, starred);
            refreshGridItemIcon(row);
        }
    }
    reloadSidebar();
    if (onlyStarredFilter_ && !starred) {
        reloadPhotoGrid();
    }
}

void MainWindow::onAddSelectedToAlbum() {
    const qint64 albumId = trayAlbumCombo_->currentData().toLongLong();
    if (albumId <= 0) {
        QMessageBox::information(this,
                                 trUi(QStringLiteral("Нет альбомов"), QStringLiteral("No Albums")),
                                 trUi(QStringLiteral("Сначала создайте альбом кнопкой «+ Альбом»."),
                                      QStringLiteral("Create an album first using \"+ Album\".")));
        return;
    }
    for (QListWidgetItem* item : photoGrid_->selectedItems()) {
        const int row = photoGrid_->row(item);
        if (row >= 0 && row < currentPhotos_.size()) {
            db_->addPhotoToAlbum(currentPhotos_[row].id, albumId);
        }
    }
    reloadSidebar();
}

void MainWindow::showPhotoGridContextMenu(const QPoint& pos) {
    QListWidgetItem* item = photoGrid_->itemAt(pos);
    if (!item) return;
    const int row = photoGrid_->row(item);
    if (row < 0 || row >= currentPhotos_.size()) return;
    PhotoRecord& rec = currentPhotos_[row];

    QMenu menu(this);
    QAction* openAct = menu.addAction(trUi(QStringLiteral("Открыть в редакторе"), QStringLiteral("Open in Editor")));
    QAction* starAct = menu.addAction(makeUnifiedStarIcon(!rec.starred, 16),
                                      rec.starred
                                          ? trUi(QStringLiteral("Убрать из избранного"), QStringLiteral("Remove from Favorites"))
                                          : trUi(QStringLiteral("Добавить в избранное"), QStringLiteral("Add to Favorites")));
    QMenu* albumSub = menu.addMenu(trUi(QStringLiteral("Добавить в альбом"), QStringLiteral("Add to Album")));
    const QList<AlbumRecord> albums = db_->listAlbums();
    for (const AlbumRecord& alb : albums) {
        QAction* aa = albumSub->addAction(alb.name);
        aa->setData(alb.id);
    }
    QAction* createAlbAct = albumSub->addAction(trUi(QStringLiteral("+ Новый альбом..."), QStringLiteral("+ New Album...")));

    menu.addSeparator();
    QAction* removeAct = menu.addAction(trUi(QStringLiteral("Убрать из библиотеки"), QStringLiteral("Remove from Library")));

    QAction* chosen = menu.exec(photoGrid_->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == openAct) {
        onOpenEditorForRecord(rec);
    } else if (chosen == starAct) {
        toggleRowStarred(row);
    } else if (chosen == createAlbAct) {
        bool ok = false;
        const QString name = QInputDialog::getText(
            this,
            trUi(QStringLiteral("Новый альбом"), QStringLiteral("New Album")),
            trUi(QStringLiteral("Название альбома:"), QStringLiteral("Album name:")),
            QLineEdit::Normal, QString(), &ok);
        if (ok && !name.trimmed().isEmpty()) {
            const qint64 newId = db_->createAlbum(name.trimmed());
            if (newId > 0) {
                db_->addPhotoToAlbum(rec.id, newId);
                reloadSidebar();
            }
        }
    } else if (chosen == removeAct) {
        db_->removePhotoFromLibrary(rec.filePath);
        reloadSidebar();
        reloadPhotoGrid();
    } else if (chosen->parentWidget() == albumSub && chosen->data().toLongLong() > 0) {
        db_->addPhotoToAlbum(rec.id, chosen->data().toLongLong());
        reloadSidebar();
    }
}

void MainWindow::openPhotoInEditor(const QString& filePath) {
    if (!QFileInfo::exists(filePath)) {
        removeMissingPhotoFromView(filePath);
        QMessageBox::information(
            this,
            trUi(QStringLiteral("Файл не найден"), QStringLiteral("File Not Found")),
            trUi(QStringLiteral("Изображение было удалено или перемещено с диска, запись убрана из библиотеки."),
                 QStringLiteral("The image was deleted or moved on disk and has been removed from the library.")));
        return;
    }
    const PhotoRecord rec = db_->ensurePhotoIndexed(filePath);
    if (rec.filePath.isEmpty()) return;
    currentFolderFilter_ = rec.folderPath;
    currentAlbumFilter_ = 0;
    onlyStarredFilter_ = false;
    {
        const QSignalBlocker b(starredFilterBtn_);
        starredFilterBtn_->setChecked(false);
        starredFilterBtn_->setIcon(makeUnifiedStarIcon(false, 16));
    }
    reloadSidebar();
    reloadPhotoGrid();
    onOpenEditorForRecord(rec);
}

void MainWindow::stopEditorMovie() {
    if (editorMovie_) {
        editorMovie_->stop();
        editorMovie_->deleteLater();
        editorMovie_ = nullptr;
    }
    animFirstFrame_ = false;
    lastAnimHistMs_ = 0;
}

void MainWindow::startEditorMovieIfAnimated(const QString& filePath, bool isAnimated) {
    stopEditorMovie();
    if (!isAnimated) return;

    editorMovie_ = new QMovie(filePath, QByteArray(), this);
    editorMovie_->setCacheMode(QMovie::CacheAll);
    if (!editorMovie_->isValid() || editorMovie_->frameCount() <= 1) {
        stopEditorMovie();
        return;
    }

    animFirstFrame_ = false;
    lastAnimHistMs_ = 0;
    connect(editorMovie_, &QMovie::frameChanged, this, [this](int) {
        if (!editorMovie_ || !editingPreviewLayerImage_.isNull()) return;
        const QImage frame = editorMovie_->currentImage().convertToFormat(QImage::Format_RGBA8888);
        if (frame.isNull()) return;
        editingRawImage_ = frame;

        if (compareOriginal_ || editingRecord_.edits.isDefault()) {
            editorCanvas_->setLiveTextLayer(nullptr);
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            if (!dragPreviewActive_ && nowMs - lastAnimHistMs_ >= 450) {
                lastAnimHistMs_ = nowMs;
                histogramWidget_->setImage(frame);
            }
            editorCanvas_->setDisplayImage(frame, animFirstFrame_);
            animFirstFrame_ = false;
            return;
        }

        const int maxDim = std::max(frame.width(), frame.height());
        if (maxDim > 1600) {
            editingScreenImage_ = frame.scaled(1600, 1600, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        } else {
            editingScreenImage_ = frame;
        }
        if (dragPreviewActive_ && maxDim > 760) {
            editingDragImage_ = frame.scaled(760, 760, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        } else {
            editingDragImage_ = frame;
        }

        QString skipLayerId;
        const EffectLayer* liveLayerPtr = nullptr;
        if (!editorCanvas_->isCropMode()) {
            const int selIdx = selectedEffectLayerIndex();
            if (selIdx >= 0 &&
                editingRecord_.edits.effectLayers[selIdx].preset == EffectPreset::TextOverlay &&
                editingRecord_.edits.effectLayers[selIdx].enabled) {
                skipLayerId = selectedLayerId_;
                liveLayerPtr = &editingRecord_.edits.effectLayers[selIdx];
            }
        }

        const QImage* baseImg = &editingRawImage_;
        if (dragPreviewActive_ && !editingDragImage_.isNull() && !editorCanvas_->isActualSize()) {
            baseImg = &editingDragImage_;
        } else if (!editorCanvas_->isActualSize() && !editingScreenImage_.isNull()) {
            baseImg = &editingScreenImage_;
        }

        const QImage edited = ImageLoader::applyEdits(*baseImg, editingRecord_.edits, editorCanvas_->isCropMode(), skipLayerId);
        if (!editorCanvas_->isDraggingText()) {
            editorCanvas_->setLiveTextLayer(liveLayerPtr);
        }
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (!dragPreviewActive_ && nowMs - lastAnimHistMs_ >= 450) {
            lastAnimHistMs_ = nowMs;
            histogramWidget_->setImage(edited);
        }
        editorCanvas_->setDisplayImage(edited, animFirstFrame_);
        animFirstFrame_ = false;
    });
    editorMovie_->start();
}

void MainWindow::insertEditorCache(const QString& filePath, const CachedEditorAsset& asset) {
    if (filePath.isEmpty() || asset.raw.isNull()) return;
    editorCacheOrder_.removeAll(filePath);
    editorCacheOrder_.append(filePath);
    editorCache_.insert(filePath, asset);
    while (editorCacheOrder_.size() > 5) {
        const QString oldest = editorCacheOrder_.takeFirst();
        editorCache_.remove(oldest);
    }
}

void MainWindow::preloadNeighborInEditor(int delta) {
    if (currentPhotos_.size() <= 1) return;
    int idx = -1;
    for (int i = 0; i < currentPhotos_.size(); ++i) {
        if (currentPhotos_[i].filePath == editingRecord_.filePath) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return;
    const int n = currentPhotos_.size();
    const QString fp = currentPhotos_[(idx + delta + n) % n].filePath;
    if (fp.isEmpty() || editorCache_.contains(fp) || preloadingEditorPaths_.contains(fp)) return;

    preloadingEditorPaths_.insert(fp);
    auto* watcher = new QFutureWatcher<CachedEditorAsset>(this);
    connect(watcher, &QFutureWatcher<CachedEditorAsset>::finished, this, [this, watcher, fp]() {
        preloadingEditorPaths_.remove(fp);
        const CachedEditorAsset asset = watcher->result();
        watcher->deleteLater();
        insertEditorCache(fp, asset);
    });
    watcher->setFuture(QtConcurrent::run([fp]() {
        CachedEditorAsset asset;
        const LoadedImageResult loaded = ImageLoader::loadImage(fp, 0);
        if (!loaded.success || loaded.image.isNull()) return asset;
        asset.raw = loaded.image;
        const int maxDim = std::max(asset.raw.width(), asset.raw.height());
        asset.screen = (maxDim > 1600)
                           ? asset.raw.scaled(1600, 1600, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                           : asset.raw;
        asset.drag = (maxDim > 760)
                         ? asset.raw.scaled(760, 760, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                         : asset.raw;
        asset.meta = loaded.metadata;
        asset.psdLayers = loaded.metadata.psdLayerDetails;
        asset.isAnimated = loaded.isAnimated;
        return asset;
    }));
}

void MainWindow::applyLoadedEditorAsset(const PhotoRecord& rec, const CachedEditorAsset& asset, bool resetView) {
    editingRawImage_ = asset.raw;
    editingScreenImage_ = asset.screen;
    editingDragImage_ = asset.drag;
    editingLoadedMeta_ = asset.meta;
    editingPreviewLayerImage_ = QImage();
    editingPsdLayers_ = asset.psdLayers;
    editingIsAnimated_ = asset.isAnimated;

    editorMetaStatus_->setText(
        QString("%1  ·  %2 × %3 px")
            .arg(rec.fileName)
            .arg(asset.raw.width())
            .arg(asset.raw.height())
    );

    refreshMetadataInspector();
    populateEditorPsdLayers();
    populateEditorThumbnails();
    refreshEditorPreview(resetView);
    startEditorMovieIfAnimated(rec.filePath, asset.isAnimated);

    QTimer::singleShot(0, this, [this]() {
        if (edLeftPane_) adaptLeftPaneLayout(edLeftPane_->width());
    });
}

void MainWindow::onOpenEditorForRecord(const PhotoRecord& rec) {
    if (!QFileInfo::exists(rec.filePath)) {
        removeMissingPhotoFromView(rec.filePath);
        return;
    }
    if (dbSaveTimer_ && dbSaveTimer_->isActive() && !editingRecord_.filePath.isEmpty()) {
        dbSaveTimer_->stop();
        db_->setEdits(editingRecord_.filePath, editingRecord_.edits);
    }
    if (previewCoalesceTimer_) {
        previewCoalesceTimer_->stop();
    }
    dragPreviewActive_ = false;
    editingIsAnimated_ = false;

    stopEditorMovie();
    const uint64_t seq = ++editorLoadSeq_;
    ++thumbJobSeq_;

    editingRecord_ = rec;
    undoStack_.clear();
    updateUndoButton();
    compareOriginal_ = false;
    {
        const QSignalBlocker b(editorCompareBtn_);
        editorCompareBtn_->setChecked(false);
    }
    eyedropperActive_ = false;
    eyedropperBtn_->setChecked(false);
    stripMetadataOnExport_ = false;
    {
        const QSignalBlocker sbMeta(stripMetaBtn_);
        const bool dark = AppSettings::instance().effectiveDarkTheme();
        stripMetaBtn_->setChecked(false);
        stripMetaBtn_->setIcon(makeUiIcon(UiIconKind::Trash, dark ? QColor(0xFD, 0xA4, 0xAF) : QColor(0xBE, 0x12, 0x3C), 15));
        stripMetaBtn_->setText(trUi(QStringLiteral("Удалить метаданные"), QStringLiteral("Strip Metadata")));
    }

    editingPreviewLayerImage_ = QImage();
    editingPsdLayers_.clear();
    cropBarWidget_->setVisible(false);
    if (cropCanvasBar_) cropCanvasBar_->setVisible(false);
    if (basicAdjustWidget_) {
        basicAdjustWidget_->setVisible(!qFuzzyIsNull(rec.edits.straightenDeg));
    }
    editorCanvas_->setCropMode(false, rec.edits.cropLeft, rec.edits.cropTop, rec.edits.cropRight, rec.edits.cropBottom);
    editorCanvas_->setLiveTextLayer(nullptr);
    editorCanvas_->setEyedropperMode(false);
    editorZoomBtn_->setText(QStringLiteral("1:1"));
    updateBackdropButton();

    selectedLayerId_ = rec.edits.effectLayers.isEmpty() ? QString() : rec.edits.effectLayers.last().id;

    {
        const QSignalBlocker sb(editorStarBtn_);
        editorStarBtn_->setChecked(rec.starred);
        editorStarBtn_->setIcon(makeUnifiedStarIcon(rec.starred, 16));
        editorStarBtn_->setText(rec.starred
                                    ? trUi(QStringLiteral("В избранном"), QStringLiteral("Favorited"))
                                    : trUi(QStringLiteral("Избранное"), QStringLiteral("Favorite")));
    }

    editorCaptionEdit_->setText(rec.caption);
    syncEditorSliders();
    populateEditorFilmstrip();
    populateEffectStackList();
    updateActiveEffectControls();

    mainStack_->setCurrentIndex(1);
    updateEditorPaneConstraints();
    if (edBody_ && !editorInitialSplitDone_) {
        editorInitialSplitDone_ = true;
        const int totalW = std::max(900, width());
        const int leftW = std::clamp(350, std::max(96, qRound(totalW * 0.08)), std::max(140, qRound(totalW * 0.40)));
        const int rightW = 300;
        const int centerW = std::max(320, totalW - leftW - rightW);
        edBody_->setSizes({leftW, centerW, rightW});
    }

    if (editorCache_.contains(rec.filePath)) {
        editorCacheOrder_.removeAll(rec.filePath);
        editorCacheOrder_.append(rec.filePath);
        const CachedEditorAsset cached = editorCache_.value(rec.filePath);
        applyLoadedEditorAsset(rec, cached, true);
        preloadNeighborInEditor(1);
        preloadNeighborInEditor(-1);
        setFocus();
        return;
    }

    // Immediate non-blocking UI preview while decoding full image & metadata off the main thread
    const QImage quickThumb = thumbImageCache_.value(rec.filePath);
    editingRawImage_ = quickThumb;
    rebuildEditorPreviewCaches();
    editingLoadedMeta_ = ExifMetadata{};
    refreshMetadataInspector();
    populateEditorPsdLayers();
    if (!quickThumb.isNull()) {
        refreshEditorPreview(true);
    } else {
        editorCanvas_->setDisplayImage(QImage(), true);
    }
    editorMetaStatus_->setText(
        QString("%1  ·  %2")
            .arg(rec.fileName)
            .arg(trUi(QStringLiteral("Загрузка..."), QStringLiteral("Loading..."))));
    QTimer::singleShot(0, this, [this]() {
        if (edLeftPane_) adaptLeftPaneLayout(edLeftPane_->width());
    });
    setFocus();

    const QString targetPath = rec.filePath;
    auto* watcher = new QFutureWatcher<CachedEditorAsset>(this);
    connect(watcher, &QFutureWatcher<CachedEditorAsset>::finished, this, [this, watcher, seq, targetPath]() {
        const CachedEditorAsset asset = watcher->result();
        watcher->deleteLater();
        insertEditorCache(targetPath, asset);
        if (seq != editorLoadSeq_ || mainStack_->currentIndex() != 1 || editingRecord_.filePath != targetPath) {
            return;
        }
        if (asset.raw.isNull()) {
            editorMetaStatus_->setText(
                QString("%1  ·  %2")
                    .arg(editingRecord_.fileName)
                    .arg(trUi(QStringLiteral("Ошибка загрузки"), QStringLiteral("Load error"))));
            return;
        }
        applyLoadedEditorAsset(editingRecord_, asset, true);
        preloadNeighborInEditor(1);
        preloadNeighborInEditor(-1);
    });
    watcher->setFuture(QtConcurrent::run([targetPath]() {
        CachedEditorAsset asset;
        const LoadedImageResult loaded = ImageLoader::loadImage(targetPath, 0);
        if (!loaded.success || loaded.image.isNull()) return asset;
        asset.raw = loaded.image;
        const int maxDim = std::max(asset.raw.width(), asset.raw.height());
        asset.screen = (maxDim > 1600)
                           ? asset.raw.scaled(1600, 1600, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                           : asset.raw;
        asset.drag = (maxDim > 760)
                         ? asset.raw.scaled(760, 760, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                         : asset.raw;
        asset.meta = loaded.metadata;
        asset.psdLayers = loaded.metadata.psdLayerDetails;
        asset.isAnimated = loaded.isAnimated;
        return asset;
    }));
}

void MainWindow::openRelativePhotoInEditor(int delta) {
    if (currentPhotos_.size() <= 1) return;
    int idx = -1;
    for (int i = 0; i < currentPhotos_.size(); ++i) {
        if (currentPhotos_[i].filePath == editingRecord_.filePath) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        idx = std::max(0, editorFilmstrip_->currentRow());
    }
    const int n = currentPhotos_.size();
    const int next = (idx + delta + n) % n;
    onOpenEditorForRecord(currentPhotos_[next]);
}

void MainWindow::setCropEditingEnabled(bool enabled, bool resetViewAfterConfirm) {
    if (enabled) {
        eyedropperActive_ = false;
        eyedropperBtn_->setChecked(false);
        editingRecord_.edits.cropEnabled = true;
    }
    editorCanvas_->setCropMode(enabled, editingRecord_.edits.cropLeft, editingRecord_.edits.cropTop,
                               editingRecord_.edits.cropRight, editingRecord_.edits.cropBottom);
    cropBarWidget_->setVisible(enabled);
    if (cropCanvasBar_) {
        cropCanvasBar_->setVisible(enabled);
    }
    if (enabled) {
        updateCropReadout();
    }
    updateLeftTabsHeight();
    refreshEditorPreview(!enabled && resetViewAfterConfirm);
}

void MainWindow::updateCropReadout() {
    if (!cropCanvasInfoLabel_) return;
    const double cw = std::clamp(editingRecord_.edits.cropRight - editingRecord_.edits.cropLeft, 0.01, 1.0);
    const double ch = std::clamp(editingRecord_.edits.cropBottom - editingRecord_.edits.cropTop, 0.01, 1.0);
    const int wPct = qRound(cw * 100.0);
    const int hPct = qRound(ch * 100.0);
    if (!editingRawImage_.isNull()) {
        const int pxW = std::max(1, qRound(editingRawImage_.width() * cw));
        const int pxH = std::max(1, qRound(editingRawImage_.height() * ch));
        cropCanvasInfoLabel_->setText(QString("%1 × %2 px (%3% × %4%)").arg(pxW).arg(pxH).arg(wPct).arg(hPct));
    } else {
        cropCanvasInfoLabel_->setText(QString("%1% × %2%").arg(wPct).arg(hPct));
    }
}

void MainWindow::updateBackdropButton() {
    if (!edBackdropBtn_) return;
    const CanvasBackdropMode m = editorCanvas_->backdropMode();
    if (m == CanvasBackdropMode::White) {
        edBackdropBtn_->setChecked(true);
        edBackdropBtn_->setText(trUi(QStringLiteral("Фон: Белый"), QStringLiteral("Backdrop: White")));
    } else if (m == CanvasBackdropMode::Black) {
        edBackdropBtn_->setChecked(true);
        edBackdropBtn_->setText(trUi(QStringLiteral("Фон: Чёрный"), QStringLiteral("Backdrop: Black")));
    } else {
        edBackdropBtn_->setChecked(false);
        edBackdropBtn_->setText(trUi(QStringLiteral("Фон: Выкл"), QStringLiteral("Backdrop: Off")));
    }
}

void MainWindow::cycleBackdropMode() {
    const CanvasBackdropMode cur = editorCanvas_->backdropMode();
    CanvasBackdropMode next = CanvasBackdropMode::White;
    if (cur == CanvasBackdropMode::White) {
        next = CanvasBackdropMode::Black;
    } else if (cur == CanvasBackdropMode::Black) {
        next = CanvasBackdropMode::None;
    }
    editorCanvas_->setBackdropMode(next);
    updateBackdropButton();
}

void MainWindow::refreshMetadataInspector() {
    const ExifMetadata& m = editingLoadedMeta_;
    const QString camFallback = trUi(QStringLiteral("Нет данных EXIF"), QStringLiteral("No EXIF data"));
    editorExifLabel_->setText(
        trUi(QString("Камера: %1\nВыдержка: %2  ·  %3  ·  ISO %4"),
             QString("Camera: %1\nShutter: %2  ·  %3  ·  ISO %4"))
            .arg(m.camera.isEmpty() ? camFallback : m.camera)
            .arg(m.shutterSpeed.isEmpty() ? QStringLiteral("—") : m.shutterSpeed)
            .arg(m.aperture.isEmpty() ? QStringLiteral("—") : m.aperture)
            .arg(m.iso > 0 ? QString::number(m.iso) : QStringLiteral("—"))
    );

    const QString extra = m.rawMetadataText.trimmed();
    constexpr int kMaxDisplayChars = 4500;
    QString displayMeta = extra;
    if (extra.size() > kMaxDisplayChars) {
        displayMeta = extra.left(kMaxDisplayChars) +
                      trUi(QStringLiteral("\n\n… (сокращено для ускорения; «Копировать» скопирует полностью)"),
                           QStringLiteral("\n\n… (truncated for speed; Copy copies full metadata)"));
    }
    metadataTextEdit_->setPlainText(displayMeta);
    const bool hasAnyMeta = !extra.isEmpty() || !m.camera.isEmpty() || !m.software.isEmpty() ||
                            !m.shutterSpeed.isEmpty() || !m.aperture.isEmpty() || m.iso > 0;
    copyMetaBtn_->setEnabled(hasAnyMeta);
    stripMetaBtn_->setEnabled(true);
}

void MainWindow::copyCurrentMetadata() {
    QStringList blocks;
    if (!editingLoadedMeta_.camera.isEmpty())
        blocks << trUi(QStringLiteral("Камера: "), QStringLiteral("Camera: ")) + editingLoadedMeta_.camera;
    if (!editingLoadedMeta_.lens.isEmpty())
        blocks << trUi(QStringLiteral("Объектив: "), QStringLiteral("Lens: ")) + editingLoadedMeta_.lens;
    if (!editingLoadedMeta_.focalLength.isEmpty())
        blocks << trUi(QStringLiteral("Фокусное: "), QStringLiteral("Focal length: ")) + editingLoadedMeta_.focalLength;
    if (!editingLoadedMeta_.aperture.isEmpty())
        blocks << trUi(QStringLiteral("Диафрагма: "), QStringLiteral("Aperture: ")) + editingLoadedMeta_.aperture;
    if (!editingLoadedMeta_.shutterSpeed.isEmpty())
        blocks << trUi(QStringLiteral("Выдержка: "), QStringLiteral("Shutter: ")) + editingLoadedMeta_.shutterSpeed;
    if (editingLoadedMeta_.iso > 0)
        blocks << QString("ISO: %1").arg(editingLoadedMeta_.iso);
    if (!editingLoadedMeta_.rawMetadataText.trimmed().isEmpty()) {
        blocks << editingLoadedMeta_.rawMetadataText.trimmed();
    }

    if (blocks.isEmpty()) return;
    QApplication::clipboard()->setText(blocks.join(QStringLiteral("\n")));
    copyMetaBtn_->setText(trUi(QStringLiteral("Скопировано ✓"), QStringLiteral("Copied ✓")));
    QTimer::singleShot(1800, copyMetaBtn_, [this]() {
        if (copyMetaBtn_) copyMetaBtn_->setText(trUi(QStringLiteral("Копировать"), QStringLiteral("Copy")));
    });
}

void MainWindow::updateEditorPaneConstraints() {
    if (!edLeftPane_ || !edRightPane_) return;
    const int winW = std::max(800, width());
    const int minLeft = std::max(96, qRound(winW * 0.08));
    const int maxLeft = std::max(minLeft + 60, qRound(winW * 0.40));
    edLeftPane_->setMinimumWidth(minLeft);
    edLeftPane_->setMaximumWidth(maxLeft);
    edRightPane_->setMinimumWidth(220);
    edRightPane_->setMaximumWidth(460);
}

void MainWindow::adaptLeftPaneLayout(int paneW) {
    if (!basicTilesList_ || !processEffectsList_ || !artEffectsList_ || !frameEffectsList_) return;

    int availW = std::max(68, paneW - 18);
    if (edTabs_) {
        if (auto* sa = qobject_cast<QScrollArea*>(edTabs_->currentWidget())) {
            if (sa->viewport() && sa->viewport()->width() > 60) {
                availW = std::max(68, sa->viewport()->width() - 16);
            }
        }
    }
    for (QListWidget* g : {basicTilesList_, processEffectsList_, artEffectsList_, frameEffectsList_}) {
        if (g && g->isVisible() && g->viewport() && g->viewport()->width() > 60) {
            availW = g->viewport()->width();
            break;
        }
    }

    int cols = 3;
    if (availW < 135) {
        cols = 1;
    } else if (availW < 210) {
        cols = 2;
    } else if (availW < 380) {
        cols = 3;
    } else {
        cols = 4;
    }

    const int cellW = std::max(58, (availW - 6) / cols);
    const int iconW = std::clamp(cellW - 12, 42, 136);
    const int iconH = std::max(30, qRound(iconW * 0.62));
    const int cellH = iconH + 44;

    auto applyToGrid = [&](QListWidget* grid) {
        grid->setIconSize(QSize(iconW, iconH));
        grid->setGridSize(QSize(cellW, cellH));
        const int cnt = std::max(1, grid->count());
        const int rows = (cnt + cols - 1) / cols;
        grid->setFixedHeight(rows * cellH + 6);
    };
    applyToGrid(basicTilesList_);
    applyToGrid(processEffectsList_);
    applyToGrid(artEffectsList_);
    applyToGrid(frameEffectsList_);

    updateLeftTabsHeight();
}

void MainWindow::updateLeftTabsHeight() {
    if (!edTabs_ || !edLeftPane_) return;
    QWidget* curPage = edTabs_->currentWidget();
    if (!curPage) return;

    int contentH = 0;
    if (auto* sa = qobject_cast<QScrollArea*>(curPage)) {
        if (QWidget* inner = sa->widget()) {
            if (inner->layout()) inner->layout()->activate();
            contentH = inner->sizeHint().height();
        }
    } else {
        contentH = curPage->sizeHint().height();
    }

    const int tabBarH = edTabs_->tabBar() ? edTabs_->tabBar()->sizeHint().height() : 36;
    const int desiredTabsH = tabBarH + contentH + 6;
    const int paneH = (edLeftPane_->height() > 160)
                          ? edLeftPane_->height()
                          : std::max(460, height() - 110);
    const int effBoxH = (activeEffectBox_ && activeEffectBox_->isVisible())
                            ? activeEffectBox_->sizeHint().height()
                            : 0;
    const int maxTabsH = std::max(170, paneH - effBoxH);
    edTabs_->setFixedHeight(std::min(desiredTabsH, maxTabsH));
}

void MainWindow::updateActiveLayerLabelInStack() {
    const int idx = selectedEffectLayerIndex();
    if (idx < 0 || !effectStackList_) return;
    const EffectLayer& l = editingRecord_.edits.effectLayers[idx];
    const QString title = (l.preset == EffectPreset::TextOverlay)
                              ? trUi(QString("Текст: «%1»").arg(l.text.left(18)),
                                     QString("Text: \"%1\"").arg(l.text.left(18)))
                              : QString("%1 · %2%").arg(ImageLoader::effectPresetName(l.preset)).arg(l.intensity);
    for (int i = 0; i < effectStackList_->count(); ++i) {
        QListWidgetItem* item = effectStackList_->item(i);
        if (item && item->data(Qt::UserRole).toString() == l.id) {
            const QSignalBlocker b(effectStackList_);
            item->setText(title);
            break;
        }
    }
}

void MainWindow::populateEditorPsdLayers() {
    const QSignalBlocker blocker(editorPsdLayers_);
    editorPsdLayers_->clear();
    editorExportPsdLayerBtn_->setEnabled(false);

    const bool showLayers = !editingPsdLayers_.isEmpty();
    editorLayersTitle_->setVisible(showLayers);
    editorPsdLayers_->setVisible(showLayers);
    editorExportPsdLayerBtn_->setVisible(showLayers);
    if (!showLayers) return;

    editorPsdLayers_->addItem(new QListWidgetItem(
        QIcon(QPixmap::fromImage(editingRawImage_.scaled(34, 34, Qt::KeepAspectRatio, Qt::SmoothTransformation))),
        trUi(QStringLiteral("Объединённый вид (Композит)"), QStringLiteral("Merged View (Composite)"))
    ));

    const bool dark = AppSettings::instance().effectiveDarkTheme();
    for (const PsdLayerInfo& layer : editingPsdLayers_) {
        QPixmap thumb(34, 34);
        thumb.fill(dark ? QColor(42, 46, 58) : QColor(235, 238, 242));
        if (!layer.image.isNull()) {
            QPixmap s = QPixmap::fromImage(layer.image.scaled(34, 34, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            QPainter p(&thumb);
            p.drawPixmap((34 - s.width()) / 2, (34 - s.height()) / 2, s);
        }
        const QString dim = layer.image.isNull()
                                ? trUi(QStringLiteral(" (группа/текст)"), QStringLiteral(" (group/text)"))
                                : QString(" (%1×%2)").arg(layer.image.width()).arg(layer.image.height());
        editorPsdLayers_->addItem(new QListWidgetItem(QIcon(thumb), layer.name + dim));
    }
    editorPsdLayers_->setCurrentRow(0);
}

void MainWindow::exportSelectedEditorPsdLayer() {
    if (editingPreviewLayerImage_.isNull()) return;
    const int row = editorPsdLayers_->currentRow();
    const QString layerName = (row > 0 && row - 1 < editingPsdLayers_.size())
                                  ? editingPsdLayers_[row - 1].name
                                  : QStringLiteral("layer");
    const QFileInfo srcFi(editingRecord_.filePath);
    const QString defaultPath = srcFi.absolutePath() + "/" + srcFi.completeBaseName() + "_" + layerName + ".png";
    QString outPath = QFileDialog::getSaveFileName(
        this,
        trUi(QStringLiteral("Экспорт слоя PSD"), QStringLiteral("Export PSD Layer")),
        defaultPath,
        trUi(QStringLiteral("PNG с прозрачностью (*.png)"), QStringLiteral("PNG with transparency (*.png)"))
    );
    if (outPath.isEmpty()) return;
    if (QFileInfo(outPath).suffix().isEmpty()) {
        outPath += QStringLiteral(".png");
    }

    QImageWriter writer(outPath, "PNG");
    if (writer.write(editingPreviewLayerImage_)) {
        editorExportPsdLayerBtn_->setText(trUi(QStringLiteral("Сохранено ✓"), QStringLiteral("Saved ✓")));
        QTimer::singleShot(1800, editorExportPsdLayerBtn_, [this]() {
            if (editorExportPsdLayerBtn_) {
                editorExportPsdLayerBtn_->setText(trUi(QStringLiteral("Экспортировать слой PNG"),
                                                       QStringLiteral("Export Layer PNG")));
            }
        });
    } else {
        QMessageBox::warning(this,
                             trUi(QStringLiteral("Ошибка"), QStringLiteral("Error")),
                             trUi(QStringLiteral("Не удалось сохранить слой:\n"),
                                  QStringLiteral("Failed to save layer:\n")) + writer.errorString());
    }
}

void MainWindow::addOrSelectEffectLayer(EffectPreset preset, int defaultIntensity) {
    if (preset == EffectPreset::None) return;
    pushUndoState();
    EffectLayer layer = EffectLayer::create(preset, defaultIntensity, 50);
    editingRecord_.edits.effectLayers.append(layer);
    selectedLayerId_ = layer.id;
    populateEffectStackList();
    updateActiveEffectControls();
    saveEditorEdits();
}

void MainWindow::addNewTextLayer() {
    int existingCount = 0;
    for (const EffectLayer& l : editingRecord_.edits.effectLayers) {
        if (l.preset == EffectPreset::TextOverlay) ++existingCount;
    }

    bool ok = false;
    const QString txt = QInputDialog::getText(
        this,
        trUi(QStringLiteral("Новый текстовый слой"), QStringLiteral("New Text Layer")),
        trUi(QStringLiteral("Введите текст (можно добавлять сколько угодно надписей и перетаскивать каждую мышью):"),
             QStringLiteral("Enter text (you can add multiple text layers and drag each with the mouse):")),
        QLineEdit::Normal,
        QString(),
        &ok);
    if (!ok || txt.trimmed().isEmpty()) return;

    pushUndoState();
    const double startY = std::clamp(0.32 + (existingCount % 4) * 0.16, 0.16, 0.84);
    EffectLayer layer = EffectLayer::createText(txt.trimmed(), 0.5, startY, 42, QStringLiteral("#FFFFFF"));
    editingRecord_.edits.effectLayers.append(layer);
    selectedLayerId_ = layer.id;
    populateEffectStackList();
    updateActiveEffectControls();
    saveEditorEdits();
}

int MainWindow::selectedEffectLayerIndex() const {
    if (selectedLayerId_.isEmpty()) return -1;
    for (int i = 0; i < editingRecord_.edits.effectLayers.size(); ++i) {
        if (editingRecord_.edits.effectLayers[i].id == selectedLayerId_) return i;
    }
    return -1;
}

void MainWindow::selectEffectLayerById(const QString& layerId) {
    selectedLayerId_ = layerId;
    const int idx = selectedEffectLayerIndex();
    if (idx >= 0) {
        const EffectLayer& l = editingRecord_.edits.effectLayers[idx];
        const int cat = ImageLoader::effectCategoryIndex(l.preset);
        if (cat >= 0) {
            edTabs_->setCurrentIndex(2 + cat);
            QListWidget* grid = (cat == 0) ? processEffectsList_ : (cat == 1 ? artEffectsList_ : frameEffectsList_);
            const QList<EffectPreset> presets = ImageLoader::effectsForCategory(cat);
            const int pIdx = presets.indexOf(l.preset);
            if (grid && pIdx >= 0) {
                const QSignalBlocker b(grid);
                grid->setCurrentRow(pIdx);
            }
        } else if (l.preset == EffectPreset::AutoBalance) {
            edTabs_->setCurrentIndex(1);
        } else {
            edTabs_->setCurrentIndex(0);
        }
    }
    updateActiveEffectControls();
    populateEffectStackList();
    if (editorCanvas_->isCropMode()) {
        setCropEditingEnabled(false, false);
    } else {
        refreshEditorPreview(false);
    }
}

void MainWindow::populateEffectStackList() {
    const QSignalBlocker blocker(effectStackList_);
    effectStackList_->clear();

    int selectedRow = -1;
    for (int i = editingRecord_.edits.effectLayers.size() - 1; i >= 0; --i) {
        const EffectLayer& l = editingRecord_.edits.effectLayers[i];
        QString title;
        if (l.preset == EffectPreset::TextOverlay) {
            title = trUi(QString("Текст: «%1»").arg(l.text.left(18)),
                         QString("Text: \"%1\"").arg(l.text.left(18)));
        } else {
            title = QString("%1 · %2%").arg(ImageLoader::effectPresetName(l.preset)).arg(l.intensity);
        }
        auto* item = new QListWidgetItem(title);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(l.enabled ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, l.id);
        effectStackList_->addItem(item);
        if (l.id == selectedLayerId_) {
            selectedRow = effectStackList_->count() - 1;
        }
    }

    if (editingRecord_.edits.hasLightColorAdjustments()) {
        auto* lcItem = new QListWidgetItem(trUi(QStringLiteral("☀ Свет и цвет (ручная коррекция)"),
                                                QStringLiteral("☀ Light & Color (manual)")));
        lcItem->setFlags(lcItem->flags() | Qt::ItemIsUserCheckable);
        lcItem->setCheckState(editingRecord_.edits.lightColorEnabled ? Qt::Checked : Qt::Unchecked);
        lcItem->setData(Qt::UserRole, QStringLiteral("__lightcolor__"));
        effectStackList_->addItem(lcItem);
        if (selectedLayerId_ == QStringLiteral("__lightcolor__")) {
            selectedRow = effectStackList_->count() - 1;
        }
    }

    if (editingRecord_.edits.hasCrop()) {
        auto* cItem = new QListWidgetItem(trUi(QStringLiteral("✂ Кадрирование"),
                                               QStringLiteral("✂ Crop")));
        cItem->setFlags(cItem->flags() | Qt::ItemIsUserCheckable);
        cItem->setCheckState(editingRecord_.edits.cropEnabled ? Qt::Checked : Qt::Unchecked);
        cItem->setData(Qt::UserRole, QStringLiteral("__crop__"));
        effectStackList_->addItem(cItem);
        if (selectedLayerId_ == QStringLiteral("__crop__")) {
            selectedRow = effectStackList_->count() - 1;
        }
    }

    if (editingRecord_.edits.hasBasicLayerAdjustments()) {
        auto* bItem = new QListWidgetItem(trUi(QStringLiteral("⚙ Выравнивание горизонта"),
                                               QStringLiteral("⚙ Straighten Horizon")));
        bItem->setFlags(bItem->flags() | Qt::ItemIsUserCheckable);
        bItem->setCheckState(editingRecord_.edits.basicEnabled ? Qt::Checked : Qt::Unchecked);
        bItem->setData(Qt::UserRole, QStringLiteral("__basic__"));
        effectStackList_->addItem(bItem);
        if (selectedLayerId_ == QStringLiteral("__basic__")) {
            selectedRow = effectStackList_->count() - 1;
        }
    }

    auto* baseItem = new QListWidgetItem(trUi(QStringLiteral("Исходное изображение (фон)"),
                                              QStringLiteral("Original Image (background)")));
    baseItem->setData(Qt::UserRole, QString());
    effectStackList_->addItem(baseItem);

    if (selectedRow >= 0) {
        effectStackList_->setCurrentRow(selectedRow);
    } else {
        effectStackList_->setCurrentRow(effectStackList_->count() - 1);
    }

    const int idx = selectedEffectLayerIndex();
    layerUpBtn_->setEnabled(idx >= 0 && idx + 1 < editingRecord_.edits.effectLayers.size());
    layerDownBtn_->setEnabled(idx > 0);
    layerDeleteBtn_->setEnabled(idx >= 0 ||
                                selectedLayerId_ == QStringLiteral("__lightcolor__") ||
                                selectedLayerId_ == QStringLiteral("__crop__") ||
                                selectedLayerId_ == QStringLiteral("__basic__"));
}

void MainWindow::updateActiveEffectControls() {
    const int idx = selectedEffectLayerIndex();
    if (idx < 0) {
        activeEffectBox_->setVisible(false);
        editorCanvas_->setLiveTextLayer(nullptr);
        updateLeftTabsHeight();
        return;
    }

    const EffectLayer& l = editingRecord_.edits.effectLayers[idx];
    activeEffectBox_->setVisible(true);
    activeEffectTitle_->setText(trUi(QStringLiteral("Слой: "), QStringLiteral("Layer: ")) +
                                ImageLoader::effectPresetName(l.preset));
    effectIntensityRow_->setValueSilently(l.intensity);

    const bool isText = (l.preset == EffectPreset::TextOverlay);
    textControlsWidget_->setVisible(isText);
    if (isText) {
        {
            const QSignalBlocker b(textLayerEdit_);
            textLayerEdit_->setText(l.text);
        }
        textLayerSizeRow_->setValueSilently(l.textSize);
        textColorBtn_->setProperty("swatchColor", l.textColor);
        textColorBtn_->setStyleSheet(QString());
        textColorBtn_->update();
        effectIntensityRow_->setTitle(trUi(QStringLiteral("Непрозрачность текста"), QStringLiteral("Text Opacity")));
        effectParam2Row_->setVisible(false);
        editorCanvas_->setLiveTextLayer(l.enabled ? &l : nullptr);
    } else {
        editorCanvas_->setLiveTextLayer(nullptr);
        effectIntensityRow_->setTitle(trUi(QStringLiteral("Сила эффекта"), QStringLiteral("Effect Strength")));
        const bool showP2 = effectUsesParam2(l.preset);
        effectParam2Row_->setVisible(showP2);
        if (showP2) {
            effectParam2Row_->setTitle(effectParam2Label(l.preset));
            effectParam2Row_->setValueSilently(l.param2);
        }
    }
    updateLeftTabsHeight();
}

void MainWindow::pushUndoState() {
    if (!undoStack_.isEmpty() && editsEqual(undoStack_.last(), editingRecord_.edits)) {
        return;
    }
    undoStack_.append(editingRecord_.edits);
    if (undoStack_.size() > 40) {
        undoStack_.removeFirst();
    }
    updateUndoButton();
}

void MainWindow::undoLastEdit() {
    if (undoStack_.isEmpty()) return;
    editingRecord_.edits = undoStack_.takeLast();
    updateUndoButton();
    if (selectedEffectLayerIndex() < 0) {
        selectedLayerId_ = editingRecord_.edits.effectLayers.isEmpty()
                               ? QString()
                               : editingRecord_.edits.effectLayers.last().id;
    }
    if (editorCanvas_->isCropMode()) {
        editorCanvas_->setCropMode(true, editingRecord_.edits.cropLeft, editingRecord_.edits.cropTop,
                                   editingRecord_.edits.cropRight, editingRecord_.edits.cropBottom);
        updateCropReadout();
    }
    syncEditorSliders();
    populateEffectStackList();
    updateActiveEffectControls();
    saveEditorEdits();
}

void MainWindow::updateUndoButton() {
    if (undoBtn_) {
        undoBtn_->setEnabled(!undoStack_.isEmpty());
    }
}

void MainWindow::syncEditorSliders() {
    straightenRow_->setValueSilently(qRound(editingRecord_.edits.straightenDeg));

    brightnessRow_->setValueSilently(editingRecord_.edits.brightness);
    highlightsRow_->setValueSilently(editingRecord_.edits.highlights);
    shadowsRow_->setValueSilently(editingRecord_.edits.shadows);
    contrastRow_->setValueSilently(editingRecord_.edits.contrast);
    saturationRow_->setValueSilently(editingRecord_.edits.saturation);
    tempRow_->setValueSilently(editingRecord_.edits.temperature);
    tintRow_->setValueSilently(editingRecord_.edits.tint);
    hueShiftRow_->setValueSilently(editingRecord_.edits.hueShift);
}

void MainWindow::populateEditorThumbnails() {
    basicTilesList_->clear();
    processEffectsList_->clear();
    artEffectsList_->clear();
    frameEffectsList_->clear();
    if (editingRawImage_.isNull()) return;

    const QImage scaledMini = editingRawImage_.scaled(96, 66, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const QImage mini = scaledMini.copy(
        std::max(0, (scaledMini.width() - 96) / 2),
        std::max(0, (scaledMini.height() - 66) / 2),
        std::min(96, scaledMini.width()),
        std::min(66, scaledMini.height())
    );

    struct TileSpec {
        QString label;
        PhotoEdits edits;
    };

    auto makeEd = [](auto fn) {
        PhotoEdits e;
        fn(e);
        return e;
    };

    const QList<TileSpec> basicSpecs = {
        {trUi(QStringLiteral("Кадрирование"), QStringLiteral("Crop")),
         makeEd([](PhotoEdits& e) { e.cropLeft = 0.14; e.cropTop = 0.14; e.cropRight = 0.86; e.cropBottom = 0.86; })},
        {trUi(QStringLiteral("Выравнивание"), QStringLiteral("Straighten")),
         makeEd([](PhotoEdits& e) { e.straightenDeg = 4.0; })},
        {trUi(QStringLiteral("Мне повезёт!"), QStringLiteral("I'm Feeling Lucky")),
         makeEd([](PhotoEdits& e) { e.effectLayers.append(EffectLayer::create(EffectPreset::AutoLucky, 90)); })},
        {trUi(QStringLiteral("Автоконтраст"), QStringLiteral("Auto Contrast")),
         makeEd([](PhotoEdits& e) { e.effectLayers.append(EffectLayer::create(EffectPreset::AutoContrast, 90)); })},
        {trUi(QStringLiteral("Автоцвет"), QStringLiteral("Auto Color")),
         makeEd([](PhotoEdits& e) { e.effectLayers.append(EffectLayer::create(EffectPreset::AutoColor, 90)); })},
        {trUi(QStringLiteral("Поворот 90°"), QStringLiteral("Rotate 90°")),
         makeEd([](PhotoEdits& e) { e.rotationDeg = 90; })},
        {trUi(QStringLiteral("Текст"), QStringLiteral("Text")),
         makeEd([](PhotoEdits& e) { e.effectLayers.append(EffectLayer::createText(QStringLiteral("ABC"), 0.5, 0.5, 85)); })},
        {trUi(QStringLiteral("Оригинал"), QStringLiteral("Original")), PhotoEdits{}},
    };

    for (const TileSpec& spec : basicSpecs) {
        const QImage preview = ImageLoader::applyEdits(mini, spec.edits);
        auto* item = new QListWidgetItem(QIcon(QPixmap::fromImage(preview)), spec.label);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        basicTilesList_->addItem(item);
    }

    const QIcon baseIcon(QPixmap::fromImage(mini));
    auto fillPlaceholders = [&](QListWidget* grid, int categoryIdx) {
        const QSignalBlocker blocker(grid);
        for (EffectPreset preset : ImageLoader::effectsForCategory(categoryIdx)) {
            auto* item = new QListWidgetItem(baseIcon, ImageLoader::effectPresetName(preset));
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
            grid->addItem(item);
        }
    };
    fillPlaceholders(processEffectsList_, 0);
    fillPlaceholders(artEffectsList_, 1);
    fillPlaceholders(frameEffectsList_, 2);

    populateEffectTabsAsync(++thumbJobSeq_, mini);
}

void MainWindow::populateEffectTabsAsync(uint64_t seq, const QImage& mini) {
    struct EffectThumbResult {
        int category = 0;
        int index = 0;
        QImage img;
    };

    auto* watcher = new QFutureWatcher<QList<EffectThumbResult>>(this);
    connect(watcher, &QFutureWatcher<QList<EffectThumbResult>>::finished, this, [this, watcher, seq]() {
        const QList<EffectThumbResult> results = watcher->result();
        watcher->deleteLater();
        if (seq != thumbJobSeq_) return;

        for (const EffectThumbResult& r : results) {
            QListWidget* grid = (r.category == 0) ? processEffectsList_
                                                  : (r.category == 1 ? artEffectsList_ : frameEffectsList_);
            if (grid && r.index >= 0 && r.index < grid->count() && !r.img.isNull()) {
                if (QListWidgetItem* item = grid->item(r.index)) {
                    item->setIcon(QIcon(QPixmap::fromImage(r.img)));
                }
            }
        }
    });

    watcher->setFuture(QtConcurrent::run([mini]() {
        QList<EffectThumbResult> out;
        for (int cat = 0; cat < 3; ++cat) {
            const QList<EffectPreset> presets = ImageLoader::effectsForCategory(cat);
            for (int i = 0; i < presets.size(); ++i) {
                PhotoEdits e;
                e.effectLayers.append(EffectLayer::create(presets[i], 85, 50));
                out.append({cat, i, ImageLoader::applyEdits(mini, e)});
            }
        }
        return out;
    }));
}

void MainWindow::populateEditorFilmstrip() {
    const QSignalBlocker blocker(editorFilmstrip_);
    editorFilmstrip_->clear();

    QPixmap blank(44, 32);
    blank.fill(QColor(0xD4, 0xD4, 0xD8));
    const QIcon blankIcon(blank);

    int activeRow = -1;
    for (int i = 0; i < currentPhotos_.size(); ++i) {
        const PhotoRecord& rec = currentPhotos_[i];
        auto* item = new QListWidgetItem();
        item->setToolTip(rec.fileName);
        item->setData(Qt::UserRole, rec.filePath);
        if (thumbImageCache_.contains(rec.filePath)) {
            const QPixmap pm = QPixmap::fromImage(thumbImageCache_.value(rec.filePath).scaled(44, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            item->setData(Qt::UserRole + 5, pm);
            item->setIcon(QIcon(pm));
        } else {
            item->setData(Qt::UserRole + 5, blank);
            item->setIcon(blankIcon);
            enqueueThumbnail(rec.filePath, false);
        }
        editorFilmstrip_->addItem(item);
        if (rec.filePath == editingRecord_.filePath) {
            activeRow = i;
        }
    }
    if (activeRow >= 0) {
        editorFilmstrip_->setCurrentRow(activeRow);
        editorFilmstrip_->scrollToItem(editorFilmstrip_->item(activeRow));
    }
}

void MainWindow::rebuildEditorPreviewCaches() {
    if (editingRawImage_.isNull()) {
        editingScreenImage_ = QImage();
        editingDragImage_ = QImage();
        return;
    }
    const int maxDim = std::max(editingRawImage_.width(), editingRawImage_.height());
    if (maxDim > 1600) {
        editingScreenImage_ = editingRawImage_.scaled(1600, 1600, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        editingScreenImage_ = editingRawImage_;
    }
    if (maxDim > 760) {
        editingDragImage_ = editingRawImage_.scaled(760, 760, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        editingDragImage_ = editingRawImage_;
    }
}

void MainWindow::scheduleLivePreview() {
    if (previewCoalesceTimer_ && !previewCoalesceTimer_->isActive()) {
        previewCoalesceTimer_->start();
    }
}

void MainWindow::saveEditorEdits() {
    if (dbSaveTimer_) dbSaveTimer_->stop();
    if (!editingRecord_.filePath.isEmpty()) {
        db_->setEdits(editingRecord_.filePath, editingRecord_.edits);
    }
    dragPreviewActive_ = false;
    refreshEditorPreview(false);
}

void MainWindow::refreshEditorPreview(bool resetView) {
    if (!editingPreviewLayerImage_.isNull()) {
        editorCanvas_->setLiveTextLayer(nullptr);
        histogramWidget_->setImage(editingPreviewLayerImage_);
        editorCanvas_->setDisplayImage(editingPreviewLayerImage_, resetView);
        return;
    }
    if (editingRawImage_.isNull()) return;

    QString skipLayerId;
    const EffectLayer* liveLayerPtr = nullptr;
    if (!compareOriginal_ && !editorCanvas_->isCropMode()) {
        const int selIdx = selectedEffectLayerIndex();
        if (selIdx >= 0 &&
            editingRecord_.edits.effectLayers[selIdx].preset == EffectPreset::TextOverlay &&
            editingRecord_.edits.effectLayers[selIdx].enabled) {
            skipLayerId = selectedLayerId_;
            liveLayerPtr = &editingRecord_.edits.effectLayers[selIdx];
        }
    }

    const QImage* baseImg = &editingRawImage_;
    if (dragPreviewActive_ && !editingDragImage_.isNull() && !editorCanvas_->isActualSize()) {
        baseImg = &editingDragImage_;
    } else if (!editorCanvas_->isActualSize() && !editingScreenImage_.isNull()) {
        baseImg = &editingScreenImage_;
    }

    const PhotoEdits activeEdits = compareOriginal_ ? PhotoEdits{} : editingRecord_.edits;
    const QImage edited = ImageLoader::applyEdits(*baseImg, activeEdits, editorCanvas_->isCropMode(), skipLayerId);

    editorCanvas_->setLiveTextLayer(liveLayerPtr);
    if (!dragPreviewActive_) {
        histogramWidget_->setImage(edited);
    }
    editorCanvas_->setDisplayImage(edited, resetView);
}

void MainWindow::exportEditedPhoto() {
    if (editingRawImage_.isNull()) return;
    if (dbSaveTimer_ && dbSaveTimer_->isActive() && !editingRecord_.filePath.isEmpty()) {
        dbSaveTimer_->stop();
        db_->setEdits(editingRecord_.filePath, editingRecord_.edits);
    }

    const QFileInfo srcFi(editingRecord_.filePath);
    const QString srcExt = srcFi.suffix().toLower();
    QString defaultExt = QStringLiteral("jpg");
    if (editingIsAnimated_ || srcExt == QLatin1String("gif")) {
        defaultExt = QStringLiteral("gif");
    } else if (srcExt == QLatin1String("png") || editingRawImage_.hasAlphaChannel()) {
        defaultExt = QStringLiteral("png");
    } else if (srcExt == QLatin1String("webp")) {
        defaultExt = QStringLiteral("webp");
    }

    const QString fGif = editingIsAnimated_
                             ? trUi(QStringLiteral("GIF-анимация (*.gif)"), QStringLiteral("GIF Animation (*.gif)"))
                             : QStringLiteral("GIF (*.gif)");
    const QString fJpg = QStringLiteral("JPEG (*.jpg *.jpeg)");
    const QString fPng = QStringLiteral("PNG (*.png)");
    const QString fWebp = QStringLiteral("WebP (*.webp)");
    const QString fBmp = QStringLiteral("BMP (*.bmp)");
    const QString fTiff = QStringLiteral("TIFF (*.tif *.tiff)");
    const QString allFilters = QStringList({fGif, fJpg, fPng, fWebp, fBmp, fTiff}).join(QStringLiteral(";;"));

    QString selectedFilter = fJpg;
    if (defaultExt == QLatin1String("gif")) selectedFilter = fGif;
    else if (defaultExt == QLatin1String("png")) selectedFilter = fPng;
    else if (defaultExt == QLatin1String("webp")) selectedFilter = fWebp;

    const QString defaultPath = srcFi.absolutePath() + "/" + srcFi.completeBaseName() + "_edited." + defaultExt;
    QString outPath = QFileDialog::getSaveFileName(
        this,
        trUi(QStringLiteral("Экспорт обработанного снимка"), QStringLiteral("Export Edited Image")),
        defaultPath,
        allFilters,
        &selectedFilter
    );
    if (outPath.isEmpty()) return;

    QString ext = QFileInfo(outPath).suffix().toLower();
    if (ext.isEmpty()) {
        if (selectedFilter.contains(QStringLiteral("GIF"), Qt::CaseInsensitive)) ext = QStringLiteral("gif");
        else if (selectedFilter.contains(QStringLiteral("PNG"))) ext = QStringLiteral("png");
        else if (selectedFilter.contains(QStringLiteral("WebP"))) ext = QStringLiteral("webp");
        else if (selectedFilter.contains(QStringLiteral("BMP"))) ext = QStringLiteral("bmp");
        else if (selectedFilter.contains(QStringLiteral("TIFF"))) ext = QStringLiteral("tif");
        else ext = QStringLiteral("jpg");
        outPath += "." + ext;
    }

    const QImage finalImg = ImageLoader::applyEdits(editingRawImage_, editingRecord_.edits);
    if (finalImg.isNull()) {
        QMessageBox::warning(this,
                             trUi(QStringLiteral("Ошибка"), QStringLiteral("Error")),
                             trUi(QStringLiteral("Не удалось подготовить изображение для экспорта."),
                                  QStringLiteral("Failed to prepare image for export.")));
        return;
    }

    if (ext == QLatin1String("gif") && editingIsAnimated_) {
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
        const QString srcPath = editingRecord_.filePath;
        const PhotoEdits edits = editingRecord_.edits;
        auto* watcher = new QFutureWatcher<QString>(this);
        connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, safeDlg, cancelled]() {
            const QString err = watcher->result();
            watcher->deleteLater();
            if (safeDlg) {
                safeDlg->close();
                safeDlg->deleteLater();
            }
            if (cancelled->load()) return;
            if (err.isEmpty()) {
                if (exportBtn_) {
                    exportBtn_->setText(trUi(QStringLiteral("Сохранено ✓"), QStringLiteral("Saved ✓")));
                    QTimer::singleShot(2000, exportBtn_, [this]() {
                        if (exportBtn_) exportBtn_->setText(trUi(QStringLiteral("Экспорт"), QStringLiteral("Export")));
                    });
                }
            } else {
                QMessageBox::warning(this,
                                     trUi(QStringLiteral("Ошибка экспорта"), QStringLiteral("Export Error")),
                                     trUi(QStringLiteral("Не удалось сохранить файл:\n"),
                                          QStringLiteral("Failed to save file:\n")) + err);
            }
        });

        watcher->setFuture(QtConcurrent::run([srcPath, outPath, edits, finalImg, cancelled, safeDlg]() -> QString {
            QString err;
            ImageLoader::saveAnimatedGif(
                srcPath,
                outPath,
                edits,
                finalImg,
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
    if (ext == QStringLiteral("png")) fmt = "PNG";
    else if (ext == QStringLiteral("webp")) fmt = "WEBP";
    else if (ext == QStringLiteral("bmp")) fmt = "BMP";
    else if (ext == QStringLiteral("tif") || ext == QStringLiteral("tiff")) fmt = "TIFF";
    else if (ext == QStringLiteral("gif")) fmt = "GIF";

    QString err;
    if (ImageLoader::saveEditedImage(editingRecord_.filePath,
                                     outPath,
                                     fmt,
                                     finalImg,
                                     stripMetadataOnExport_,
                                     &editingLoadedMeta_,
                                     &err)) {
        if (exportBtn_) {
            exportBtn_->setText(trUi(QStringLiteral("Сохранено ✓"), QStringLiteral("Saved ✓")));
            QTimer::singleShot(2000, exportBtn_, [this]() {
                if (exportBtn_) exportBtn_->setText(trUi(QStringLiteral("Экспорт"), QStringLiteral("Export")));
            });
        }
    } else {
        QMessageBox::warning(this,
                             trUi(QStringLiteral("Ошибка экспорта"), QStringLiteral("Export Error")),
                             trUi(QStringLiteral("Не удалось сохранить файл:\n"),
                                  QStringLiteral("Failed to save file:\n")) + err);
    }
}

} // namespace lumina
