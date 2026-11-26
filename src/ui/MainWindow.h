#pragma once

#include "../core/LibraryDatabase.h"
#include "../core/PhotoModels.h"
#include <QCheckBox>
#include <QComboBox>
#include <QHash>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QMainWindow>
#include <QMovie>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointF>
#include <QPushButton>
#include <QRectF>
#include <QSet>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTimer>
#include <array>
#include <cstdint>

namespace lumina {

class SmoothButtonFilter;
class FilmstripDelegate;

class HistogramWidget : public QWidget {
    Q_OBJECT
public:
    explicit HistogramWidget(QWidget* parent = nullptr);
    void setImage(const QImage& img);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::array<int, 64> histR_{};
    std::array<int, 64> histG_{};
    std::array<int, 64> histB_{};
    int maxCount_ = 1;
    bool hasData_ = false;
};

class SnapSlider : public QSlider {
    Q_OBJECT
public:
    explicit SnapSlider(int minVal, int maxVal, int defaultVal, QWidget* parent = nullptr);

    int defaultValue() const { return defaultVal_; }
    void syncValueSilently(int v);

signals:
    void editStarted();
    void snappedValueChanged(int value);
    void commitRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    int defaultVal_ = 0;
    int prevVal_ = 0;
    bool internalAdjust_ = false;
};

class MagneticSliderRow : public QWidget {
    Q_OBJECT
public:
    explicit MagneticSliderRow(const QString& title,
                               int minVal,
                               int maxVal,
                               int defaultVal = 0,
                               const QString& unitSuffix = QString(),
                               QWidget* parent = nullptr);

    int value() const { return slider_->value(); }
    void setTitle(const QString& title);
    void setValueSilently(int v);
    void refreshTheme();

signals:
    void editStarted();
    void liveValueChanged(int v);
    void valueCommitted(int v);

private:
    void updateReadout(int v);

    int defaultVal_ = 0;
    QString suffix_;
    QLabel* titleLabel_ = nullptr;
    QLabel* valueLabel_ = nullptr;
    QPushButton* resetBtn_ = nullptr;
    SnapSlider* slider_ = nullptr;
};

// Custom Photo Card Delegate & Grid with direct '+' (Add to Album) and '★' (Favorite) corner buttons
class PhotoCardDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    enum ItemRoles {
        StarredRole = Qt::UserRole + 10,
        IsPsdRole = Qt::UserRole + 11,
        HoverRowRole = Qt::UserRole + 12,
        ThumbPixmapRole = Qt::UserRole + 13
    };

    explicit PhotoCardDelegate(QObject* parent = nullptr);

    static QRect cardRect(const QRect& itemRect);
    static QRect thumbRect(const QRect& itemRect);
    static QRect plusButtonRect(const QRect& itemRect);
    static QRect starButtonRect(const QRect& itemRect);

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

class PhotoGridWidget : public QListWidget {
    Q_OBJECT
public:
    explicit PhotoGridWidget(QWidget* parent = nullptr);

    int hoveredRow() const { return hoveredRow_; }
    int hoveredBtn() const { return hoveredBtn_; } // 0 = none, 1 = plus, 2 = star

signals:
    void cardPlusClicked(int row, const QPoint& globalPos);
    void cardStarClicked(int row);

protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    int hitCornerButton(const QPoint& pos, QModelIndex* idxOut = nullptr) const;

    int hoveredRow_ = -1;
    int hoveredBtn_ = 0;
    int pressedRow_ = -1;
    int pressedBtn_ = 0;
};

// Interactive Editor Canvas: supports Fit/1:1/Wheel/Center zoom, anytime drag panning,
// free-focus interactive crop handles, zero-lag live draggable text overlay, backdrop toggle, grid, and eyedropper.
class EditorCanvasWidget : public QWidget {
    Q_OBJECT
public:
    explicit EditorCanvasWidget(QWidget* parent = nullptr);

    void setDisplayImage(const QImage& img, bool resetView = false);
    void setActualSizeMode(bool actual);
    void zoomByCenter(double factor);
    void setCropMode(bool enabled, double l, double t, double r, double b);
    void setLiveTextLayer(const EffectLayer* layer);
    void setShowGrid(bool show);
    void setEyedropperMode(bool enabled);
    void setBackdropMode(CanvasBackdropMode mode);

    bool isActualSize() const { return actualSize_; }
    bool isCropMode() const { return cropMode_; }
    bool isDraggingText() const { return activeDrag_ == DragHandle::DragText; }
    CanvasBackdropMode backdropMode() const { return backdropMode_; }

signals:
    void cropRectAdjusted(double l, double t, double r, double b);
    void textPositionDragStarted();
    void textPositionCommitted(double normX, double normY);
    void colorPicked(const QColor& color);
    void zoomStateChanged(bool actualSize);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    enum class DragHandle {
        None,
        PanImage,
        DragText,
        MoveCrop,
        TopLeft,
        Top,
        TopRight,
        Right,
        BottomRight,
        Bottom,
        BottomLeft,
        Left
    };

