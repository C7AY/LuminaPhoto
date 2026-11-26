#pragma once

#include <QDateTime>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QUuid>

namespace lumina {

enum class CanvasBackdropMode {
    None = 0,
    White = 1,
    Black = 2
};

enum class EffectPreset {
    None = 0,
    // Tab 3: Обработка (Picasa tab 3)
    SharpenEffect = 1,
    Sepia = 2,
    BlackWhite = 3,
    WarmLight = 4,
    FilmGrain = 5,
    ColorTint = 6,
    SatBoost = 7,
    SoftFocus = 8,
    Glow = 9,
    BwFilter = 10,
    BwFocus = 11,
    GradTint = 12,
    // Tab 4: Художественные (Picasa tab 4)
    Infrared = 13,
    Lomo = 14,
    Holga = 15,
    HdrEffect = 16,
    Cinemascope = 17,
    Orton = 18,
    Vintage60s = 19,
    Invert = 20,
    HeatMap = 21,
    CrossProcess = 22,
    Posterize = 23,
    DuoTone = 24,
    // Tab 5: Творческие / Рамки (Picasa tab 5)
    Boost = 25,
    Soften = 26,
    Vignette = 27,
    Pixelate = 28,
    FocalZoom = 29,
    PencilSketch = 30,
    Neon = 31,
    ComicHalftone = 32,
    Contour = 33,
    DropShadow = 34,
    MatteFrame = 35,
    Polaroid = 36,
    // Stackable Auto-Fixes & Free-Positioned Text Layer
    AutoLucky = 37,
    AutoContrast = 38,
    AutoColor = 39,
    AutoBalance = 40,
    TextOverlay = 41,
    // Legacy aliases preserved for compatibility
    CoolFilm = GradTint,
    HighContrast = Boost,
    Cinema = Cinemascope
};

struct EffectLayer {
    QString id;
    EffectPreset preset = EffectPreset::None;
    int intensity = 80; // 0 .. 100
    int param2 = 50;    // 0 .. 100
    bool enabled = true;

    // TextOverlay properties (normalized posX/posY anywhere on canvas)
    QString text;
    double posX = 0.5;
    double posY = 0.86;
    int textSize = 36;
    QString textColor = QStringLiteral("#FFFFFF");

    static EffectLayer create(EffectPreset p, int initIntensity = 80, int initParam2 = 50) {
        EffectLayer layer;
        layer.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        layer.preset = p;
        layer.intensity = initIntensity;
        layer.param2 = initParam2;
        layer.enabled = true;
        return layer;
    }

    static EffectLayer createText(const QString& txt,
                                  double x = 0.5,
                                  double y = 0.86,
                                  int size = 36,
                                  const QString& color = QStringLiteral("#FFFFFF")) {
        EffectLayer layer = create(EffectPreset::TextOverlay, 100, 50);
        layer.text = txt;
        layer.posX = x;
        layer.posY = y;
        layer.textSize = size;
        layer.textColor = color.isEmpty() ? QStringLiteral("#FFFFFF") : color;
        return layer;
    }

    QJsonObject toJson() const {
        QJsonObject o;
        o["id"] = id;
        o["preset"] = static_cast<int>(preset);
        o["intensity"] = intensity;
        o["param2"] = param2;
        o["enabled"] = enabled;
        if (preset == EffectPreset::TextOverlay || !text.isEmpty()) {
            o["text"] = text;
            o["posX"] = posX;
            o["posY"] = posY;
            o["textSize"] = textSize;
            o["textColor"] = textColor;
        }
        return o;
    }

    static EffectLayer fromJson(const QJsonObject& o) {
        EffectLayer l;
        l.id = o["id"].toString();
        if (l.id.isEmpty()) {
            l.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        l.preset = static_cast<EffectPreset>(o["preset"].toInt(0));
        l.intensity = o["intensity"].toInt(80);
        l.param2 = o["param2"].toInt(50);
        l.enabled = o["enabled"].toBool(true);
        l.text = o["text"].toString();
        l.posX = o["posX"].toDouble(0.5);
        l.posY = o["posY"].toDouble(0.86);
        l.textSize = o["textSize"].toInt(36);
        l.textColor = o["textColor"].toString(QStringLiteral("#FFFFFF"));
        return l;
    }
};

struct PhotoEdits {
    int rotationDeg = 0;        // 0, 90, 180, 270
    double straightenDeg = 0.0; // -25.0 .. +25.0

    // Normalized crop box [0..1] allowing arbitrary focus placement & extreme zoom
    double cropLeft = 0.0;
    double cropTop = 0.0;
    double cropRight = 1.0;
    double cropBottom = 1.0;
    int cropZoom = 0;           // 0 .. 300 (%) synchronized around crop box center

    int sharpen = 0;            // 0 .. 200
    bool basicEnabled = true;
    bool cropEnabled = true;

    int brightness = 0;         // -100 .. +150 (Fill light)
    int highlights = 0;         // -100 .. +100
    int shadows = 0;            // -100 .. +100
    int contrast = 0;           // -100 .. +100
    int saturation = 0;         // -100 .. +150
    int temperature = 0;        // -100 .. +100
    int tint = 0;               // -100 .. +100 (Green <-> Magenta)
    int hueShift = 0;           // -180 .. +180 degrees (Full color wheel)
    bool lightColorEnabled = true;

    QString overlayText;
    int overlayTextSize = 28;

    QList<EffectLayer> effectLayers;

    bool hasCrop() const {
        return cropLeft > 0.002 || cropTop > 0.002 || cropRight < 0.998 || cropBottom < 0.998;
    }

