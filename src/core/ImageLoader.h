#pragma once

#include "PhotoModels.h"
#include <QImage>
#include <QList>
#include <QSize>
#include <QString>
#include <functional>

namespace lumina {

struct LoadedImageResult {
    bool success = false;
    QImage image;
    ExifMetadata metadata;
    QString errorMessage;
    bool isPsd = false;
    bool isAnimated = false;
};

class ImageLoader {
public:
    static QStringList supportedExtensions();
    static bool isSupportedFile(const QString& filePath);
    static bool isAnimatedImage(const QString& filePath);

    static QString effectPresetName(EffectPreset preset);
    static int effectCategoryIndex(EffectPreset preset); // 0=Обработка, 1=Арт, 2=Рамки, -1=Базовые/Свет
    static QList<EffectPreset> effectsForCategory(int categoryIndex);

    // Ultra-fast header dimension check without decoding full pixel buffers
    static QSize readHeaderDimensions(const QString& filePath);

    // Extracts EXIF camera details + PNG/JPEG/WebP AI generation metadata (prompts, parameters, XMP, comments)
    static ExifMetadata readMetadata(const QString& filePath);

    // Losslessly removes EXIF, XMP, JPEG comments, and PNG generation chunks from disk
    static bool stripMetadata(const QString& filePath, QString* errorOut = nullptr);

    // Loads image (including .psd with extracted Unicode layers, .svg/.svgz, .gif, .ico, .avif/.heic, .tiff, .jpg, .png, .webp).
    // If maxDimension > 0, uses scaled decoding when supported for fast thumbnail generation.
    static LoadedImageResult loadImage(const QString& filePath, int maxDimension = 0);

    // Draws a TextOverlay layer proportionally inside targetRect (shared by canvas live preview & bake).
    static void drawTextOverlay(QPainter& painter, const QRectF& targetRect, const EffectLayer& layer);

    // Applies non-destructive edits (crop, light/color, hue, sharpen, stacked EffectLayers, rotation, text).
    // Set skipCrop=true during interactive crop overlay; skipLayerId hides a live-dragged layer from the baked base.
    static QImage applyEdits(const QImage& source,
                             const PhotoEdits& edits,
                             bool skipCrop = false,
                             const QString& skipLayerId = QString());

    // Writes an edited image to disk, preserving original EXIF/XMP/PNG generation chunks unless stripMetadata=true.
    static bool saveEditedImage(const QString& sourceFilePath,
                                const QString& destFilePath,
                                const QByteArray& format,
                                const QImage& image,
                                bool stripMetadata,
                                const ExifMetadata* fallbackMeta = nullptr,
                                QString* errorOut = nullptr);

    // Encodes an animated GIF89a (applying PhotoEdits to every frame and preserving per-frame delays/looping).
    static bool saveAnimatedGif(const QString& sourceFilePath,
                                const QString& destFilePath,
                                const PhotoEdits& edits,
                                const QImage& fallbackImage = QImage(),
                                const std::function<bool(int frameIndex, int totalFrames)>& progressCb = nullptr,
                                QString* errorOut = nullptr);
};

} // namespace lumina