    double computeFitScale() const;
    double effectiveScale() const;
    QRectF imageScreenRect() const;
    QRectF cropScreenRect(const QRectF& imgRect) const;
    DragHandle hitTestCropHandle(const QPointF& pos, const QRectF& cropRect) const;
    void updateCursorForPos(const QPointF& pos);

    QImage image_;
    QPixmap pixmap_;
    bool hasAlpha_ = false;
    CanvasBackdropMode backdropMode_ = CanvasBackdropMode::None;
    bool actualSize_ = false;
    bool customZoom_ = false;
    double zoomFactor_ = 1.0;
    QPointF panOffset_{0.0, 0.0};

    bool cropMode_ = false;
    double cropL_ = 0.0;
    double cropT_ = 0.0;
    double cropR_ = 1.0;
    double cropB_ = 1.0;

    bool hasLiveText_ = false;
    EffectLayer liveTextLayer_;

    bool showGrid_ = false;
    bool eyedropperMode_ = false;

    DragHandle activeDrag_ = DragHandle::None;
    QPoint dragStartPos_;
    QPointF panStartOffset_{0.0, 0.0};
    QPointF textDragGrabOffset_{0.0, 0.0};
    double startL_ = 0.0, startT_ = 0.0, startR_ = 1.0, startB_ = 1.0;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(LibraryDatabase* db, QWidget* parent = nullptr);
    ~MainWindow() override;

    void openPhotoInEditor(const QString& filePath);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    struct CachedEditorAsset {
        QImage raw;
        QImage screen;
        QImage drag;
        ExifMetadata meta;
        QList<PsdLayerInfo> psdLayers;
        bool isAnimated = false;
    };

    void setupUi();
    void applyTheme();
    void updateThemeIcons();
    void retranslateUi();
    void openSettingsDialog();

    void reloadSidebar();
    void updateSidebarCompactMode();
    void updateSidebarThumbnailsForPath(const QString& filePath);
    void reloadPhotoGrid();
    void refreshGridItemIcon(int row);

    // Bounded priority thumbnail queue (visible viewport cards first, zero UI freeze)
    void enqueueThumbnail(const QString& filePath, bool highPriority = false);
    void prioritizeVisibleThumbnails();
    void pumpThumbnailQueue();

    // Non-blocking background folder scanning with progress dialog
    void startFolderImport(const QString& dirPath, const QString& progressTitle);

    // Library Actions
    void onAddFolderClicked();
    void onScanPicturesClicked();
    void onCreateAlbumClicked();
    void onRegisterAssociationsClicked();
    void onSidebarItemActivated(QListWidgetItem* current);
    void showSidebarContextMenu(const QPoint& pos);
    void pruneMissingFilesAsync();
    void removeMissingPhotoFromView(const QString& filePath);
    void purgePathCaches(const QStringList& paths);
    void onAddSelectedToAlbum();
    void setSelectedStarred(bool starred);
    void toggleRowStarred(int row);
    void showRowAlbumMenu(int row, const QPoint& globalPos);
    void showPhotoGridContextMenu(const QPoint& pos);

    // Editor Actions
    void onOpenEditorForRecord(const PhotoRecord& rec);
    void applyLoadedEditorAsset(const PhotoRecord& rec, const CachedEditorAsset& asset, bool resetView);
    void insertEditorCache(const QString& filePath, const CachedEditorAsset& asset);
    void preloadNeighborInEditor(int delta);
    void stopEditorMovie();
    void startEditorMovieIfAnimated(const QString& filePath, bool isAnimated);
    void openRelativePhotoInEditor(int delta);
    void setCropEditingEnabled(bool enabled, bool resetViewAfterConfirm = true);
    void updateCropReadout();
    void updateBackdropButton();
    void cycleBackdropMode();
    void populateEditorThumbnails();
    void populateEffectTabsAsync(uint64_t seq, const QImage& mini);
    void populateEditorFilmstrip();
    void populateEffectStackList();
    void populateEditorPsdLayers();
    void updateActiveEffectControls();
    void refreshMetadataInspector();
    void syncEditorSliders();
    void rebuildEditorPreviewCaches();
    void refreshEditorPreview(bool resetView = false);
    void scheduleLivePreview();
    void saveEditorEdits();
    void exportEditedPhoto();
    void exportSelectedEditorPsdLayer();
    void copyCurrentMetadata();
    void updateEditorPaneConstraints();
    void adaptLeftPaneLayout(int paneW);
    void updateLeftTabsHeight();
    void updateActiveLayerLabelInStack();

