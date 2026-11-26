#pragma once

#include "PhotoModels.h"
#include <QImage>
#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>

namespace lumina {

struct PsdLoadResult {
    bool success = false;
    QImage image;
    QStringList layerNames;
    QList<PsdLayerInfo> layers;
    QString errorMessage;
};

class PsdParser {
public:
    // Fast 26-byte header dimension check for rapid folder indexing
    static QSize readHeaderDimensions(const QString& filePath);

    // Native C++20 Adobe Photoshop (.psd) parser:
    // Reads composite merged RGB/RGBA/Grayscale image (Raw and PackBits RLE, 8-bit and 16-bit),
    // decodes Unicode UTF-16BE ('luni') + ANSI Windows layer names, and extracts per-layer images.
    static PsdLoadResult load(const QString& filePath, bool readLayers = true);
};

} // namespace lumina