    bool hasBasicLayerAdjustments() const {
        return !qFuzzyIsNull(straightenDeg) || cropZoom != 0 || sharpen != 0;
    }

    bool hasBasicAdjustments() const {
        return rotationDeg != 0 || hasCrop() || hasBasicLayerAdjustments();
    }

    bool hasLightColorAdjustments() const {
        return brightness != 0 || highlights != 0 || shadows != 0 || contrast != 0 ||
               saturation != 0 || temperature != 0 || tint != 0 || hueShift != 0;
    }

    bool isDefault() const {
        return !hasBasicAdjustments() &&
               !hasLightColorAdjustments() &&
               overlayText.isEmpty() &&
               effectLayers.isEmpty();
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["rotationDeg"] = rotationDeg;
        obj["straightenDeg"] = straightenDeg;
        obj["cropLeft"] = cropLeft;
        obj["cropTop"] = cropTop;
        obj["cropRight"] = cropRight;
        obj["cropBottom"] = cropBottom;
        obj["cropZoom"] = cropZoom;
        obj["sharpen"] = sharpen;
        obj["basicEnabled"] = basicEnabled;
        obj["cropEnabled"] = cropEnabled;

        obj["brightness"] = brightness;
        obj["highlights"] = highlights;
        obj["shadows"] = shadows;
        obj["contrast"] = contrast;
        obj["saturation"] = saturation;
        obj["temperature"] = temperature;
        obj["tint"] = tint;
        obj["hueShift"] = hueShift;
        obj["lightColorEnabled"] = lightColorEnabled;

        obj["overlayText"] = overlayText;
        obj["overlayTextSize"] = overlayTextSize;

        QJsonArray layersArr;
        for (const EffectLayer& l : effectLayers) {
            layersArr.append(l.toJson());
        }
        obj["effectLayers"] = layersArr;
        return obj;
    }

    static PhotoEdits fromJson(const QJsonObject& obj) {
        PhotoEdits e;
        e.rotationDeg = obj["rotationDeg"].toInt(0);
        e.straightenDeg = obj["straightenDeg"].toDouble(0.0);
        e.cropLeft = obj["cropLeft"].toDouble(0.0);
        e.cropTop = obj["cropTop"].toDouble(0.0);
        e.cropRight = obj["cropRight"].toDouble(1.0);
        e.cropBottom = obj["cropBottom"].toDouble(1.0);
        e.cropZoom = obj["cropZoom"].toInt(0);
        e.sharpen = obj["sharpen"].toInt(0);
        e.basicEnabled = obj["basicEnabled"].toBool(true);
        e.cropEnabled = obj["cropEnabled"].toBool(true);

        e.brightness = obj["brightness"].toInt(0);
        e.highlights = obj["highlights"].toInt(0);
        e.shadows = obj["shadows"].toInt(0);
        e.contrast = obj["contrast"].toInt(0);
        e.saturation = obj["saturation"].toInt(0);
        e.temperature = obj["temperature"].toInt(0);
        e.tint = obj["tint"].toInt(0);
        e.hueShift = obj["hueShift"].toInt(0);
        e.lightColorEnabled = obj["lightColorEnabled"].toBool(true);

        e.overlayText = obj["overlayText"].toString();
        e.overlayTextSize = obj["overlayTextSize"].toInt(28);

        if (obj.contains("effectLayers") && obj["effectLayers"].isArray()) {
            const QJsonArray arr = obj["effectLayers"].toArray();
            for (const QJsonValue& val : arr) {
                if (val.isObject()) {
                    e.effectLayers.append(EffectLayer::fromJson(val.toObject()));
                }
            }
        } else {
            const int legacyEff = obj["effect"].toInt(0);
            const int legacyInt = obj["effectIntensity"].toInt(80);
            const int legacyVig = obj["vignetteAmount"].toInt(0);
            if (legacyEff > 0) {
                e.effectLayers.append(EffectLayer::create(static_cast<EffectPreset>(legacyEff), legacyInt));
            }
            if (legacyVig > 0) {
                e.effectLayers.append(EffectLayer::create(EffectPreset::Vignette, legacyVig));
            }
        }

        // Migrate legacy overlayText cleanly into a stackable TextOverlay layer
        if (!e.overlayText.trimmed().isEmpty()) {
            e.effectLayers.append(EffectLayer::createText(e.overlayText, 0.5, 0.86, std::max(16, e.overlayTextSize)));
            e.overlayText.clear();
        }
        return e;
    }
};

struct PsdLayerInfo {
    QString name;
    QImage image;
    QRect bounds;
    bool visible = true;
};

struct ExifMetadata {
    QString camera;
    QString lens;
    QString focalLength;
    QString aperture;
    QString shutterSpeed;
    QString software;
    int iso = 0;
    QString rawMetadataText;
    bool hasAiMetadata = false;
    QStringList psdLayers;
    QList<PsdLayerInfo> psdLayerDetails;
};

struct PhotoRecord {
    qint64 id = 0;
    QString filePath;
    QString fileName;
    QString folderPath;
    QString caption;
    bool starred = false;
    int width = 0;
    int height = 0;
    qint64 fileSizeBytes = 0;
    QDateTime dateTaken;
    ExifMetadata exif;
    PhotoEdits edits;
};

struct AlbumRecord {
    qint64 id = 0;
    QString name;
    QString iconKey;
    int photoCount = 0;
    QString previewFilePath;
};

struct FolderSummary {
    QString folderPath;
    QString displayName;
    int photoCount = 0;
    QString previewFilePath;
};

} // namespace lumina