    void addOrSelectEffectLayer(EffectPreset preset, int defaultIntensity = 80);
    void addNewTextLayer();
    int selectedEffectLayerIndex() const;
    void selectEffectLayerById(const QString& layerId);

    void pushUndoState();
    void undoLastEdit();
    void updateUndoButton();

    LibraryDatabase* db_;
    QString currentFolderFilter_;
    qint64 currentAlbumFilter_ = 0;
    bool onlyStarredFilter_ = false;
    QString searchQuery_;
    int thumbSize_ = 176;
    bool collectionCollapsed_ = false;
    bool albumsCollapsed_ = false;
    bool foldersCollapsed_ = false;
    bool sidebarCompact_ = false;
    bool pruneInFlight_ = false;
    qint64 lastPruneMs_ = 0;

    QList<PhotoRecord> currentPhotos_;
    QHash<QString, QImage> thumbImageCache_;
    QSet<QString> pendingThumbs_;
    QSet<QString> inFlightThumbs_;
    QList<QString> thumbQueue_;

    // Fast async editor image & metadata cache (LRU max 5 entries + neighbor preloading)
    QHash<QString, CachedEditorAsset> editorCache_;
    QList<QString> editorCacheOrder_;
    QSet<QString> preloadingEditorPaths_;
    uint64_t editorLoadSeq_ = 0;
    uint64_t thumbJobSeq_ = 0;

    // Animated GIF/WebP playback in Editor
    QMovie* editorMovie_ = nullptr;
    qint64 lastAnimHistMs_ = 0;
    bool animFirstFrame_ = false;

    SmoothButtonFilter* btnFilter_ = nullptr;
    FilmstripDelegate* editorFilmstripDelegate_ = nullptr;

    // Main stack: 0 = Library Grid, 1 = Non-destructive Editor
    QStackedWidget* mainStack_ = nullptr;

    // Library UI
    QWidget* topBar_ = nullptr;
    QLabel* brandLabel_ = nullptr;
    QLabel* zoomLabel_ = nullptr;
    QPushButton* addFolderBtn_ = nullptr;
    QPushButton* newAlbumBtn_ = nullptr;
    QPushButton* assocBtn_ = nullptr;
    QPushButton* libSettingsBtn_ = nullptr;
    QSplitter* libSplitter_ = nullptr;
    QListWidget* sidebarList_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
    QPushButton* starredFilterBtn_ = nullptr;
    QSlider* thumbSlider_ = nullptr;
    QStackedWidget* gridContentStack_ = nullptr;
    QLabel* emptyTitle_ = nullptr;
    QLabel* emptyDesc_ = nullptr;
    QPushButton* emptyAddBtn_ = nullptr;
    QPushButton* emptyScanPicsBtn_ = nullptr;
    PhotoGridWidget* photoGrid_ = nullptr;
    QLabel* statusCountLabel_ = nullptr;

    // Bottom Tray
    QWidget* trayBar_ = nullptr;
    QLabel* trayInfoLabel_ = nullptr;
    QPushButton* trayStarBtn_ = nullptr;
    QPushButton* trayUnstarBtn_ = nullptr;
    QLabel* albumLbl_ = nullptr;
    QComboBox* trayAlbumCombo_ = nullptr;
    QPushButton* trayAddToAlbumBtn_ = nullptr;
    QPushButton* trayNewAlbBtn_ = nullptr;
    QPushButton* openSelectedBtn_ = nullptr;
    QPushButton* clearSelBtn_ = nullptr;

    // Editor State
    PhotoRecord editingRecord_;
    ExifMetadata editingLoadedMeta_;
    QImage editingRawImage_;
    QImage editingScreenImage_;
    QImage editingDragImage_;
    QImage editingPreviewLayerImage_;
    QList<PsdLayerInfo> editingPsdLayers_;
    QList<PhotoEdits> undoStack_;
    QString selectedLayerId_;
    bool compareOriginal_ = false;
    bool eyedropperActive_ = false;
    bool stripMetadataOnExport_ = false;
    bool dragPreviewActive_ = false;
    bool editorInitialSplitDone_ = false;
    bool editingIsAnimated_ = false;
    QTimer* previewCoalesceTimer_ = nullptr;
    QTimer* dbSaveTimer_ = nullptr;

