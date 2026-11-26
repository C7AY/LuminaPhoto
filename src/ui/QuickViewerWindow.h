#pragma once

#include "../core/LibraryDatabase.h"
#include "../core/PhotoModels.h"
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QMainWindow>
#include <QMovie>
#include <QPixmap>
#include <QPointF>
#include <QPushButton>
#include <QRectF>
#include <QStringList>

namespace lumina {

// Scrollbar-free canvas with cursor-anchored smooth wheel zoom, anytime drag panning, and double-click 1:1 toggle.
class ViewerCanvasWidget : public QWidget {
    Q_OBJECT
public:
    explicit ViewerCanvasWidget(QWidget* parent = nullptr);

    void setImage(const QImage& img, bool resetView = true);
    void setErrorText(const QString& text);
    void setFitToWindow(bool fit);
    void setActualSize();
    void zoomBy(double factor, const QPointF& anchorWidgetPos = QPointF());
    void setBackdropMode(CanvasBackdropMode mode);

    bool isFitToWindow() const { return fitToWindow_; }
    CanvasBackdropMode backdropMode() const { return backdropMode_; }
    double effectiveScale() const;

signals:
    void zoomChanged(double scale, bool fitMode);
    void backdropClicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    double computeFitScale() const;
    QRectF currentImageRect() const;
    void clampPan();
    void updateHoverCursor(const QPointF& mousePos);

    QPixmap pixmap_;
    bool hasAlpha_ = false;
    CanvasBackdropMode backdropMode_ = CanvasBackdropMode::None;
    QString errorText_;
    bool fitToWindow_ = true;
    double zoomFactor_ = 1.0;
    QPointF panOffset_{0.0, 0.0};

    bool dragging_ = false;
    bool didDrag_ = false;
    bool pressedInsideImage_ = false;
    QPoint dragStartPos_;
    QPointF panStartOffset_{0.0, 0.0};
};

// Modernized Picasa Photo Viewer:
// Translucent desktop backdrop or movable compact window frame, anytime photo drag,
// animated GIF/WebP playback, crisp vector icons, live zoom %, sibling filmstrip, PSD layer viewer/extractor, and "Открыть в Lumina".
class QuickViewerWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit QuickViewerWindow(LibraryDatabase* db,
                               const QString& initialFilePath,
                               QWidget* parent = nullptr);

signals:
    void openInLibraryRequested(const QString& filePath);

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void loadSiblingList(const QString& currentFile);
    void populateMiniFilmstrip();
    void openFileAt(int index);
    void stopMovie();
    void updateRenderedImage(bool resetView);
    void updateMetaAndZoomDisplay();
    void updateStarButton();
    void updateBackdropButton();
    void cycleBackdropMode();
    void toggleStar();
    void rotateImage(int deltaDeg);
    void toggleWindowedMode(bool windowed);
    void populatePsdLayers();
    void exportSelectedPsdLayer();
    void showExportFormatMenu();
    void exportViewerImage(const QString& forcedExt = QString());
    void retranslateUi();

    LibraryDatabase* db_;
    QStringList siblingFiles_;
    int currentIndex_ = 0;

    PhotoRecord currentRecord_;
    ExifMetadata currentMeta_;
    QImage rawImage_;
    QImage activeLayerImage_;
    QList<PsdLayerInfo> psdLayerDetails_;
    bool currentIsPsd_ = false;
    bool currentIsAnimated_ = false;
    QMovie* movie_ = nullptr;
    bool firstAnimFrame_ = false;
    QString currentError_;
    bool windowedMode_ = false;
    bool draggingWindow_ = false;
    QPoint windowDragOffset_;

    ViewerCanvasWidget* canvas_ = nullptr;
    QWidget* psdDrawer_ = nullptr;
    QLabel* psdHeader_ = nullptr;
    QLabel* metaLabel_ = nullptr;
    QLabel* zoomPctLabel_ = nullptr;
    QListWidget* filmstrip_ = nullptr;
    QListWidget* psdLayerList_ = nullptr;
    QPushButton* exportLayerBtn_ = nullptr;
    QPushButton* settingsBtn_ = nullptr;
    QPushButton* winModeBtn_ = nullptr;
    QPushButton* closeBtn_ = nullptr;
    QPushButton* fitBtn_ = nullptr;
    QPushButton* bgBtn_ = nullptr;
    QPushButton* openEditorBtn_ = nullptr;
    QPushButton* exportBtn_ = nullptr;
    QPushButton* prevBtn_ = nullptr;
    QPushButton* nextBtn_ = nullptr;
    QPushButton* rotLeftBtn_ = nullptr;
    QPushButton* rotRightBtn_ = nullptr;
    QPushButton* starBtn_ = nullptr;
    QPushButton* layersBtn_ = nullptr;
};

} // namespace lumina