    // Editor Widgets
    QWidget* edTop_ = nullptr;
    QPushButton* backBtn_ = nullptr;
    QPushButton* edPrevBtn_ = nullptr;
    QPushButton* edNextBtn_ = nullptr;
    QSplitter* edBody_ = nullptr;
    QWidget* edLeftPane_ = nullptr;
    QWidget* edRightPane_ = nullptr;
    QWidget* tabBasicInner_ = nullptr;
    QWidget* tabTuningInner_ = nullptr;
    QWidget* tabProcessInner_ = nullptr;
    QWidget* tabArtInner_ = nullptr;
    QWidget* tabFramesInner_ = nullptr;
    QWidget* rightInner_ = nullptr;
    QListWidget* editorFilmstrip_ = nullptr;
    EditorCanvasWidget* editorCanvas_ = nullptr;
    QWidget* cropCanvasBar_ = nullptr;
    QLabel* ccTitle_ = nullptr;
    QLabel* cropCanvasInfoLabel_ = nullptr;
    QPushButton* canvasApplyCropBtn_ = nullptr;
    QPushButton* canvasResetCropBtn_ = nullptr;
    QPushButton* canvasCloseCropBtn_ = nullptr;
    QLineEdit* editorCaptionEdit_ = nullptr;
    QWidget* edBottomBar_ = nullptr;
    QPushButton* rotLeftBtn_ = nullptr;
    QPushButton* rotRightBtn_ = nullptr;
    QLabel* editorMetaStatus_ = nullptr;
    QPushButton* editorStarBtn_ = nullptr;
    QPushButton* editorCompareBtn_ = nullptr;
    QPushButton* editorZoomBtn_ = nullptr;
    QPushButton* edBackdropBtn_ = nullptr;
    QPushButton* undoBtn_ = nullptr;
    QPushButton* resetEditsBtn_ = nullptr;
    QPushButton* edRightToggleBtn_ = nullptr;
    QPushButton* edSettingsBtn_ = nullptr;
    QPushButton* exportBtn_ = nullptr;
    QPushButton* eyedropperBtn_ = nullptr;

    QTabWidget* edTabs_ = nullptr;
    QListWidget* basicTilesList_ = nullptr;
    QWidget* cropBarWidget_ = nullptr;
    QLabel* cropHint_ = nullptr;
    QPushButton* applyCropBtn_ = nullptr;
    QPushButton* resetCropBtn_ = nullptr;
    QWidget* basicAdjustWidget_ = nullptr;
    QPushButton* resetBasicBtn_ = nullptr;
    QPushButton* autoColorBtn_ = nullptr;
    QPushButton* resetTuningBtn_ = nullptr;
    QListWidget* processEffectsList_ = nullptr;
    QListWidget* artEffectsList_ = nullptr;
    QListWidget* frameEffectsList_ = nullptr;

    // Contextual Active Effect / Text Settings Box
    QWidget* activeEffectBox_ = nullptr;
    QLabel* activeEffectTitle_ = nullptr;
    QPushButton* removeSelectedEffBtn_ = nullptr;
    QWidget* textControlsWidget_ = nullptr;
    QLabel* textDragHint_ = nullptr;
    QLineEdit* textLayerEdit_ = nullptr;
    MagneticSliderRow* textLayerSizeRow_ = nullptr;
    QPushButton* textColorBtn_ = nullptr;
    QPushButton* addAnotherTextBtn_ = nullptr;
    MagneticSliderRow* effectIntensityRow_ = nullptr;
    MagneticSliderRow* effectParam2Row_ = nullptr;

    // Right Inspector: Photoshop-style Layer Stack + Histogram + EXIF & AI Metadata + PSD Layers
    QLabel* stackHeader_ = nullptr;
    QLabel* stackHint_ = nullptr;
    QListWidget* effectStackList_ = nullptr;
    QPushButton* layerUpBtn_ = nullptr;
    QPushButton* layerDownBtn_ = nullptr;
    QPushButton* layerDeleteBtn_ = nullptr;
    QLabel* histTitle_ = nullptr;
    HistogramWidget* histogramWidget_ = nullptr;
    QLabel* editorExifLabel_ = nullptr;
    QPlainTextEdit* metadataTextEdit_ = nullptr;
    QPushButton* copyMetaBtn_ = nullptr;
    QPushButton* stripMetaBtn_ = nullptr;
    QLabel* metaHintLabel_ = nullptr;
    QLabel* editorLayersTitle_ = nullptr;
    QListWidget* editorPsdLayers_ = nullptr;
    QPushButton* editorExportPsdLayerBtn_ = nullptr;

    // Tab 1: Basic straighten slider
    MagneticSliderRow* straightenRow_ = nullptr;

    // Tab 2: Light & Color sliders
    MagneticSliderRow* brightnessRow_ = nullptr;
    MagneticSliderRow* highlightsRow_ = nullptr;
    MagneticSliderRow* shadowsRow_ = nullptr;
    MagneticSliderRow* contrastRow_ = nullptr;
    MagneticSliderRow* saturationRow_ = nullptr;
    MagneticSliderRow* tempRow_ = nullptr;
    MagneticSliderRow* tintRow_ = nullptr;
    MagneticSliderRow* hueShiftRow_ = nullptr;
};

} // namespace lumina

