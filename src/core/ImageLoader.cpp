#include "ImageLoader.h"
#include "AppSettings.h"
#include "PsdParser.h"

#ifdef LUMINA_HAS_OIIO
#include <OpenImageIO/imageio.h>
#endif

#ifdef _WIN32
#include <objbase.h>
#include <wincodec.h>
#endif

#ifdef LUMINA_HAS_QT_SVG
#include <QSvgRenderer>
#endif

#include <QBuffer>
#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QImageReader>
#include <QImageWriter>
#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>
#include <QSaveFile>
#include <QStringDecoder>
#include <QTransform>
#include <QtEndian>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace lumina {

QStringList ImageLoader::supportedExtensions() {
    return {
        QStringLiteral("jpg"),
        QStringLiteral("jpeg"),
        QStringLiteral("jfif"),
        QStringLiteral("png"),
        QStringLiteral("webp"),
        QStringLiteral("gif"),
        QStringLiteral("svg"),
        QStringLiteral("svgz"),
        QStringLiteral("ico"),
        QStringLiteral("cur"),
        QStringLiteral("bmp"),
        QStringLiteral("tif"),
        QStringLiteral("tiff"),
        QStringLiteral("psd"),
        QStringLiteral("avif"),
        QStringLiteral("heic"),
        QStringLiteral("heif"),
        QStringLiteral("tga"),
        QStringLiteral("exr")
    };
}

bool ImageLoader::isSupportedFile(const QString& filePath) {
    const QString ext = QFileInfo(filePath).suffix().toLower();
    return supportedExtensions().contains(ext);
}

bool ImageLoader::isAnimatedImage(const QString& filePath) {
    const QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext != QStringLiteral("gif") && ext != QStringLiteral("webp")) {
        return false;
    }
    QImageReader reader(filePath);
    reader.setDecideFormatFromContent(true);
    return reader.supportsAnimation() && reader.imageCount() > 1;
}

QString ImageLoader::effectPresetName(EffectPreset preset) {
    switch (preset) {
        case EffectPreset::None: return trUi(QStringLiteral("Оригинал"), QStringLiteral("Original"));
        case EffectPreset::SharpenEffect: return trUi(QStringLiteral("Увеличить резкость"), QStringLiteral("Sharpen"));
        case EffectPreset::Sepia: return trUi(QStringLiteral("Сепия"), QStringLiteral("Sepia"));
        case EffectPreset::BlackWhite: return trUi(QStringLiteral("Чёрно-белый"), QStringLiteral("Black & White"));
        case EffectPreset::WarmLight: return trUi(QStringLiteral("Тона теплее"), QStringLiteral("Warmify"));
        case EffectPreset::FilmGrain: return trUi(QStringLiteral("Зернистость"), QStringLiteral("Film Grain"));
        case EffectPreset::ColorTint: return trUi(QStringLiteral("Оттенок"), QStringLiteral("Tint"));
        case EffectPreset::SatBoost: return trUi(QStringLiteral("Насыщенность"), QStringLiteral("Saturation"));
        case EffectPreset::SoftFocus: return trUi(QStringLiteral("Мягкий фокус"), QStringLiteral("Soft Focus"));
        case EffectPreset::Glow: return trUi(QStringLiteral("Сияние"), QStringLiteral("Glow"));
        case EffectPreset::BwFilter: return trUi(QStringLiteral("Ч-Б фильтр"), QStringLiteral("Filtered B&W"));
        case EffectPreset::BwFocus: return trUi(QStringLiteral("Ч-Б фокус"), QStringLiteral("Focal B&W"));
        case EffectPreset::GradTint: return trUi(QStringLiteral("Выбор тона"), QStringLiteral("Graduated Tint"));
        case EffectPreset::Infrared: return trUi(QStringLiteral("Инфракр. плёнка"), QStringLiteral("Infrared Film"));
        case EffectPreset::Lomo: return trUi(QStringLiteral("Ломография"), QStringLiteral("Lomo"));
        case EffectPreset::Holga: return trUi(QStringLiteral("В стиле Holga"), QStringLiteral("Holga-ish"));
        case EffectPreset::HdrEffect: return trUi(QStringLiteral("Эффект HDR"), QStringLiteral("HDR-ish"));
        case EffectPreset::Cinemascope: return trUi(QStringLiteral("Синемаскоп"), QStringLiteral("Cinemascope"));
        case EffectPreset::Orton: return trUi(QStringLiteral("Эффект Ортона"), QStringLiteral("Orton-ish"));
        case EffectPreset::Vintage60s: return trUi(QStringLiteral("В стиле 60-х"), QStringLiteral("1960's"));
        case EffectPreset::Invert: return trUi(QStringLiteral("Инвертировать"), QStringLiteral("Invert Colors"));
        case EffectPreset::HeatMap: return trUi(QStringLiteral("Тепловизор"), QStringLiteral("Heat Map"));
        case EffectPreset::CrossProcess: return trUi(QStringLiteral("Перекр. обраб."), QStringLiteral("Cross Process"));
        case EffectPreset::Posterize: return trUi(QStringLiteral("Постеризация"), QStringLiteral("Posterize"));
        case EffectPreset::DuoTone: return trUi(QStringLiteral("Дуплекс"), QStringLiteral("Duo-Tone"));
        case EffectPreset::Boost: return trUi(QStringLiteral("Усиление"), QStringLiteral("Boost"));
        case EffectPreset::Soften: return trUi(QStringLiteral("Смягчить"), QStringLiteral("Soften"));
        case EffectPreset::Vignette: return trUi(QStringLiteral("Виньетирование"), QStringLiteral("Vignette"));
        case EffectPreset::Pixelate: return trUi(QStringLiteral("Пикселизация"), QStringLiteral("Pixelate"));
        case EffectPreset::FocalZoom: return trUi(QStringLiteral("Спортивный фокус"), QStringLiteral("Focal Zoom"));
        case EffectPreset::PencilSketch: return trUi(QStringLiteral("Карандаш"), QStringLiteral("Pencil Sketch"));
        case EffectPreset::Neon: return trUi(QStringLiteral("Неон"), QStringLiteral("Neon"));
        case EffectPreset::ComicHalftone: return trUi(QStringLiteral("Комиксы"), QStringLiteral("Comic Book"));
        case EffectPreset::Contour: return trUi(QStringLiteral("Контур"), QStringLiteral("Line Art"));
        case EffectPreset::DropShadow: return trUi(QStringLiteral("Тень"), QStringLiteral("Drop Shadow"));
        case EffectPreset::MatteFrame: return trUi(QStringLiteral("Паспарту"), QStringLiteral("Matte"));
        case EffectPreset::Polaroid: return trUi(QStringLiteral("Polaroid"), QStringLiteral("Polaroid"));
        case EffectPreset::AutoLucky: return trUi(QStringLiteral("Мне повезёт!"), QStringLiteral("I'm Feeling Lucky"));
        case EffectPreset::AutoContrast: return trUi(QStringLiteral("Автоконтраст"), QStringLiteral("Auto Contrast"));
        case EffectPreset::AutoColor: return trUi(QStringLiteral("Автоцвет"), QStringLiteral("Auto Color"));
        case EffectPreset::AutoBalance: return trUi(QStringLiteral("Автобаланс"), QStringLiteral("Auto Balance"));
        case EffectPreset::TextOverlay: return trUi(QStringLiteral("Текст"), QStringLiteral("Text"));
    }
    return trUi(QStringLiteral("Эффект"), QStringLiteral("Effect"));
}

int ImageLoader::effectCategoryIndex(EffectPreset preset) {
    const int val = static_cast<int>(preset);
    if (val >= 1 && val <= 12) return 0;
    if (val >= 13 && val <= 24) return 1;
    if (val >= 25 && val <= 36) return 2;
    return -1; // Base / Auto / Text layers map to Tab 0 or 1
}

QList<EffectPreset> ImageLoader::effectsForCategory(int categoryIndex) {
    if (categoryIndex == 0) {
        return {
            EffectPreset::SharpenEffect, EffectPreset::Sepia, EffectPreset::BlackWhite,
            EffectPreset::WarmLight, EffectPreset::FilmGrain, EffectPreset::ColorTint,
            EffectPreset::SatBoost, EffectPreset::SoftFocus, EffectPreset::Glow,
            EffectPreset::BwFilter, EffectPreset::BwFocus, EffectPreset::GradTint
        };
    }
    if (categoryIndex == 1) {
        return {
            EffectPreset::Infrared, EffectPreset::Lomo, EffectPreset::Holga,
            EffectPreset::HdrEffect, EffectPreset::Cinemascope, EffectPreset::Orton,
            EffectPreset::Vintage60s, EffectPreset::Invert, EffectPreset::HeatMap,
            EffectPreset::CrossProcess, EffectPreset::Posterize, EffectPreset::DuoTone
        };
    }
    return {
        EffectPreset::Boost, EffectPreset::Soften, EffectPreset::Vignette,
        EffectPreset::Pixelate, EffectPreset::FocalZoom, EffectPreset::PencilSketch,
        EffectPreset::Neon, EffectPreset::ComicHalftone, EffectPreset::Contour,
        EffectPreset::DropShadow, EffectPreset::MatteFrame, EffectPreset::Polaroid
    };
}

QSize ImageLoader::readHeaderDimensions(const QString& filePath) {
    const QFileInfo fi(filePath);
    if (!fi.exists()) return QSize();
    const QString ext = fi.suffix().toLower();
    if (ext == QStringLiteral("psd")) {
        return PsdParser::readHeaderDimensions(filePath);
    }
#ifdef LUMINA_HAS_QT_SVG
    if (ext == QStringLiteral("svg") || ext == QStringLiteral("svgz")) {
        QSvgRenderer svg(filePath);
        if (svg.isValid()) {
            const QSize def = svg.defaultSize();
            return def.isValid() && !def.isEmpty() ? def : QSize(1024, 1024);
        }
    }
#endif
    QImageReader reader(filePath);
    reader.setDecideFormatFromContent(true);
    const QSize sz = reader.size();
    if (sz.isValid()) return sz;
    return QSize();
}

namespace {

// ==================== NATIVE EXIF / XMP / PNG / WEBP METADATA PARSER ====================
uint16_t readTiffU16(const QByteArray& data, int offset, bool littleEndian) {
    if (offset < 0 || offset + 2 > data.size()) return 0;
    const auto* p = reinterpret_cast<const uchar*>(data.constData() + offset);
    return littleEndian ? qFromLittleEndian<uint16_t>(p) : qFromBigEndian<uint16_t>(p);
}

uint32_t readTiffU32(const QByteArray& data, int offset, bool littleEndian) {
    if (offset < 0 || offset + 4 > data.size()) return 0;
    const auto* p = reinterpret_cast<const uchar*>(data.constData() + offset);
    return littleEndian ? qFromLittleEndian<uint32_t>(p) : qFromBigEndian<uint32_t>(p);
}

double readTiffRational(const QByteArray& data, int offset, bool littleEndian) {
    const uint32_t num = readTiffU32(data, offset, littleEndian);
    const uint32_t den = readTiffU32(data, offset + 4, littleEndian);
    if (den == 0) return 0.0;
    return static_cast<double>(num) / static_cast<double>(den);
}

QByteArray readTiffTagRawBytes(const QByteArray& tiff, int entryOffset, bool le, uint16_t& typeOut, uint32_t& countOut) {
    typeOut = readTiffU16(tiff, entryOffset + 2, le);
    countOut = readTiffU32(tiff, entryOffset + 4, le);

    int elemSize = 1;
    if (typeOut == 3 || typeOut == 8) elemSize = 2;
    else if (typeOut == 4 || typeOut == 9 || typeOut == 11) elemSize = 4;
    else if (typeOut == 5 || typeOut == 10 || typeOut == 12) elemSize = 8;

    const qint64 byteLen = static_cast<qint64>(countOut) * elemSize;
    if (byteLen <= 0 || byteLen > tiff.size()) return QByteArray();

    if (byteLen <= 4) {
        return tiff.mid(entryOffset + 8, static_cast<int>(byteLen));
    }
    const uint32_t valOff = readTiffU32(tiff, entryOffset + 8, le);
    if (valOff + byteLen > static_cast<qint64>(tiff.size())) return QByteArray();
    return tiff.mid(static_cast<int>(valOff), static_cast<int>(byteLen));
}

QString decodeExifUserComment(const QByteArray& raw) {
    if (raw.isEmpty()) return QString();
    if (raw.startsWith(QByteArray("ASCII\0\0\0", 8))) {
        return QString::fromUtf8(raw.mid(8)).remove(QChar('\0')).trimmed();
    }
    if (raw.startsWith(QByteArray("UNICODE\0", 8))) {
        const QByteArray body = raw.mid(8);
        // Heuristic: check whether UTF-16BE or UTF-16LE has more null high-bytes
        int evenZeros = 0, oddZeros = 0;
        for (int i = 0; i + 1 < body.size(); i += 2) {
            if (body[i] == '\0') ++evenZeros;
            if (body[i + 1] == '\0') ++oddZeros;
        }
        QStringDecoder dec(evenZeros > oddZeros ? QStringDecoder::Utf16BE : QStringDecoder::Utf16LE);
        QString decoded = dec.decode(body);
        return decoded.remove(QChar('\0')).trimmed();
    }
    return QString::fromUtf8(raw).remove(QChar('\0')).trimmed();
}

void parseTiffBlock(const QByteArray& tiff, ExifMetadata& meta, QStringList& extraLines) {
    if (tiff.size() < 8) return;
    bool le = false;
    if (tiff[0] == 'I' && tiff[1] == 'I') le = true;
    else if (tiff[0] == 'M' && tiff[1] == 'M') le = false;
    else return;

    if (readTiffU16(tiff, 2, le) != 42) return;
    const uint32_t ifd0Off = readTiffU32(tiff, 4, le);

    QString make, model;
    uint32_t exifIfdOff = 0;

    auto scanIfd = [&](uint32_t ifdOffset, bool isExifSub) {
        if (ifdOffset == 0 || ifdOffset + 2 > static_cast<uint32_t>(tiff.size())) return;
        const uint16_t count = readTiffU16(tiff, static_cast<int>(ifdOffset), le);
        for (uint16_t i = 0; i < count; ++i) {
            const int ent = static_cast<int>(ifdOffset) + 2 + i * 12;
            if (ent + 12 > tiff.size()) break;
            const uint16_t tag = readTiffU16(tiff, ent, le);
            uint16_t type = 0;
            uint32_t cnt = 0;
            const QByteArray raw = readTiffTagRawBytes(tiff, ent, le, type, cnt);

            if (!isExifSub) {
                if (tag == 0x010F) make = QString::fromUtf8(raw).remove(QChar('\0')).trimmed();
                else if (tag == 0x0110) model = QString::fromUtf8(raw).remove(QChar('\0')).trimmed();
                else if (tag == 0x0131) meta.software = QString::fromUtf8(raw).remove(QChar('\0')).trimmed();
                else if (tag == 0x010E) {
                    const QString desc = QString::fromUtf8(raw).remove(QChar('\0')).trimmed();
                    if (!desc.isEmpty()) {
                        extraLines << QStringLiteral("Описание: ") + desc;
                        meta.hasAiMetadata = true;
                    }
                } else if (tag == 0x8769 && cnt >= 1) {
                    exifIfdOff = readTiffU32(tiff, ent + 8, le);
                }
            } else {
                if (tag == 0x829A && type == 5 && cnt >= 1 && raw.size() >= 8) {
                    const double val = readTiffRational(raw, 0, le);
                    if (val > 0.0) {
                        meta.shutterSpeed = (val < 1.0)
                                                ? QString("1/%1 s").arg(qRound(1.0 / val))
                                                : QString("%1 s").arg(val, 0, 'f', 1);
                    }
                } else if (tag == 0x829D && type == 5 && cnt >= 1 && raw.size() >= 8) {
                    const double val = readTiffRational(raw, 0, le);
                    if (val > 0.0) meta.aperture = QString("f/%1").arg(val, 0, 'f', 1);
                } else if (tag == 0x8827 && cnt >= 1) {
                    meta.iso = (type == 3) ? readTiffU16(tiff, ent + 8, le) : static_cast<int>(readTiffU32(tiff, ent + 8, le));
                } else if (tag == 0x920A && type == 5 && cnt >= 1 && raw.size() >= 8) {
                    const double val = readTiffRational(raw, 0, le);
                    if (val > 0.0) meta.focalLength = QString("%1 mm").arg(qRound(val));
                } else if (tag == 0xA434) {
                    meta.lens = QString::fromUtf8(raw).remove(QChar('\0')).trimmed();
                } else if (tag == 0x9286) {
                    const QString uc = decodeExifUserComment(raw);
                    if (!uc.isEmpty()) {
                        extraLines << QStringLiteral("Parameters / UserComment:\n") + uc;
                        meta.hasAiMetadata = true;
                    }
                }
            }
        }
    };

    scanIfd(ifd0Off, false);
    if (exifIfdOff > 0) scanIfd(exifIfdOff, true);

    if (!make.isEmpty() || !model.isEmpty()) {
        if (!model.isEmpty() && model.startsWith(make, Qt::CaseInsensitive)) {
            meta.camera = model;
        } else {
            meta.camera = (make + (make.isEmpty() || model.isEmpty() ? "" : " ") + model).trimmed();
        }
    }
    if (!meta.software.isEmpty()) {
        extraLines << QStringLiteral("Программа / ПО: ") + meta.software;
    }
}

QString stripXmlTagsFast(const QString& input) {
    QString out;
    out.reserve(input.size());
    bool inTag = false;
    for (QChar ch : input) {
        if (ch == QLatin1Char('<')) {
            inTag = true;
            continue;
        }
        if (ch == QLatin1Char('>')) {
            inTag = false;
            out.append(QLatin1Char(' '));
            continue;
        }
        if (!inTag) out.append(ch);
    }
    return out.simplified();
}

void extractXmpSummary(const QByteArray& xmpBytes, ExifMetadata& meta, QStringList& extraLines) {
    // Cap parsed slice cleanly to keep metadata inspection sub-millisecond even on huge Adobe/AI XMP blobs
    const QString xmp = QString::fromUtf8(xmpBytes.left(48 * 1024)).trimmed();
    if (xmp.isEmpty()) return;

    auto extractTag = [&](const QString& tag) -> QString {
        const QString openStart = QLatin1Char('<') + tag;
        const QString closeTag = QStringLiteral("</") + tag + QLatin1Char('>');
        const int s0 = xmp.indexOf(openStart);
        if (s0 < 0) return QString();
        const int gt = xmp.indexOf(QLatin1Char('>'), s0 + openStart.size());
        if (gt < 0) return QString();
        if (gt > 0 && xmp.at(gt - 1) == QLatin1Char('/')) return QString();
        const int e0 = xmp.indexOf(closeTag, gt + 1);
        if (e0 < 0) return QString();
        return stripXmlTagsFast(xmp.mid(gt + 1, e0 - (gt + 1)));
    };

    const QString desc = extractTag(QStringLiteral("dc:description"));
    const QString creator = extractTag(QStringLiteral("xmp:CreatorTool"));
    const QString credit = extractTag(QStringLiteral("photoshop:Credit"));
    const QString source = extractTag(QStringLiteral("photoshop:Source"));

    if (!creator.isEmpty() && meta.software.isEmpty()) {
        meta.software = creator;
        extraLines << QStringLiteral("ПО (XMP): ") + creator;
    }
    if (!desc.isEmpty()) {
        extraLines << QStringLiteral("XMP Описание:\n") + desc;
        meta.hasAiMetadata = true;
    }
    if (!credit.isEmpty()) {
        extraLines << QStringLiteral("XMP Credit: ") + credit;
        meta.hasAiMetadata = true;
    }
    if (!source.isEmpty()) {
        extraLines << QStringLiteral("XMP Source: ") + source;
        meta.hasAiMetadata = true;
    }
    if (desc.isEmpty() && credit.isEmpty() && source.isEmpty() && xmp.size() < 4000) {
        extraLines << QStringLiteral("XMP:\n") + xmp;
        meta.hasAiMetadata = true;
    }
}

QByteArray uncompressPngZlib(const QByteArray& compressed) {
    if (compressed.isEmpty()) return QByteArray();
    for (uint32_t mult : {8u, 24u, 64u}) {
        const uint32_t guess = std::clamp<uint32_t>(static_cast<uint32_t>(compressed.size()) * mult, 4096u, 8u * 1024u * 1024u);
        QByteArray wrapped(4 + compressed.size(), '\0');
        qToBigEndian<uint32_t>(guess, reinterpret_cast<uchar*>(wrapped.data()));
        std::memcpy(wrapped.data() + 4, compressed.constData(), static_cast<size_t>(compressed.size()));
        const QByteArray out = qUncompress(wrapped);
        if (!out.isEmpty()) return out;
    }
    return QByteArray();
}

// Fast PNG metadata reader that seeks directly past IDAT pixel chunks without ever invoking libpng decompression!
void scanPngMetadataFast(const QString& filePath,
                         ExifMetadata* metaOut,
                         QStringList* extraLinesOut,
                         QList<QPair<QString, QString>>* textEntriesOut,
                         QList<QByteArray>* rawMetaChunksOut,
                         QByteArray* exifTiffOut,
                         QByteArray* xmpOut) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return;
    if (f.read(8) != QByteArray("\x89PNG\r\n\x1a\n", 8)) return;

    while (!f.atEnd()) {
        const QByteArray lenBytes = f.read(4);
        const QByteArray typeBytes = f.read(4);
        if (lenBytes.size() != 4 || typeBytes.size() != 4) break;
        const uint32_t len = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar*>(lenBytes.constData()));
        if (len > 32u * 1024u * 1024u) break;

        if (typeBytes == "IDAT" || typeBytes == "IHDR" || typeBytes == "PLTE" ||
            typeBytes == "gAMA" || typeBytes == "sRGB" || typeBytes == "pHYs") {
            f.seek(f.pos() + static_cast<qint64>(len) + 4);
            continue;
        }
        if (typeBytes == "IEND") {
            break;
        }

        if (typeBytes == "eXIf" || typeBytes == "tEXt" || typeBytes == "zTXt" || typeBytes == "iTXt") {
            const QByteArray dataAndCrc = f.read(static_cast<qint64>(len) + 4);
            if (dataAndCrc.size() != static_cast<qint64>(len) + 4) break;
            const QByteArray payload = dataAndCrc.left(static_cast<int>(len));

            if (typeBytes == "eXIf") {
                if (metaOut && extraLinesOut) {
                    parseTiffBlock(payload, *metaOut, *extraLinesOut);
                }
                if (exifTiffOut && exifTiffOut->isEmpty()) {
                    *exifTiffOut = payload;
                }
                if (rawMetaChunksOut) {
                    QByteArray chunk;
                    chunk.reserve(8 + dataAndCrc.size());
                    chunk.append(lenBytes);
                    chunk.append(typeBytes);
                    chunk.append(dataAndCrc);
                    rawMetaChunksOut->append(chunk);
                }
                continue;
            }

            if (rawMetaChunksOut) {
                QByteArray chunk;
                chunk.reserve(8 + dataAndCrc.size());
                chunk.append(lenBytes);
                chunk.append(typeBytes);
                chunk.append(dataAndCrc);
                rawMetaChunksOut->append(chunk);
            }

            QString key;
            QString val;
            if (typeBytes == "tEXt") {
                const int nul = payload.indexOf('\0');
                if (nul > 0) {
                    key = QString::fromLatin1(payload.constData(), nul);
                    val = QString::fromLatin1(payload.constData() + nul + 1, payload.size() - nul - 1).trimmed();
                }
            } else if (typeBytes == "zTXt") {
                const int nul = payload.indexOf('\0');
                if (nul > 0 && nul + 2 < payload.size() && payload[nul + 1] == '\0') {
                    key = QString::fromLatin1(payload.constData(), nul);
                    const QByteArray dec = uncompressPngZlib(payload.mid(nul + 2));
                    if (!dec.isEmpty()) {
                        val = QString::fromUtf8(dec).trimmed();
                    }
                }
            } else if (typeBytes == "iTXt") {
                const int nul1 = payload.indexOf('\0');
                if (nul1 > 0 && nul1 + 3 < payload.size()) {
                    key = QString::fromLatin1(payload.constData(), nul1);
                    const bool compFlag = (payload[nul1 + 1] != '\0');
                    const int nul2 = payload.indexOf('\0', nul1 + 3); // end of language tag
                    const int nul3 = (nul2 >= 0) ? payload.indexOf('\0', nul2 + 1) : -1; // end of translated keyword
                    if (nul3 >= 0 && nul3 + 1 <= payload.size()) {
                        const QByteArray rawText = payload.mid(nul3 + 1);
                        const QByteArray utf8 = compFlag ? uncompressPngZlib(rawText) : rawText;
                        if (!utf8.isEmpty()) {
                            val = QString::fromUtf8(utf8).trimmed();
                        }
                    }
                }
            }

            if (!key.isEmpty() && !val.isEmpty()) {
                if (textEntriesOut) {
                    textEntriesOut->append({key, val});
                }
                if (xmpOut && xmpOut->isEmpty() &&
                    key.compare(QStringLiteral("XML:com.adobe.xmp"), Qt::CaseInsensitive) == 0) {
                    *xmpOut = val.toUtf8();
                }
                if (metaOut && extraLinesOut) {
                    if (key.compare(QStringLiteral("XML:com.adobe.xmp"), Qt::CaseInsensitive) == 0) {
                        extractXmpSummary(val.toUtf8(), *metaOut, *extraLinesOut);
                    } else {
                        if (key.compare(QStringLiteral("Software"), Qt::CaseInsensitive) == 0 && metaOut->software.isEmpty()) {
                            metaOut->software = val;
                        }
                        extraLinesOut->append(QString("[%1]\n%2").arg(key, val));
                        metaOut->hasAiMetadata = true;
                    }
                }
            }
        } else {
            f.seek(f.pos() + static_cast<qint64>(len) + 4);
        }
    }
}

void scanJpegMetadata(const QString& filePath, ExifMetadata& meta, QStringList& extraLines) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return;
    const QByteArray soi = f.read(2);
    if (soi.size() != 2 || static_cast<uchar>(soi[0]) != 0xFF || static_cast<uchar>(soi[1]) != 0xD8) return;

    while (!f.atEnd()) {
        uchar b = 0;
        if (f.read(reinterpret_cast<char*>(&b), 1) != 1) break;
        if (b != 0xFF) continue;
        while (b == 0xFF) {
            if (f.read(reinterpret_cast<char*>(&b), 1) != 1) return;
        }
        if (b == 0xD9 || b == 0xDA) break; // EOI or SOS (image stream starts)
        if (b == 0x00 || (b >= 0xD0 && b <= 0xD8)) continue;

        const QByteArray lenBytes = f.read(2);
        if (lenBytes.size() != 2) break;
        const uint16_t segLen = qFromBigEndian<uint16_t>(reinterpret_cast<const uchar*>(lenBytes.constData()));
        if (segLen < 2) break;
        const int payloadLen = segLen - 2;

        if (b == 0xE1) { // APP1 (EXIF or XMP)
            const QByteArray payload = f.read(payloadLen);
            if (payload.startsWith(QByteArray("Exif\0\0", 6))) {
                parseTiffBlock(payload.mid(6), meta, extraLines);
            } else if (payload.startsWith(QByteArray("http://ns.adobe.com/xap/1.0/\0", 29))) {
                extractXmpSummary(payload.mid(29), meta, extraLines);
            }
        } else if (b == 0xFE) { // COM comment
            const QByteArray comment = f.read(payloadLen);
            const QString txt = QString::fromUtf8(comment).remove(QChar('\0')).trimmed();
            if (!txt.isEmpty()) {
                extraLines << QStringLiteral("JPEG Comment:\n") + txt;
                meta.hasAiMetadata = true;
            }
        } else {
            f.seek(f.pos() + payloadLen);
        }
    }
}

void scanWebpMetadata(const QString& filePath, ExifMetadata& meta, QStringList& extraLines) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return;
    const QByteArray hdr = f.read(12);
    if (hdr.size() != 12 || hdr.mid(0, 4) != "RIFF" || hdr.mid(8, 4) != "WEBP") return;

    while (!f.atEnd()) {
        const QByteArray chId = f.read(4);
        const QByteArray chSizeBytes = f.read(4);
        if (chId.size() != 4 || chSizeBytes.size() != 4) break;
        const uint32_t sz = qFromLittleEndian<uint32_t>(reinterpret_cast<const uchar*>(chSizeBytes.constData()));
        if (sz > 64u * 1024u * 1024u) break;
        const qint64 padded = sz + (sz & 1u);
        if (chId == "EXIF") {
            QByteArray payload = f.read(sz);
            if (payload.startsWith(QByteArray("Exif\0\0", 6))) payload = payload.mid(6);
            parseTiffBlock(payload, meta, extraLines);
            if (sz & 1u) f.seek(f.pos() + 1);
        } else if (chId == "XMP ") {
            const QByteArray payload = f.read(sz);
            extractXmpSummary(payload, meta, extraLines);
            if (sz & 1u) f.seek(f.pos() + 1);
        } else {
            f.seek(f.pos() + padded);
        }
    }
}

bool stripPngMetadataLossless(const QString& filePath, QString* errorOut) {
    QFile in(filePath);
    if (!in.open(QIODevice::ReadOnly)) {
        if (errorOut) *errorOut = QStringLiteral("Не удалось открыть PNG для чтения.");
        return false;
    }
    const QByteArray sig = in.read(8);
    if (sig != QByteArray("\x89PNG\r\n\x1a\n", 8)) return false;

    QSaveFile out(filePath);
    if (!out.open(QIODevice::WriteOnly)) {
        if (errorOut) *errorOut = QStringLiteral("Не удалось открыть файл для записи.");
        return false;
    }
    out.write(sig);

    while (!in.atEnd()) {
        const QByteArray lenBytes = in.read(4);
        const QByteArray typeBytes = in.read(4);
        if (lenBytes.size() != 4 || typeBytes.size() != 4) break;
        const uint32_t len = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar*>(lenBytes.constData()));
        const QByteArray dataAndCrc = in.read(static_cast<qint64>(len) + 4);
        if (dataAndCrc.size() != static_cast<qint64>(len) + 4) {
            if (errorOut) *errorOut = QStringLiteral("Повреждённый PNG-чанк.");
            return false;
        }
        const bool drop = (typeBytes == "tEXt" || typeBytes == "zTXt" || typeBytes == "iTXt" ||
                           typeBytes == "eXIf" || typeBytes == "tIME");
        if (!drop) {
            out.write(lenBytes);
            out.write(typeBytes);
            out.write(dataAndCrc);
        }
        if (typeBytes == "IEND") break;
    }

    if (!out.commit()) {
        if (errorOut) *errorOut = out.errorString();
        return false;
    }
    return true;
}

bool stripJpegMetadataLossless(const QString& filePath, QString* errorOut) {
    QFile in(filePath);
    if (!in.open(QIODevice::ReadOnly)) {
        if (errorOut) *errorOut = QStringLiteral("Не удалось открыть JPEG для чтения.");
        return false;
    }
    const QByteArray all = in.readAll();
    in.close();
    if (all.size() < 4 || static_cast<uchar>(all[0]) != 0xFF || static_cast<uchar>(all[1]) != 0xD8) {
        return false;
    }

    QByteArray clean;
    clean.reserve(all.size());
    clean.append(all.constData(), 2); // SOI

    int pos = 2;
    while (pos < all.size()) {
        if (static_cast<uchar>(all[pos]) != 0xFF) {
            clean.append(all.constData() + pos, all.size() - pos);
            break;
        }
        int markerStart = pos;
        while (pos < all.size() && static_cast<uchar>(all[pos]) == 0xFF) ++pos;
        if (pos >= all.size()) break;
        const uchar marker = static_cast<uchar>(all[pos]);
        ++pos;

        if (marker == 0xDA || marker == 0xD9) {
            clean.append(all.constData() + markerStart, all.size() - markerStart);
            break;
        }
        if (marker == 0x00 || (marker >= 0xD0 && marker <= 0xD8)) {
            clean.append(all.constData() + markerStart, pos - markerStart);
            continue;
        }
        if (pos + 2 > all.size()) break;
        const uint16_t segLen = qFromBigEndian<uint16_t>(reinterpret_cast<const uchar*>(all.constData() + pos));
        const int segEnd = pos + segLen;
        if (segLen < 2 || segEnd > all.size()) break;

        // Strip APP1 (EXIF/XMP), APP12, APP13 (IPTC), COM (0xFE)
        const bool drop = (marker == 0xE1 || marker == 0xEC || marker == 0xED || marker == 0xFE);
        if (!drop) {
            clean.append(all.constData() + markerStart, segEnd - markerStart);
        }
        pos = segEnd;
    }

    QSaveFile out(filePath);
    if (!out.open(QIODevice::WriteOnly)) {
        if (errorOut) *errorOut = QStringLiteral("Не удалось записать очищенный JPEG.");
        return false;
    }
    out.write(clean);
    if (!out.commit()) {
        if (errorOut) *errorOut = out.errorString();
        return false;
    }
    return true;
}

bool stripWebpMetadataLossless(const QString& filePath, QString* errorOut) {
    QFile in(filePath);
    if (!in.open(QIODevice::ReadOnly)) return false;
    const QByteArray hdr = in.read(12);
    if (hdr.size() != 12 || hdr.mid(0, 4) != "RIFF" || hdr.mid(8, 4) != "WEBP") return false;

    QByteArray chunks;
    while (!in.atEnd()) {
        const QByteArray chId = in.read(4);
        const QByteArray szBytes = in.read(4);
        if (chId.size() != 4 || szBytes.size() != 4) break;
        const uint32_t sz = qFromLittleEndian<uint32_t>(reinterpret_cast<const uchar*>(szBytes.constData()));
        if (sz > 64u * 1024u * 1024u) return false;
        const qint64 padded = sz + (sz & 1u);
        QByteArray payload = in.read(padded);
        if (chId == "EXIF" || chId == "XMP ") {
            continue;
        }
        if (chId == "VP8X" && payload.size() >= 1) {
            // Clear EXIF (bit 3: 0x08) and XMP (bit 2: 0x04) feature flags
            payload[0] = static_cast<char>(static_cast<uchar>(payload[0]) & ~0x0Cu);
        }
        chunks.append(chId);
        chunks.append(szBytes);
        chunks.append(payload);
    }
    in.close();

    QByteArray outData;
    outData.append("RIFF");
    const uint32_t totalSize = 4 + static_cast<uint32_t>(chunks.size());
    char szBuf[4];
    qToLittleEndian<uint32_t>(totalSize, reinterpret_cast<uchar*>(szBuf));
    outData.append(szBuf, 4);
    outData.append("WEBP");
    outData.append(chunks);

    QSaveFile out(filePath);
    if (!out.open(QIODevice::WriteOnly)) {
        if (errorOut) *errorOut = QStringLiteral("Не удалось записать очищенный WebP.");
        return false;
    }
    out.write(outData);
    if (!out.commit()) {
        if (errorOut) *errorOut = out.errorString();
        return false;
    }
    return true;
}

uint32_t pngCrc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

QByteArray buildPngChunk(const QByteArray& type4, const QByteArray& payload) {
    QByteArray chunk;
    chunk.reserve(12 + payload.size());
    char buf[4];
    qToBigEndian<uint32_t>(static_cast<uint32_t>(payload.size()), reinterpret_cast<uchar*>(buf));
    chunk.append(buf, 4);
    chunk.append(type4.left(4));
    chunk.append(payload);
    const uint32_t crc = pngCrc32(reinterpret_cast<const uint8_t*>(chunk.constData() + 4),
                                  static_cast<size_t>(4 + payload.size()));
    qToBigEndian<uint32_t>(crc, reinterpret_cast<uchar*>(buf));
    chunk.append(buf, 4);
    return chunk;
}

// Force EXIF Orientation tag (0x0112) = 1 so already auto-rotated pixels do not double-rotate on export
void normalizeTiffOrientationTo1(QByteArray& tiff) {
    if (tiff.size() < 8) return;
    bool le = false;
    if (tiff[0] == 'I' && tiff[1] == 'I') le = true;
    else if (tiff[0] == 'M' && tiff[1] == 'M') le = false;
    else return;

    if (readTiffU16(tiff, 2, le) != 42) return;
    const uint32_t ifd0Off = readTiffU32(tiff, 4, le);
    if (ifd0Off == 0 || ifd0Off + 2 > static_cast<uint32_t>(tiff.size())) return;

    const uint16_t count = readTiffU16(tiff, static_cast<int>(ifd0Off), le);
    for (uint16_t i = 0; i < count; ++i) {
        const int ent = static_cast<int>(ifd0Off) + 2 + i * 12;
        if (ent + 12 > tiff.size()) break;
        const uint16_t tag = readTiffU16(tiff, ent, le);
        if (tag == 0x0112) {
            auto* ptr = reinterpret_cast<uchar*>(tiff.data() + ent + 8);
            if (le) qToLittleEndian<uint16_t>(1, ptr);
            else qToBigEndian<uint16_t>(1, ptr);
            break;
        }
    }
}

struct ExportMetadataBlobs {
    QList<QPair<QString, QString>> pngTextEntries;
    QList<QByteArray> rawPngMetaChunks; // already full length+type+payload+crc chunks
    QByteArray rawExifTiff;             // raw TIFF header ("II..." / "MM...") without "Exif\0\0"
    QByteArray rawXmpUtf8;
    QByteArray rawIptc;                 // APP13 payload
    QByteArray rawJpegComment;          // COM payload
};

ExportMetadataBlobs collectSourceMetadataBlobs(const QString& sourcePath, const ExifMetadata* fallbackMeta) {
    ExportMetadataBlobs blobs;
    if (sourcePath.isEmpty()) return blobs;

    QFile f(sourcePath);
    if (f.open(QIODevice::ReadOnly)) {
        const QByteArray head = f.read(12);
        if (head.startsWith("\x89PNG\r\n\x1a\n")) {
            f.close();
            scanPngMetadataFast(sourcePath, nullptr, nullptr, &blobs.pngTextEntries, &blobs.rawPngMetaChunks, &blobs.rawExifTiff, &blobs.rawXmpUtf8);
            if (!blobs.rawExifTiff.isEmpty()) {
                normalizeTiffOrientationTo1(blobs.rawExifTiff);
                for (QByteArray& ch : blobs.rawPngMetaChunks) {
                    if (ch.size() >= 8 && ch.mid(4, 4) == "eXIf") {
                        ch = buildPngChunk("eXIf", blobs.rawExifTiff);
                    }
                }
            }
        } else if (head.size() >= 2 && static_cast<uchar>(head[0]) == 0xFF && static_cast<uchar>(head[1]) == 0xD8) {
            f.seek(2);
            while (!f.atEnd()) {
                uchar b = 0;
                if (f.read(reinterpret_cast<char*>(&b), 1) != 1) break;
                if (b != 0xFF) continue;
                while (b == 0xFF) {
                    if (f.read(reinterpret_cast<char*>(&b), 1) != 1) break;
                }
                if (b == 0xD9 || b == 0xDA) break;
                if (b == 0x00 || (b >= 0xD0 && b <= 0xD8)) continue;
                const QByteArray lenBytes = f.read(2);
                if (lenBytes.size() != 2) break;
                const uint16_t segLen = qFromBigEndian<uint16_t>(reinterpret_cast<const uchar*>(lenBytes.constData()));
                if (segLen < 2) break;
                const QByteArray payload = f.read(segLen - 2);
                if (b == 0xE1) {
                    if (payload.startsWith(QByteArray("Exif\0\0", 6)) && blobs.rawExifTiff.isEmpty()) {
                        blobs.rawExifTiff = payload.mid(6);
                        normalizeTiffOrientationTo1(blobs.rawExifTiff);
                    } else if (payload.startsWith(QByteArray("http://ns.adobe.com/xap/1.0/\0", 29)) && blobs.rawXmpUtf8.isEmpty()) {
                        blobs.rawXmpUtf8 = payload.mid(29);
                    }
                } else if (b == 0xED && blobs.rawIptc.isEmpty()) {
                    blobs.rawIptc = payload;
                } else if (b == 0xFE && blobs.rawJpegComment.isEmpty()) {
                    blobs.rawJpegComment = payload;
                }
            }
        } else if (head.startsWith("RIFF") && head.mid(8, 4) == "WEBP") {
            f.seek(12);
            while (!f.atEnd()) {
                const QByteArray chId = f.read(4);
                const QByteArray chSizeBytes = f.read(4);
                if (chId.size() != 4 || chSizeBytes.size() != 4) break;
                const uint32_t sz = qFromLittleEndian<uint32_t>(reinterpret_cast<const uchar*>(chSizeBytes.constData()));
                if (sz > 64u * 1024u * 1024u) break;
                const QByteArray payload = f.read(sz);
                if (sz & 1u) f.seek(f.pos() + 1);
                if (chId == "EXIF" && blobs.rawExifTiff.isEmpty()) {
                    blobs.rawExifTiff = payload.startsWith(QByteArray("Exif\0\0", 6)) ? payload.mid(6) : payload;
                    normalizeTiffOrientationTo1(blobs.rawExifTiff);
                } else if (chId == "XMP " && blobs.rawXmpUtf8.isEmpty()) {
                    blobs.rawXmpUtf8 = payload;
                }
            }
        }
        f.close();
    }

    if (blobs.rawXmpUtf8.isEmpty() && blobs.rawJpegComment.isEmpty() && blobs.pngTextEntries.isEmpty() && fallbackMeta) {
        QString fallbackDesc = fallbackMeta->rawMetadataText.trimmed();
        if (fallbackDesc.isEmpty() && (!fallbackMeta->camera.isEmpty() || !fallbackMeta->software.isEmpty())) {
            QStringList parts;
            if (!fallbackMeta->camera.isEmpty()) parts << QStringLiteral("Camera: ") + fallbackMeta->camera;
            if (!fallbackMeta->software.isEmpty()) parts << QStringLiteral("Software: ") + fallbackMeta->software;
            fallbackDesc = parts.join(QStringLiteral("\n"));
        }
        if (!fallbackDesc.isEmpty()) {
            blobs.pngTextEntries.append({QStringLiteral("Description"), fallbackDesc});
            blobs.rawJpegComment = fallbackDesc.toUtf8();
        }
    }

    if (blobs.rawJpegComment.isEmpty() && !blobs.pngTextEntries.isEmpty()) {
        QStringList parts;
        for (const auto& kv : blobs.pngTextEntries) {
            if (kv.first.compare(QStringLiteral("XML:com.adobe.xmp"), Qt::CaseInsensitive) == 0) continue;
            if (blobs.pngTextEntries.size() == 1 && kv.first.compare(QStringLiteral("parameters"), Qt::CaseInsensitive) == 0) {
                parts << kv.second;
            } else {
                parts << QString("%1: %2").arg(kv.first, kv.second);
            }
        }
        if (!parts.isEmpty()) {
            blobs.rawJpegComment = parts.join(QStringLiteral("\n")).toUtf8();
        }
    }

    if (blobs.rawXmpUtf8.isEmpty() && !blobs.rawJpegComment.isEmpty()) {
        QString escaped = QString::fromUtf8(blobs.rawJpegComment);
        escaped.replace('&', QStringLiteral("&amp;"));
        escaped.replace('<', QStringLiteral("&lt;"));
        escaped.replace('>', QStringLiteral("&gt;"));
        const QString xmp = QStringLiteral(
            "<?xpacket begin=\"\xEF\xBB\xBF\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>"
            "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">"
            "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">"
            "<rdf:Description rdf:about=\"\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
            "<dc:description>%1</dc:description>"
            "</rdf:Description></rdf:RDF></x:xmpmeta><?xpacket end=\"w\"?>").arg(escaped);
        blobs.rawXmpUtf8 = xmp.toUtf8();
    }

    return blobs;
}

QByteArray injectPngMetadata(const QByteArray& encodedPng, const ExportMetadataBlobs& blobs) {
    if (!encodedPng.startsWith("\x89PNG\r\n\x1a\n")) return encodedPng;

    QByteArray out;
    out.reserve(encodedPng.size() + 4096);
    out.append(encodedPng.constData(), 8);

    int pos = 8;
    bool inserted = false;
    while (pos + 12 <= encodedPng.size()) {
        const uint32_t len = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar*>(encodedPng.constData() + pos));
        const QByteArray type4 = encodedPng.mid(pos + 4, 4);
        const int totalChunkLen = 12 + static_cast<int>(len);
        if (pos + totalChunkLen > encodedPng.size()) break;

        if (!inserted && type4 == "IEND") {
            if (!blobs.rawPngMetaChunks.isEmpty()) {
                for (const QByteArray& ch : blobs.rawPngMetaChunks) {
                    out.append(ch);
                }
            } else {
                if (!blobs.rawExifTiff.isEmpty()) {
                    out.append(buildPngChunk("eXIf", blobs.rawExifTiff));
                }
                if (!blobs.rawXmpUtf8.isEmpty()) {
                    QByteArray itxt;
                    itxt.append("XML:com.adobe.xmp\0", 18);
                    itxt.append('\0'); // compression flag
                    itxt.append('\0'); // compression method
                    itxt.append('\0'); // language tag
                    itxt.append('\0'); // translated keyword
                    itxt.append(blobs.rawXmpUtf8);
                    out.append(buildPngChunk("iTXt", itxt));
                }
                if (!blobs.rawJpegComment.isEmpty()) {
                    QByteArray itxt;
                    itxt.append("Description\0", 12);
                    itxt.append('\0');
                    itxt.append('\0');
                    itxt.append('\0');
                    itxt.append('\0');
                    itxt.append(blobs.rawJpegComment);
                    out.append(buildPngChunk("iTXt", itxt));
                }
            }
            inserted = true;
        }

        // Skip Qt's minimal text chunks if we are re-injecting the full original PNG chunks cleanly
        const bool isQtText = (!blobs.rawPngMetaChunks.isEmpty() &&
                               (type4 == "tEXt" || type4 == "iTXt" || type4 == "zTXt" || type4 == "eXIf"));
        if (!isQtText) {
            out.append(encodedPng.constData() + pos, totalChunkLen);
        }
        pos += totalChunkLen;
    }
    return out;
}

QByteArray injectJpegMetadata(const QByteArray& encodedJpg, const ExportMetadataBlobs& blobs) {
    if (encodedJpg.size() < 4 ||
        static_cast<uchar>(encodedJpg[0]) != 0xFF ||
        static_cast<uchar>(encodedJpg[1]) != 0xD8) {
        return encodedJpg;
    }

    auto makeSeg = [](uchar marker, const QByteArray& payload) -> QByteArray {
        const QByteArray clipped = payload.left(65531);
        QByteArray seg;
        seg.reserve(4 + clipped.size());
        seg.append(static_cast<char>(0xFF));
        seg.append(static_cast<char>(marker));
        char lenBuf[2];
        qToBigEndian<uint16_t>(static_cast<uint16_t>(clipped.size() + 2), reinterpret_cast<uchar*>(lenBuf));
        seg.append(lenBuf, 2);
        seg.append(clipped);
        return seg;
    };

    QByteArray out;
    out.reserve(encodedJpg.size() + 8192);
    out.append(encodedJpg.constData(), 2); // SOI

    if (!blobs.rawExifTiff.isEmpty()) {
        QByteArray app1("Exif\0\0", 6);
        app1.append(blobs.rawExifTiff);
        out.append(makeSeg(0xE1, app1));
    }
    if (!blobs.rawXmpUtf8.isEmpty()) {
        QByteArray app1("http://ns.adobe.com/xap/1.0/\0", 29);
        app1.append(blobs.rawXmpUtf8);
        out.append(makeSeg(0xE1, app1));
    }
    if (!blobs.rawIptc.isEmpty()) {
        out.append(makeSeg(0xED, blobs.rawIptc));
    }
    if (!blobs.rawJpegComment.isEmpty()) {
        out.append(makeSeg(0xFE, blobs.rawJpegComment));
    }

    out.append(encodedJpg.constData() + 2, encodedJpg.size() - 2);
    return out;
}

QByteArray injectWebpMetadata(const QByteArray& encodedWebp, const ExportMetadataBlobs& blobs, int imgW, int imgH, bool hasAlpha) {
    if (encodedWebp.size() < 12 || encodedWebp.mid(0, 4) != "RIFF" || encodedWebp.mid(8, 4) != "WEBP") {
        return encodedWebp;
    }
    if (blobs.rawExifTiff.isEmpty() && blobs.rawXmpUtf8.isEmpty()) {
        return encodedWebp;
    }

    auto appendChunk = [](QByteArray& dst, const char id[4], const QByteArray& payload) {
        dst.append(id, 4);
        char szBuf[4];
        qToLittleEndian<uint32_t>(static_cast<uint32_t>(payload.size()), reinterpret_cast<uchar*>(szBuf));
        dst.append(szBuf, 4);
        dst.append(payload);
        if (payload.size() & 1) {
            dst.append('\0');
        }
    };

    QByteArray bodyChunks;
    bool hasVp8x = false;
    int pos = 12;
    while (pos + 8 <= encodedWebp.size()) {
        const QByteArray chId = encodedWebp.mid(pos, 4);
        const uint32_t sz = qFromLittleEndian<uint32_t>(reinterpret_cast<const uchar*>(encodedWebp.constData() + pos + 4));
        const int padded = static_cast<int>(sz + (sz & 1u));
        if (pos + 8 + padded > encodedWebp.size()) break;
        QByteArray payload = encodedWebp.mid(pos + 8, static_cast<int>(sz));
        if (chId == "VP8X" && payload.size() >= 10) {
            hasVp8x = true;
            uchar flags = static_cast<uchar>(payload[0]);
            if (!blobs.rawExifTiff.isEmpty()) flags |= 0x08u;
            if (!blobs.rawXmpUtf8.isEmpty()) flags |= 0x04u;
            payload[0] = static_cast<char>(flags);
            appendChunk(bodyChunks, "VP8X", payload);
        } else if (chId != "EXIF" && chId != "XMP ") {
            appendChunk(bodyChunks, chId.constData(), payload);
        }
        pos += 8 + padded;
    }

    QByteArray finalChunks;
    if (!hasVp8x && imgW > 0 && imgH > 0) {
        QByteArray vp8x(10, '\0');
        uchar flags = 0;
        if (hasAlpha) flags |= 0x10u;
        if (!blobs.rawExifTiff.isEmpty()) flags |= 0x08u;
        if (!blobs.rawXmpUtf8.isEmpty()) flags |= 0x04u;
        vp8x[0] = static_cast<char>(flags);
        const uint32_t wm1 = static_cast<uint32_t>(std::max(0, imgW - 1));
        const uint32_t hm1 = static_cast<uint32_t>(std::max(0, imgH - 1));
        vp8x[4] = static_cast<char>(wm1 & 0xFFu);
        vp8x[5] = static_cast<char>((wm1 >> 8) & 0xFFu);
        vp8x[6] = static_cast<char>((wm1 >> 16) & 0xFFu);
        vp8x[7] = static_cast<char>(hm1 & 0xFFu);
        vp8x[8] = static_cast<char>((hm1 >> 8) & 0xFFu);
        vp8x[9] = static_cast<char>((hm1 >> 16) & 0xFFu);
        appendChunk(finalChunks, "VP8X", vp8x);
    }
    finalChunks.append(bodyChunks);

    if (!blobs.rawExifTiff.isEmpty()) {
        appendChunk(finalChunks, "EXIF", blobs.rawExifTiff);
    }
    if (!blobs.rawXmpUtf8.isEmpty()) {
        appendChunk(finalChunks, "XMP ", blobs.rawXmpUtf8);
    }

    QByteArray out;
    out.reserve(12 + finalChunks.size());
    out.append("RIFF", 4);
    char totalBuf[4];
    qToLittleEndian<uint32_t>(static_cast<uint32_t>(4 + finalChunks.size()), reinterpret_cast<uchar*>(totalBuf));
    out.append(totalBuf, 4);
    out.append("WEBP", 4);
    out.append(finalChunks);
    return out;
}

#ifdef _WIN32
template <typename T>
struct ComPtr {
    T* ptr = nullptr;
    ~ComPtr() {
        if (ptr) ptr->Release();
    }
    T** put() { return &ptr; }
    T* operator->() const { return ptr; }
    explicit operator bool() const { return ptr != nullptr; }
};

QImage loadViaWindowsWic(const QString& filePath) {
    const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninit = SUCCEEDED(hrCo);

    QImage result;
    {
        ComPtr<IWICImagingFactory> factory;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(factory.put())))) {
            if (shouldUninit) CoUninitialize();
            return result;
        }

        ComPtr<IWICBitmapDecoder> decoder;
        if (FAILED(factory->CreateDecoderFromFilename(
                filePath.toStdWString().c_str(), nullptr, GENERIC_READ,
                WICDecodeMetadataCacheOnDemand, decoder.put()))) {
            if (shouldUninit) CoUninitialize();
            return result;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        if (FAILED(decoder->GetFrame(0, frame.put()))) {
            if (shouldUninit) CoUninitialize();
            return result;
        }

        ComPtr<IWICFormatConverter> converter;
        if (FAILED(factory->CreateFormatConverter(converter.put()))) {
            if (shouldUninit) CoUninitialize();
            return result;
        }

        if (FAILED(converter->Initialize(frame.ptr, GUID_WICPixelFormat32bppRGBA,
                                         WICBitmapDitherTypeNone, nullptr, 0.0,
                                         WICBitmapPaletteTypeCustom))) {
            if (shouldUninit) CoUninitialize();
            return result;
        }

        UINT w = 0;
        UINT h = 0;
        if (SUCCEEDED(converter->GetSize(&w, &h)) && w > 0 && h > 0) {
            QImage img(static_cast<int>(w), static_cast<int>(h), QImage::Format_RGBA8888);
            if (!img.isNull() &&
                SUCCEEDED(converter->CopyPixels(nullptr, static_cast<UINT>(img.bytesPerLine()),
                                                static_cast<UINT>(img.sizeInBytes()), img.bits()))) {
                result = std::move(img);
            }
        }
    }

    if (shouldUninit) CoUninitialize();
    return result;
}
#endif

struct GifQuantizedFrame {
    int width = 0;
    int height = 0;
    bool hasAlpha = false;
    std::array<uint8_t, 256 * 3> palette{};
    std::vector<uint8_t> indices;
};

GifQuantizedFrame quantizeImageForGif(const QImage& src) {
    GifQuantizedFrame out;
    const QImage rgba = (src.format() == QImage::Format_RGBA8888)
                            ? src
                            : src.convertToFormat(QImage::Format_RGBA8888);
    if (rgba.isNull()) return out;

    const int w = rgba.width();
    const int h = rgba.height();
    out.width = w;
    out.height = h;

    struct Bin {
        uint16_t key = 0;
        uint32_t count = 0;
        uint64_t sumR = 0;
        uint64_t sumG = 0;
        uint64_t sumB = 0;
        uint8_t minR = 255, maxR = 0;
        uint8_t minG = 255, maxG = 0;
        uint8_t minB = 255, maxB = 0;
    };

    constexpr int kHistSize = 32768;
    std::vector<uint32_t> counts(kHistSize, 0);
    std::vector<uint64_t> sumR(kHistSize, 0);
    std::vector<uint64_t> sumG(kHistSize, 0);
    std::vector<uint64_t> sumB(kHistSize, 0);

    bool anyAlpha = false;
    for (int y = 0; y < h; ++y) {
        const uint8_t* p = rgba.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            const uint8_t r = p[0];
            const uint8_t g = p[1];
            const uint8_t b = p[2];
            const uint8_t a = p[3];
            p += 4;
            if (a < 128) {
                anyAlpha = true;
                continue;
            }
            const int key = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
            counts[key] += 1;
            sumR[key] += r;
            sumG[key] += g;
            sumB[key] += b;
        }
    }
    out.hasAlpha = anyAlpha;

    std::vector<Bin> bins;
    bins.reserve(2048);
    for (int k = 0; k < kHistSize; ++k) {
        if (counts[k] == 0) continue;
        Bin b;
        b.key = static_cast<uint16_t>(k);
        b.count = counts[k];
        b.sumR = sumR[k];
        b.sumG = sumG[k];
        b.sumB = sumB[k];
        const uint8_t ar = static_cast<uint8_t>(sumR[k] / counts[k]);
        const uint8_t ag = static_cast<uint8_t>(sumG[k] / counts[k]);
        const uint8_t ab = static_cast<uint8_t>(sumB[k] / counts[k]);
        b.minR = b.maxR = ar;
        b.minG = b.maxG = ag;
        b.minB = b.maxB = ab;
        bins.push_back(b);
    }

    const size_t maxColors = anyAlpha ? 255 : 256;
    struct Box {
        size_t start = 0;
        size_t end = 0;
        uint64_t totalCount = 0;
        int longestAxis = 0;
        int range = 0;
    };

    auto buildBox = [&](size_t s, size_t e) -> Box {
        Box box;
        box.start = s;
        box.end = e;
        uint8_t mnR = 255, mxR = 0, mnG = 255, mxG = 0, mnB = 255, mxB = 0;
        for (size_t i = s; i < e; ++i) {
            box.totalCount += bins[i].count;
            mnR = std::min(mnR, bins[i].minR);
            mxR = std::max(mxR, bins[i].maxR);
            mnG = std::min(mnG, bins[i].minG);
            mxG = std::max(mxG, bins[i].maxG);
            mnB = std::min(mnB, bins[i].minB);
            mxB = std::max(mxB, bins[i].maxB);
        }
        const int rR = mxR - mnR;
        const int rG = mxG - mnG;
        const int rB = mxB - mnB;
        if (rG >= rR && rG >= rB) {
            box.longestAxis = 1;
            box.range = rG;
        } else if (rR >= rB) {
            box.longestAxis = 0;
            box.range = rR;
        } else {
            box.longestAxis = 2;
            box.range = rB;
        }
        return box;
    };

    std::vector<Box> boxes;
    if (!bins.empty()) {
        boxes.push_back(buildBox(0, bins.size()));
    }

    while (boxes.size() < maxColors) {
        int bestIdx = -1;
        int bestScore = 0;
        for (int i = 0; i < static_cast<int>(boxes.size()); ++i) {
            if (boxes[i].end - boxes[i].start > 1 && boxes[i].range > bestScore) {
                bestScore = boxes[i].range;
                bestIdx = i;
            }
        }
        if (bestIdx < 0) break;

        Box target = boxes[bestIdx];
        const int axis = target.longestAxis;
        std::sort(bins.begin() + static_cast<std::ptrdiff_t>(target.start),
                  bins.begin() + static_cast<std::ptrdiff_t>(target.end),
                  [axis](const Bin& a, const Bin& b) {
                      if (axis == 0) return a.minR < b.minR;
                      if (axis == 1) return a.minG < b.minG;
                      return a.minB < b.minB;
                  });

        uint64_t half = target.totalCount / 2;
        uint64_t acc = 0;
        size_t mid = target.start + 1;
        for (size_t i = target.start; i + 1 < target.end; ++i) {
            acc += bins[i].count;
            mid = i + 1;
            if (acc >= half) break;
        }

        boxes[bestIdx] = buildBox(target.start, mid);
        boxes.push_back(buildBox(mid, target.end));
    }

    const size_t palCount = boxes.size();
    for (size_t i = 0; i < palCount; ++i) {
        uint64_t sr = 0, sg = 0, sb = 0, sc = 0;
        for (size_t j = boxes[i].start; j < boxes[i].end; ++j) {
            sr += bins[j].sumR;
            sg += bins[j].sumG;
            sb += bins[j].sumB;
            sc += bins[j].count;
        }
        if (sc > 0) {
            out.palette[i * 3 + 0] = static_cast<uint8_t>(std::min<uint64_t>(255, sr / sc));
            out.palette[i * 3 + 1] = static_cast<uint8_t>(std::min<uint64_t>(255, sg / sc));
            out.palette[i * 3 + 2] = static_cast<uint8_t>(std::min<uint64_t>(255, sb / sc));
        }
    }

    std::vector<uint8_t> lut(kHistSize, 0);
    for (size_t i = 0; i < palCount; ++i) {
        for (size_t j = boxes[i].start; j < boxes[i].end; ++j) {
            lut[bins[j].key] = static_cast<uint8_t>(i);
        }
    }

    out.indices.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
    size_t dstIdx = 0;
    for (int y = 0; y < h; ++y) {
        const uint8_t* p = rgba.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            const uint8_t r = p[0];
            const uint8_t g = p[1];
            const uint8_t b = p[2];
            const uint8_t a = p[3];
            p += 4;
            if (anyAlpha && a < 128) {
                out.indices[dstIdx++] = 255;
            } else {
                const int key = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
                out.indices[dstIdx++] = lut[key];
            }
        }
    }
    return out;
}

bool writeGifLzwRaster(QIODevice& dev, const std::vector<uint8_t>& pixels) {
    if (pixels.empty()) return false;

    const uint8_t minCodeSize = 8;
    if (dev.write(reinterpret_cast<const char*>(&minCodeSize), 1) != 1) return false;

    constexpr int clearCode = 256;
    constexpr int eoiCode = 257;
    constexpr int kHashSize = 8192;

    std::array<int32_t, kHashSize> keys;
    std::array<int16_t, kHashSize> vals;
    keys.fill(-1);

    int nextCode = 258;
    int codeBits = 9;
    int maxCodeForBits = 512;

    uint32_t bitBuf = 0;
    int bitCount = 0;
    std::array<uint8_t, 256> subBuf{};
    int subLen = 0;

    auto flushBlock = [&]() -> bool {
        if (subLen <= 0) return true;
        const uint8_t lenByte = static_cast<uint8_t>(subLen);
        if (dev.write(reinterpret_cast<const char*>(&lenByte), 1) != 1) return false;
        if (dev.write(reinterpret_cast<const char*>(subBuf.data()), subLen) != subLen) return false;
        subLen = 0;
        return true;
    };

    auto emitCode = [&](int code, int bits) -> bool {
        bitBuf |= (static_cast<uint32_t>(code) << bitCount);
        bitCount += bits;
        while (bitCount >= 8) {
            subBuf[subLen++] = static_cast<uint8_t>(bitBuf & 0xFFu);
            bitBuf >>= 8;
            bitCount -= 8;
            if (subLen == 255) {
                if (!flushBlock()) return false;
            }
        }
        return true;
    };

    if (!emitCode(clearCode, codeBits)) return false;

    int prefix = pixels[0];
    for (size_t i = 1; i < pixels.size(); ++i) {
        const uint8_t c = pixels[i];
        const int32_t key = (prefix << 8) | static_cast<int32_t>(c);
        uint32_t h = (static_cast<uint32_t>(key) * 2654435761u) & static_cast<uint32_t>(kHashSize - 1);
        while (keys[h] != -1 && keys[h] != key) {
            h = (h + 1u) & static_cast<uint32_t>(kHashSize - 1);
        }

        if (keys[h] == key) {
            prefix = vals[h];
        } else {
            if (!emitCode(prefix, codeBits)) return false;
            if (nextCode < 4096) {
                keys[h] = key;
                vals[h] = static_cast<int16_t>(nextCode++);
                if (nextCode > maxCodeForBits && codeBits < 12) {
                    ++codeBits;
                    maxCodeForBits <<= 1;
                }
            } else {
                if (!emitCode(clearCode, codeBits)) return false;
                keys.fill(-1);
                nextCode = 258;
                codeBits = 9;
                maxCodeForBits = 512;
            }
            prefix = c;
        }
    }

    if (!emitCode(prefix, codeBits)) return false;
    if (!emitCode(eoiCode, codeBits)) return false;
    if (bitCount > 0) {
        subBuf[subLen++] = static_cast<uint8_t>(bitBuf & 0xFFu);
        if (subLen == 255) {
            if (!flushBlock()) return false;
        }
    }
    if (!flushBlock()) return false;
    const uint8_t term = 0;
    return dev.write(reinterpret_cast<const char*>(&term), 1) == 1;
}

bool writeGifFrameRecord(QIODevice& dev,
                         const GifQuantizedFrame& qf,
                         int delayMs,
                         bool useGlobalColorTable) {
    const uint16_t delayCs = static_cast<uint16_t>(std::clamp(delayMs <= 0 ? 10 : qRound(delayMs / 10.0), 2, 65535));
    uint8_t gce[8] = {
        0x21, 0xF9, 0x04,
        static_cast<uint8_t>(qf.hasAlpha ? 0x09 : 0x04),
        static_cast<uint8_t>(delayCs & 0xFFu),
        static_cast<uint8_t>((delayCs >> 8) & 0xFFu),
        static_cast<uint8_t>(qf.hasAlpha ? 255 : 0),
        0x00
    };
    if (dev.write(reinterpret_cast<const char*>(gce), 8) != 8) return false;

    const uint16_t fw = static_cast<uint16_t>(std::clamp(qf.width, 1, 65535));
    const uint16_t fh = static_cast<uint16_t>(std::clamp(qf.height, 1, 65535));
    uint8_t imgDesc[10] = {
        0x2C,
        0x00, 0x00, 0x00, 0x00,
        static_cast<uint8_t>(fw & 0xFFu), static_cast<uint8_t>((fw >> 8) & 0xFFu),
        static_cast<uint8_t>(fh & 0xFFu), static_cast<uint8_t>((fh >> 8) & 0xFFu),
        static_cast<uint8_t>(useGlobalColorTable ? 0x00 : 0x87)
    };
    if (dev.write(reinterpret_cast<const char*>(imgDesc), 10) != 10) return false;

    if (!useGlobalColorTable) {
        if (dev.write(reinterpret_cast<const char*>(qf.palette.data()), 768) != 768) return false;
    }

    return writeGifLzwRaster(dev, qf.indices);
}

} // namespace

ExifMetadata ImageLoader::readMetadata(const QString& filePath) {
    ExifMetadata meta;
    QStringList extraLines;

    const QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == QStringLiteral("png")) {
        scanPngMetadataFast(filePath, &meta, &extraLines, nullptr, nullptr, nullptr, nullptr);
    } else if (ext == QStringLiteral("jpg") || ext == QStringLiteral("jpeg")) {
        scanJpegMetadata(filePath, meta, extraLines);
    } else if (ext == QStringLiteral("webp")) {
        scanWebpMetadata(filePath, meta, extraLines);
    } else {
        QFile f(filePath);
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray head = f.read(12);
            f.close();
            if (head.startsWith("\x89PNG\r\n\x1a\n")) {
                scanPngMetadataFast(filePath, &meta, &extraLines, nullptr, nullptr, nullptr, nullptr);
            } else if (head.size() >= 2 && static_cast<uchar>(head[0]) == 0xFF && static_cast<uchar>(head[1]) == 0xD8) {
                scanJpegMetadata(filePath, meta, extraLines);
            } else if (head.startsWith("RIFF") && head.mid(8, 4) == "WEBP") {
                scanWebpMetadata(filePath, meta, extraLines);
            }
        }
    }

    if (!extraLines.isEmpty()) {
        meta.rawMetadataText = extraLines.join(QStringLiteral("\n\n"));
        if (meta.rawMetadataText.size() > 64000) {
            meta.rawMetadataText.truncate(64000);
        }
    }
    return meta;
}

bool ImageLoader::stripMetadata(const QString& filePath, QString* errorOut) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorOut) *errorOut = QStringLiteral("Файл недоступен для чтения.");
        return false;
    }
    const QByteArray head = f.read(12);
    f.close();

    if (head.startsWith("\x89PNG\r\n\x1a\n")) {
        return stripPngMetadataLossless(filePath, errorOut);
    }
    if (head.size() >= 2 && static_cast<uchar>(head[0]) == 0xFF && static_cast<uchar>(head[1]) == 0xD8) {
        return stripJpegMetadataLossless(filePath, errorOut);
    }
    if (head.startsWith("RIFF") && head.mid(8, 4) == "WEBP") {
        return stripWebpMetadataLossless(filePath, errorOut);
    }

    // Fallback for BMP/TIFF: re-save pure pixel data without metadata
    QImageReader reader(filePath);
    reader.setDecideFormatFromContent(true);
    reader.setAutoTransform(true);
    const QImage cleanImg = reader.read();
    if (cleanImg.isNull()) {
        if (errorOut) *errorOut = QStringLiteral("Формат не поддерживает безопасную очистку метаданных.");
        return false;
    }
    if (!cleanImg.save(filePath)) {
        if (errorOut) *errorOut = QStringLiteral("Не удалось сохранить очищенный файл.");
        return false;
    }
    return true;
}

bool ImageLoader::saveEditedImage(const QString& sourceFilePath,
                                  const QString& destFilePath,
                                  const QByteArray& format,
                                  const QImage& image,
                                  bool stripMetadata,
                                  const ExifMetadata* fallbackMeta,
                                  QString* errorOut) {
    if (image.isNull()) {
        if (errorOut) *errorOut = QStringLiteral("Пустое изображение для сохранения.");
        return false;
    }

    const QByteArray fmt = format.toUpper();
    if (fmt == "GIF") {
        return saveAnimatedGif(sourceFilePath, destFilePath, PhotoEdits{}, image, nullptr, errorOut);
    }

    QImage outImg = image;
    if (fmt == "JPEG" || fmt == "JPG" || fmt == "BMP") {
        QImage rgb(outImg.size(), QImage::Format_RGB888);
        rgb.fill(Qt::white);
        QPainter p(&rgb);
        p.drawImage(0, 0, outImg);
        outImg = std::move(rgb);
    }

    ExportMetadataBlobs blobs;
    if (!stripMetadata) {
        blobs = collectSourceMetadataBlobs(sourceFilePath, fallbackMeta);
    }

    QByteArray encoded;
    {
        QBuffer buf(&encoded);
        buf.open(QIODevice::WriteOnly);
        QImageWriter writer(&buf, fmt);
        if (fmt == "JPEG" || fmt == "JPG" || fmt == "WEBP") {
            writer.setQuality(92);
        }
        if (!stripMetadata && (fmt == "PNG" || fmt == "TIFF" || fmt == "TIF")) {
            for (const auto& kv : blobs.pngTextEntries) {
                writer.setText(kv.first, kv.second);
            }
        }
        if (!writer.write(outImg)) {
            if (errorOut) *errorOut = writer.errorString();
            return false;
        }
    }

    if (!stripMetadata) {
        if (fmt == "PNG") {
            encoded = injectPngMetadata(encoded, blobs);
        } else if (fmt == "JPEG" || fmt == "JPG") {
            encoded = injectJpegMetadata(encoded, blobs);
        } else if (fmt == "WEBP") {
            encoded = injectWebpMetadata(encoded, blobs, outImg.width(), outImg.height(), image.hasAlphaChannel());
        }
    }

    QSaveFile outFile(destFilePath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        if (errorOut) *errorOut = outFile.errorString();
        return false;
    }
    if (outFile.write(encoded) != encoded.size() || !outFile.commit()) {
        if (errorOut) *errorOut = outFile.errorString();
        return false;
    }
    return true;
}

bool ImageLoader::saveAnimatedGif(const QString& sourceFilePath,
                                  const QString& destFilePath,
                                  const PhotoEdits& edits,
                                  const QImage& fallbackImage,
                                  const std::function<bool(int frameIndex, int totalFrames)>& progressCb,
                                  QString* errorOut) {
    QImageReader reader(sourceFilePath);
    reader.setDecideFormatFromContent(true);
    reader.setAutoTransform(true);

    const bool hasAnim = reader.supportsAnimation() && reader.imageCount() > 1;
    const int totalFrames = hasAnim ? std::max(1, reader.imageCount()) : 1;

    QSaveFile outFile(destFilePath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        if (errorOut) *errorOut = outFile.errorString();
        return false;
    }

    bool headerWritten = false;
    int targetW = 0;
    int targetH = 0;

    auto writeHeaderAndFirst = [&](const QImage& firstEdited, int delayMs, int loopCount) -> bool {
        const GifQuantizedFrame qf = quantizeImageForGif(firstEdited);
        if (qf.width <= 0 || qf.height <= 0 || qf.indices.empty()) {
            if (errorOut) *errorOut = QStringLiteral("Failed to quantize GIF frame.");
            return false;
        }
        targetW = qf.width;
        targetH = qf.height;

        if (outFile.write("GIF89a", 6) != 6) return false;
        const uint16_t lw = static_cast<uint16_t>(std::clamp(targetW, 1, 65535));
        const uint16_t lh = static_cast<uint16_t>(std::clamp(targetH, 1, 65535));
        uint8_t lsd[7] = {
            static_cast<uint8_t>(lw & 0xFFu), static_cast<uint8_t>((lw >> 8) & 0xFFu),
            static_cast<uint8_t>(lh & 0xFFu), static_cast<uint8_t>((lh >> 8) & 0xFFu),
            0xF7, // Global color table, 8-bit color res, 256 entries
            0x00,
            0x00
        };
        if (outFile.write(reinterpret_cast<const char*>(lsd), 7) != 7) return false;
        if (outFile.write(reinterpret_cast<const char*>(qf.palette.data()), 768) != 768) return false;

        if (hasAnim) {
            const uint16_t lc = (loopCount < 0) ? 0 : static_cast<uint16_t>(std::min(loopCount, 65535));
            const uint8_t netscape[19] = {
                0x21, 0xFF, 0x0B,
                'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0',
                0x03, 0x01,
                static_cast<uint8_t>(lc & 0xFFu),
                static_cast<uint8_t>((lc >> 8) & 0xFFu),
                0x00
            };
            if (outFile.write(reinterpret_cast<const char*>(netscape), 19) != 19) return false;
        }

        headerWritten = true;
        return writeGifFrameRecord(outFile, qf, delayMs, true);
    };

    if (hasAnim) {
        const int loopCount = reader.loopCount();
        int frameIdx = 0;
        QImage rawFrame;
        while (reader.read(&rawFrame)) {
            if (progressCb && !progressCb(frameIdx, totalFrames)) {
                outFile.cancelWriting();
                if (errorOut) *errorOut = trUi(QStringLiteral("Экспорт отменён."), QStringLiteral("Export cancelled."));
                return false;
            }
            const int delayMs = reader.nextImageDelay();
            QImage edited = edits.isDefault()
                                ? rawFrame.convertToFormat(QImage::Format_RGBA8888)
                                : applyEdits(rawFrame, edits);
            if (edited.isNull()) break;

            if (!headerWritten) {
                if (!writeHeaderAndFirst(edited, delayMs, loopCount)) {
                    outFile.cancelWriting();
                    return false;
                }
            } else {
                if (edited.width() != targetW || edited.height() != targetH) {
                    edited = edited.scaled(targetW, targetH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                }
                const GifQuantizedFrame qf = quantizeImageForGif(edited);
                if (!writeGifFrameRecord(outFile, qf, delayMs, false)) {
                    outFile.cancelWriting();
                    if (errorOut) *errorOut = outFile.errorString();
                    return false;
                }
            }
            ++frameIdx;
        }
    }

    if (!headerWritten) {
        QImage single = fallbackImage;
        if (single.isNull()) {
            single = reader.read();
        }
        if (single.isNull()) {
            outFile.cancelWriting();
            if (errorOut) *errorOut = trUi(QStringLiteral("Пустое изображение для сохранения."),
                                           QStringLiteral("Empty image for export."));
            return false;
        }
        const QImage edited = edits.isDefault()
                                  ? single.convertToFormat(QImage::Format_RGBA8888)
                                  : applyEdits(single, edits);
        if (!writeHeaderAndFirst(edited, 100, 0)) {
            outFile.cancelWriting();
            return false;
        }
    }

    const uint8_t trailer = 0x3B;
    if (outFile.write(reinterpret_cast<const char*>(&trailer), 1) != 1 || !outFile.commit()) {
        if (errorOut) *errorOut = outFile.errorString();
        return false;
    }
    return true;
}

#ifdef LUMINA_HAS_OIIO
static ExifMetadata extractMetadataFromSpec(const OIIO::ImageSpec& spec) {
    ExifMetadata meta;
    const std::string make = spec.get_string_attribute("Make");
    const std::string model = spec.get_string_attribute("Model");
    if (!make.empty() || !model.empty()) {
        meta.camera = QString::fromStdString(make + (make.empty() || model.empty() ? "" : " ") + model).trimmed();
    }
    const std::string lens = spec.get_string_attribute("Exif:LensModel");
    if (!lens.empty()) meta.lens = QString::fromStdString(lens);

    float fNumber = spec.get_float_attribute("Exif:FNumber", 0.0f);
    if (fNumber > 0.0f) meta.aperture = QString("f/%1").arg(fNumber, 0, 'f', 1);

    float focal = spec.get_float_attribute("Exif:FocalLength", 0.0f);
    if (focal > 0.0f) meta.focalLength = QString("%1 mm").arg(qRound(focal));

    float exposure = spec.get_float_attribute("Exif:ExposureTime", 0.0f);
    if (exposure > 0.0f) {
        meta.shutterSpeed = exposure < 1.0f
                                ? QString("1/%1 s").arg(qRound(1.0f / exposure))
                                : QString("%1 s").arg(exposure, 0, 'f', 1);
    }
    meta.iso = spec.get_int_attribute("Exif:ISOSpeedRatings", 0);
    return meta;
}
#endif

LoadedImageResult ImageLoader::loadImage(const QString& filePath, int maxDimension) {
    LoadedImageResult result;
    const QFileInfo fi(filePath);
    if (!fi.exists()) {
        result.errorMessage = QStringLiteral("Файл не найден: ") + filePath;
        return result;
    }

    const QString ext = fi.suffix().toLower();
    result.isPsd = (ext == QStringLiteral("psd"));

    if (result.isPsd) {
        const PsdLoadResult psd = PsdParser::load(filePath, maxDimension == 0);
        if (psd.success) {
            QImage img = psd.image;
            if (maxDimension > 0 && (img.width() > maxDimension || img.height() > maxDimension)) {
                img = img.scaled(maxDimension, maxDimension, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            result.image = std::move(img);
            result.metadata.psdLayers = psd.layerNames;
            result.metadata.psdLayerDetails = psd.layers;
            result.success = true;
            return result;
        }
    }

#ifdef LUMINA_HAS_QT_SVG
    if (ext == QStringLiteral("svg") || ext == QStringLiteral("svgz")) {
        QSvgRenderer svg(filePath);
        if (svg.isValid()) {
            QSize targetSz = svg.defaultSize();
            if (!targetSz.isValid() || targetSz.isEmpty()) {
                targetSz = QSize(1024, 1024);
            }
            const int desiredMax = (maxDimension > 0) ? maxDimension : 1600;
            if (maxDimension > 0) {
                targetSz = targetSz.scaled(desiredMax, desiredMax, Qt::KeepAspectRatio);
            } else if (std::max(targetSz.width(), targetSz.height()) < 1024) {
                targetSz = targetSz.scaled(1600, 1600, Qt::KeepAspectRatio);
            }
            QImage svgImg(std::max(1, targetSz.width()), std::max(1, targetSz.height()), QImage::Format_RGBA8888);
            svgImg.fill(Qt::transparent);
            QPainter p(&svgImg);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            svg.render(&p, QRectF(0, 0, svgImg.width(), svgImg.height()));
            result.image = std::move(svgImg);
            result.success = true;
            return result;
        }
    }
#endif

    // Parse full metadata only when opening full view/editor (maxDimension == 0)
    if (maxDimension == 0) {
        result.metadata = readMetadata(filePath);
    }

#ifdef LUMINA_HAS_OIIO
    if (maxDimension == 0 || (fi.suffix().toLower() != QStringLiteral("jpg") &&
                              fi.suffix().toLower() != QStringLiteral("jpeg") &&
                              fi.suffix().toLower() != QStringLiteral("png") &&
                              fi.suffix().toLower() != QStringLiteral("webp"))) {
        std::unique_ptr<OIIO::ImageInput> inp = OIIO::ImageInput::open(filePath.toStdString());
        if (inp) {
            const OIIO::ImageSpec& spec = inp->spec();
            if (maxDimension == 0 && result.metadata.camera.isEmpty()) {
                ExifMetadata oiioMeta = extractMetadataFromSpec(spec);
                if (!oiioMeta.camera.isEmpty()) result.metadata.camera = oiioMeta.camera;
                if (!oiioMeta.lens.isEmpty()) result.metadata.lens = oiioMeta.lens;
                if (!oiioMeta.aperture.isEmpty()) result.metadata.aperture = oiioMeta.aperture;
                if (!oiioMeta.focalLength.isEmpty()) result.metadata.focalLength = oiioMeta.focalLength;
                if (!oiioMeta.shutterSpeed.isEmpty()) result.metadata.shutterSpeed = oiioMeta.shutterSpeed;
                if (oiioMeta.iso > 0) result.metadata.iso = oiioMeta.iso;
            }

            const int w = spec.width;
            const int h = spec.height;
            const int ch = spec.nchannels;

            if (w > 0 && h > 0 && ch > 0) {
                std::vector<uint8_t> raw(static_cast<size_t>(w) * h * ch);
                if (inp->read_image(0, 0, 0, ch, OIIO::TypeDesc::UINT8, raw.data())) {
                    QImage img(w, h, QImage::Format_RGBA8888);
                    for (int y = 0; y < h; ++y) {
                        uint8_t* dst = img.scanLine(y);
                        const uint8_t* src = raw.data() + static_cast<size_t>(y) * w * ch;
                        for (int x = 0; x < w; ++x) {
                            const uint8_t* px = src + static_cast<size_t>(x) * ch;
                            if (ch == 1) {
                                dst[0] = dst[1] = dst[2] = px[0];
                                dst[3] = 255;
                            } else if (ch == 3) {
                                dst[0] = px[0];
                                dst[1] = px[1];
                                dst[2] = px[2];
                                dst[3] = 255;
                            } else {
                                dst[0] = px[0];
                                dst[1] = px[1];
                                dst[2] = px[2];
                                dst[3] = px[3];
                            }
                            dst += 4;
                        }
                    }
                    inp->close();

                    if (maxDimension > 0 && (img.width() > maxDimension || img.height() > maxDimension)) {
                        img = img.scaled(maxDimension, maxDimension, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    }
                    result.image = std::move(img);
                    result.success = true;
                    return result;
                }
            }
            inp->close();
        }
    }
#endif

    QImageReader reader(filePath);
    reader.setDecideFormatFromContent(true);
    reader.setAutoTransform(true);
    if (maxDimension == 0 && (ext == QStringLiteral("gif") || ext == QStringLiteral("webp"))) {
        result.isAnimated = reader.supportsAnimation() && reader.imageCount() > 1;
    }
    if (maxDimension > 0) {
        const QSize origSz = reader.size();
        if (origSz.isValid() && (origSz.width() > maxDimension || origSz.height() > maxDimension)) {
            reader.setScaledSize(origSz.scaled(maxDimension, maxDimension, Qt::KeepAspectRatio));
        }
    }

    QImage qtImg = reader.read();
    if (!qtImg.isNull()) {
        if (maxDimension > 0 && (qtImg.width() > maxDimension || qtImg.height() > maxDimension)) {
            qtImg = qtImg.scaled(maxDimension, maxDimension, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        result.image = qtImg.convertToFormat(QImage::Format_RGBA8888);
        result.success = true;
        return result;
    }

#ifdef _WIN32
    QImage wicImg = loadViaWindowsWic(filePath);
    if (!wicImg.isNull()) {
        if (maxDimension > 0 && (wicImg.width() > maxDimension || wicImg.height() > maxDimension)) {
            wicImg = wicImg.scaled(maxDimension, maxDimension, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        result.image = std::move(wicImg);
        result.success = true;
        return result;
    }
#endif

    result.errorMessage = reader.errorString().isEmpty()
                              ? QStringLiteral("Не удалось декодировать изображение")
                              : reader.errorString();
    return result;
}

namespace {

QImage applyUnsharpMask(const QImage& src, int amount) {
    if (amount <= 0 || src.width() < 3 || src.height() < 3) return src;
    const double strength = (amount / 100.0) * 1.45;
    QImage out = src.copy();
    const int w = src.width();
    const int h = src.height();

    for (int y = 1; y < h - 1; ++y) {
        const uint8_t* prev = src.constScanLine(y - 1);
        const uint8_t* curr = src.constScanLine(y);
        const uint8_t* next = src.constScanLine(y + 1);
        uint8_t* dst = out.scanLine(y);

        for (int x = 1; x < w - 1; ++x) {
            const int idx = x * 4;
            for (int c = 0; c < 3; ++c) {
                const double blur = (prev[idx + c] + next[idx + c] + curr[idx - 4 + c] + curr[idx + 4 + c]) * 0.25;
                const double val = curr[idx + c] + strength * (curr[idx + c] - blur);
                dst[idx + c] = static_cast<uint8_t>(std::clamp(val, 0.0, 255.0));
            }
        }
    }
    return out;
}

QImage fastSoftBlur(const QImage& src, int radiusFactor) {
    if (src.isNull() || src.width() < 4 || src.height() < 4) return src;
    const double div = std::clamp(2.0 + radiusFactor * 0.14, 2.0, 14.0);
    const int sw = std::max(2, qRound(src.width() / div));
    const int sh = std::max(2, qRound(src.height() / div));
    return src.scaled(sw, sh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .scaled(src.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .convertToFormat(QImage::Format_RGBA8888);
}

uint32_t fastHash(uint32_t x, uint32_t y) {
    uint32_t h = x * 374761393u + y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

void drawFreeTextLayer(QImage& img, const EffectLayer& layer) {
    if (layer.text.trimmed().isEmpty()) return;
    QPainter p(&img);
    ImageLoader::drawTextOverlay(p, QRectF(img.rect()), layer);
}

void applySingleEffectLayer(QImage& img, const EffectLayer& layer) {
    if (!layer.enabled || layer.preset == EffectPreset::None || layer.intensity <= 0) return;

    if (layer.preset == EffectPreset::TextOverlay) {
        drawFreeTextLayer(img, layer);
        return;
    }

    const double blend = std::clamp(layer.intensity, 0, 100) / 100.0;
    const double p2 = std::clamp(layer.param2, 0, 100) / 100.0;
    const int w = img.width();
    const int h = img.height();

    switch (layer.preset) {
        case EffectPreset::SharpenEffect: {
            img = applyUnsharpMask(img, qRound(layer.intensity * 1.4));
            return;
        }
        case EffectPreset::AutoLucky: {
            // Balanced Auto-Fix: boost micro-contrast, lift shadows slightly, subtle warmth + saturation + crispness
            for (int y = 0; y < h; ++y) {
                uint8_t* px = img.scanLine(y);
                for (int x = 0; x < w; ++x) {
                    double r = px[0], g = px[1], b = px[2];
                    const double lumNorm = std::clamp((0.299 * r + 0.587 * g + 0.114 * b) / 255.0, 0.0, 1.0);
                    const double lift = 18.0 * (1.0 - lumNorm) * (1.0 - lumNorm) - 10.0 * (lumNorm * lumNorm);
                    double tr = (r + 12.0 + lift - 128.0) * 1.18 + 128.0;
                    double tg = (g + 8.0 + lift - 128.0) * 1.18 + 128.0;
                    double tb = (b + 2.0 + lift - 128.0) * 1.18 + 128.0;
                    const double lum = 0.299 * tr + 0.587 * tg + 0.114 * tb;
                    tr = lum + (tr - lum) * 1.16;
                    tg = lum + (tg - lum) * 1.16;
                    tb = lum + (tb - lum) * 1.16;
                    px[0] = static_cast<uint8_t>(std::clamp(r + (tr - r) * blend, 0.0, 255.0));
                    px[1] = static_cast<uint8_t>(std::clamp(g + (tg - g) * blend, 0.0, 255.0));
                    px[2] = static_cast<uint8_t>(std::clamp(b + (tb - b) * blend, 0.0, 255.0));
                    px += 4;
                }
            }
            img = applyUnsharpMask(img, qRound(20.0 * blend));
            return;
        }
        case EffectPreset::AutoContrast: {
            for (int y = 0; y < h; ++y) {
                uint8_t* px = img.scanLine(y);
                for (int x = 0; x < w; ++x) {
                    const double r = px[0], g = px[1], b = px[2];
                    const double tr = (r - 128.0) * 1.28 + 128.0;
                    const double tg = (g - 128.0) * 1.28 + 128.0;
                    const double tb = (b - 128.0) * 1.28 + 128.0;
                    px[0] = static_cast<uint8_t>(std::clamp(r + (tr - r) * blend, 0.0, 255.0));
                    px[1] = static_cast<uint8_t>(std::clamp(g + (tg - g) * blend, 0.0, 255.0));
                    px[2] = static_cast<uint8_t>(std::clamp(b + (tb - b) * blend, 0.0, 255.0));
                    px += 4;
                }
            }
            return;
        }
        case EffectPreset::AutoColor:
        case EffectPreset::AutoBalance: {
            // Compute channel averages for gray-world auto white balance + pleasant saturation lift
            double sumR = 0, sumG = 0, sumB = 0;
            const qint64 nPx = static_cast<qint64>(w) * h;
            for (int y = 0; y < h; y += 2) {
                const uint8_t* px = img.constScanLine(y);
                for (int x = 0; x < w; x += 2) {
                    sumR += px[0];
                    sumG += px[1];
                    sumB += px[2];
                    px += 8;
                }
            }
            const double samples = std::max<double>(1.0, (h / 2.0) * (w / 2.0));
            const double avgR = sumR / samples, avgG = sumG / samples, avgB = sumB / samples;
            const double avgGray = (avgR + avgG + avgB) / 3.0;
            const double gainR = std::clamp(avgGray / std::max(16.0, avgR), 0.78, 1.28);
            const double gainG = std::clamp(avgGray / std::max(16.0, avgG), 0.78, 1.28);
            const double gainB = std::clamp(avgGray / std::max(16.0, avgB), 0.78, 1.28);
            const double satMult = (layer.preset == EffectPreset::AutoColor) ? 1.24 : 1.14;
            (void)nPx;

            for (int y = 0; y < h; ++y) {
                uint8_t* px = img.scanLine(y);
                for (int x = 0; x < w; ++x) {
                    const double r = px[0], g = px[1], b = px[2];
                    double tr = r * gainR;
                    double tg = g * gainG;
                    double tb = b * gainB;
                    const double lum = 0.299 * tr + 0.587 * tg + 0.114 * tb;
                    tr = lum + (tr - lum) * satMult;
                    tg = lum + (tg - lum) * satMult;
                    tb = lum + (tb - lum) * satMult;
                    px[0] = static_cast<uint8_t>(std::clamp(r + (tr - r) * blend, 0.0, 255.0));
                    px[1] = static_cast<uint8_t>(std::clamp(g + (tg - g) * blend, 0.0, 255.0));
                    px[2] = static_cast<uint8_t>(std::clamp(b + (tb - b) * blend, 0.0, 255.0));
                    px += 4;
                }
            }
            return;
        }
        case EffectPreset::Soften:
        case EffectPreset::SoftFocus: {
            const QImage blurred = fastSoftBlur(img, layer.preset == EffectPreset::SoftFocus ? 42 : 24);
            const double alpha = (layer.preset == EffectPreset::SoftFocus ? 0.65 : 0.78) * blend;
            for (int y = 0; y < h; ++y) {
                uint8_t* dst = img.scanLine(y);
                const uint8_t* bl = blurred.constScanLine(y);
                for (int x = 0; x < w; ++x) {
                    for (int c = 0; c < 3; ++c) {
                        const double v = dst[c] * (1.0 - alpha) + bl[c] * alpha;
                        dst[c] = static_cast<uint8_t>(std::clamp(v, 0.0, 255.0));
                    }
                    dst += 4;
                    bl += 4;
                }
            }
            return;
        }
        case EffectPreset::Glow:
        case EffectPreset::Orton: {
            const QImage blurred = fastSoftBlur(img, 36);
            for (int y = 0; y < h; ++y) {
                uint8_t* dst = img.scanLine(y);
                const uint8_t* bl = blurred.constScanLine(y);
                for (int x = 0; x < w; ++x) {
                    for (int c = 0; c < 3; ++c) {
                        const double a = dst[c] / 255.0;
                        const double b = bl[c] / 255.0;
                        const double target = (layer.preset == EffectPreset::Glow)
                                                  ? (1.0 - (1.0 - a) * (1.0 - b * 0.85))
                                                  : ((1.0 - (1.0 - a) * (1.0 - b)) * 0.7 + a * b * 0.55);
                        const double mixed = a + (target - a) * blend;
                        dst[c] = static_cast<uint8_t>(std::clamp(mixed * 255.0, 0.0, 255.0));
                    }
                    dst += 4;
                    bl += 4;
                }
            }
            return;
        }
        case EffectPreset::HdrEffect: {
            const QImage blurred = fastSoftBlur(img, 45);
            for (int y = 0; y < h; ++y) {
                uint8_t* dst = img.scanLine(y);
                const uint8_t* bl = blurred.constScanLine(y);
                for (int x = 0; x < w; ++x) {
                    for (int c = 0; c < 3; ++c) {
                        const double orig = dst[c];
                        const double detail = orig - bl[c];
                        const double boosted = std::clamp(orig + detail * 1.45, 0.0, 255.0);
                        const double cont = (boosted - 128.0) * 1.18 + 128.0;
                        dst[c] = static_cast<uint8_t>(std::clamp(orig + (cont - orig) * blend, 0.0, 255.0));
                    }
                    dst += 4;
                    bl += 4;
                }
            }
            return;
        }
        case EffectPreset::Pixelate: {
            const int block = std::max(2, qRound(2.0 + blend * std::max(w, h) * 0.032));
            const int pw = std::max(1, w / block);
            const int ph = std::max(1, h / block);
            img = img.scaled(pw, ph, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                      .scaled(w, h, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                      .convertToFormat(QImage::Format_RGBA8888);
            return;
        }
        case EffectPreset::FocalZoom: {
            const QImage blurred = fastSoftBlur(img, 36);
            const double cx = w * 0.5;
            const double cy = h * 0.5;
            const double invMaxR = 1.0 / std::max(1.0, std::sqrt(cx * cx + cy * cy));
            for (int y = 0; y < h; ++y) {
                uint8_t* dst = img.scanLine(y);
                const uint8_t* bl = blurred.constScanLine(y);
                const double dy = y - cy;
                const double dy2 = dy * dy;
                for (int x = 0; x < w; ++x) {
                    const double dx = x - cx;
                    const double dist = std::sqrt(dx * dx + dy2) * invMaxR;
                    const double factor = std::clamp((dist - 0.25) / 0.65, 0.0, 1.0) * blend;
                    for (int c = 0; c < 3; ++c) {
                        dst[c] = static_cast<uint8_t>(std::clamp(dst[c] * (1.0 - factor) + bl[c] * factor, 0.0, 255.0));
                    }
                    dst += 4;
                    bl += 4;
                }
            }
            return;
        }
        case EffectPreset::PencilSketch:
        case EffectPreset::Neon:
        case EffectPreset::Contour: {
            if (w < 3 || h < 3) return;
            const QImage srcCopy = img.copy();
            for (int y = 1; y < h - 1; ++y) {
                const uint8_t* prev = srcCopy.constScanLine(y - 1);
                const uint8_t* curr = srcCopy.constScanLine(y);
                const uint8_t* next = srcCopy.constScanLine(y + 1);
                uint8_t* dst = img.scanLine(y);
                for (int x = 1; x < w - 1; ++x) {
                    const int i = x * 4;
                    auto lumAt = [](const uint8_t* p) {
                        return 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
                    };
                    const double gx = lumAt(curr + i + 4) - lumAt(curr + i - 4);
                    const double gy = lumAt(next + i) - lumAt(prev + i);
                    const double edge = std::min(255.0, std::sqrt(gx * gx + gy * gy) * 1.8);

                    double er = curr[i], eg = curr[i + 1], eb = curr[i + 2];
                    if (layer.preset == EffectPreset::PencilSketch) {
                        const double ink = std::clamp(255.0 - edge * 1.3, 18.0, 252.0);
                        er = eg = eb = ink;
                    } else if (layer.preset == EffectPreset::Neon) {
                        const double amp = std::clamp(edge / 110.0, 0.0, 2.2);
                        er = std::clamp(curr[i] * amp * 1.5, 0.0, 255.0);
                        eg = std::clamp(curr[i + 1] * amp * 1.3, 0.0, 255.0);
                        eb = std::clamp(curr[i + 2] * amp * 1.6, 0.0, 255.0);
                    } else {
                        const double emboss = std::clamp(128.0 + (gx + gy) * 0.9, 0.0, 255.0);
                        er = eg = eb = emboss;
                    }

                    dst[i] = static_cast<uint8_t>(std::clamp(curr[i] + (er - curr[i]) * blend, 0.0, 255.0));
                    dst[i + 1] = static_cast<uint8_t>(std::clamp(curr[i + 1] + (eg - curr[i + 1]) * blend, 0.0, 255.0));
                    dst[i + 2] = static_cast<uint8_t>(std::clamp(curr[i + 2] + (eb - curr[i + 2]) * blend, 0.0, 255.0));
                }
            }
            return;
        }
        case EffectPreset::Vignette: {
            QPainter painter(&img);
            QRadialGradient grad(w / 2.0, h / 2.0, std::hypot(w / 2.0, h / 2.0));
            grad.setColorAt(0.35, QColor(0, 0, 0, 0));
            grad.setColorAt(1.0, QColor(0, 0, 0, qRound(235 * blend)));
            painter.fillRect(img.rect(), grad);
            return;
        }
        case EffectPreset::DropShadow: {
            const int pad = std::max(8, qRound(std::min(w, h) * (0.04 + 0.05 * blend)));
            QImage canvas(w + pad * 2, h + pad * 2, QImage::Format_RGBA8888);
            canvas.fill(QColor(244, 245, 248));
            QPainter p(&canvas);
            p.setRenderHint(QPainter::Antialiasing, true);
            for (int s = 6; s >= 1; --s) {
                p.fillRect(QRect(pad + s, pad + s + 2, w, h), QColor(0, 0, 0, 22));
            }
            p.fillRect(QRect(pad - 3, pad - 3, w + 6, h + 6), Qt::white);
            p.drawImage(pad, pad, img);
            img = std::move(canvas);
            return;
        }
        case EffectPreset::MatteFrame: {
            const int border = std::max(8, qRound(std::min(w, h) * (0.04 + 0.08 * blend)));
            QImage canvas(w + border * 2, h + border * 2, QImage::Format_RGBA8888);
            canvas.fill(QColor(247, 245, 240));
            QPainter p(&canvas);
            p.setPen(QPen(QColor(200, 196, 188), 2));
            p.drawRect(border - 2, border - 2, w + 3, h + 3);
            p.drawImage(border, border, img);
            img = std::move(canvas);
            return;
        }
        case EffectPreset::Polaroid: {
            const int side = std::max(10, qRound(std::min(w, h) * 0.055));
            const int bottom = std::max(24, qRound(std::min(w, h) * (0.14 + 0.06 * blend)));
            QImage canvas(w + side * 2, h + side + bottom, QImage::Format_RGBA8888);
            canvas.fill(QColor(250, 250, 248));
            QPainter p(&canvas);
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.setPen(QColor(220, 220, 216));
            p.drawRect(0, 0, canvas.width() - 1, canvas.height() - 1);
            p.drawImage(side, side, img);
            img = std::move(canvas);
            return;
        }
        default:
            break;
    }

    const double cx = w * 0.5;
    const double cy = h * 0.5;
    const double invMaxR = 1.0 / std::max(1.0, std::sqrt(cx * cx + cy * cy));
    const QColor tintColor = QColor::fromHslF(p2, 0.72, 0.52);
    const QColor duoHi = QColor::fromHslF(p2, 0.85, 0.62);
    const QColor duoLo = QColor::fromHslF(std::fmod(p2 + 0.55, 1.0), 0.75, 0.18);

    for (int y = 0; y < h; ++y) {
        uint8_t* line = img.scanLine(y);
        const double ny = static_cast<double>(y) / std::max(1, h - 1);
        const double dy = y - cy;
        const double dy2 = dy * dy;
        for (int x = 0; x < w; ++x) {
            const double r = line[0];
            const double g = line[1];
            const double b = line[2];
            const double lum = 0.299 * r + 0.587 * g + 0.114 * b;
            double er = r, eg = g, eb = b;

            switch (layer.preset) {
                case EffectPreset::Sepia: {
                    er = r * 0.393 + g * 0.769 + b * 0.189;
                    eg = r * 0.349 + g * 0.686 + b * 0.168;
                    eb = r * 0.272 + g * 0.534 + b * 0.131;
                    break;
                }
                case EffectPreset::BlackWhite: {
                    const double bw = (lum - 128.0) * 1.12 + 128.0;
                    er = eg = eb = bw;
                    break;
                }
                case EffectPreset::WarmLight: {
                    er = r + 28.0;
                    eg = g + 10.0;
                    eb = b - 20.0;
                    break;
                }
                case EffectPreset::FilmGrain: {
                    const double n = (static_cast<int>(fastHash(x, y) & 0xFF) - 128) * 0.38;
                    er = r + n;
                    eg = g + n;
                    eb = b + n;
                    break;
                }
                case EffectPreset::ColorTint: {
                    er = lum * 0.45 + tintColor.red() * 0.65;
                    eg = lum * 0.45 + tintColor.green() * 0.65;
                    eb = lum * 0.45 + tintColor.blue() * 0.65;
                    break;
                }
                case EffectPreset::SatBoost: {
                    er = lum + (r - lum) * 1.65;
                    eg = lum + (g - lum) * 1.65;
                    eb = lum + (b - lum) * 1.65;
                    break;
                }
                case EffectPreset::BwFilter: {
                    const double wr = 0.7 - p2 * 0.5;
                    const double wg = 0.35;
                    const double wb = 1.0 - wr - wg;
                    const double v = std::clamp(r * wr + g * wg + b * wb, 0.0, 255.0);
                    er = eg = eb = v;
                    break;
                }
                case EffectPreset::BwFocus: {
                    const double dx = x - cx;
                    const double dist = std::sqrt(dx * dx + dy2) * invMaxR;
                    const double keepColor = std::clamp(1.0 - (dist - 0.18) / 0.55, 0.0, 1.0);
                    er = lum + (r - lum) * keepColor;
                    eg = lum + (g - lum) * keepColor;
                    eb = lum + (b - lum) * keepColor;
                    break;
                }
                case EffectPreset::GradTint: {
                    const double skyMask = std::clamp(1.0 - ny * 1.35, 0.0, 1.0);
                    er = r * (1.0 - skyMask * 0.55) + tintColor.red() * skyMask * 0.55;
                    eg = g * (1.0 - skyMask * 0.55) + tintColor.green() * skyMask * 0.55;
                    eb = b * (1.0 - skyMask * 0.55) + tintColor.blue() * skyMask * 0.55;
                    break;
                }
                case EffectPreset::Infrared: {
                    const double ir = std::clamp((g * 0.72 + r * 0.48 - b * 0.25 - 128.0) * 1.32 + 128.0, 0.0, 255.0);
                    er = eg = eb = ir;
                    break;
                }
                case EffectPreset::Lomo: {
                    const double dx = x - cx;
                    const double dist = std::sqrt(dx * dx + dy2) * invMaxR;
                    const double vig = 1.0 - std::clamp((dist - 0.42) / 0.65, 0.0, 0.75);
                    er = ((r - 128.0) * 1.38 + 128.0 + 8.0) * vig;
                    eg = ((g - 128.0) * 1.42 + 128.0 + 4.0) * vig;
                    eb = ((b - 128.0) * 1.32 + 128.0 - 6.0) * vig;
                    break;
                }
                case EffectPreset::Holga: {
                    const double dx = x - cx;
                    const double dist = std::sqrt(dx * dx + dy2) * invMaxR;
                    const double vig = 1.0 - std::clamp((dist - 0.30) / 0.68, 0.0, 0.88);
                    const double n = (static_cast<int>(fastHash(x, y) & 0xFF) - 128) * 0.22;
                    const double v = ((lum - 128.0) * 1.35 + 128.0 + n) * vig;
                    er = eg = eb = v;
                    break;
                }
                case EffectPreset::Cinemascope: {
                    const int barH = qRound(h * 0.105 * blend);
                    if (y < barH || y >= h - barH) {
                        er = eg = eb = 10.0;
                    } else {
                        er = (lum + (r - lum) * 0.72 - 128.0) * 1.24 + 128.0 + 8.0;
                        eg = (lum + (g - lum) * 0.72 - 128.0) * 1.24 + 128.0 + 3.0;
                        eb = (lum + (b - lum) * 0.72 - 128.0) * 1.24 + 128.0 - 11.0;
                    }
                    break;
                }
                case EffectPreset::Vintage60s: {
                    er = r * 0.92 + 28.0;
                    eg = g * 0.86 + 18.0;
                    eb = b * 0.78 + 34.0;
                    break;
                }
                case EffectPreset::Invert: {
                    er = 255.0 - r;
                    eg = 255.0 - g;
                    eb = 255.0 - b;
                    break;
                }
                case EffectPreset::HeatMap: {
                    const double t = std::clamp(lum / 255.0, 0.0, 1.0);
                    if (t < 0.25) {
                        er = 0.0; eg = (t / 0.25) * 255.0; eb = 255.0;
                    } else if (t < 0.5) {
                        er = 0.0; eg = 255.0; eb = (1.0 - (t - 0.25) / 0.25) * 255.0;
                    } else if (t < 0.75) {
                        er = ((t - 0.5) / 0.25) * 255.0; eg = 255.0; eb = 0.0;
                    } else {
                        er = 255.0; eg = (1.0 - (t - 0.75) / 0.25) * 255.0; eb = 0.0;
                    }
                    break;
                }
                case EffectPreset::CrossProcess: {
                    er = (r - 128.0) * 1.25 + 132.0;
                    eg = (g - 128.0) * 1.35 + 138.0;
                    eb = (b - 128.0) * 0.92 + 114.0 + (255.0 - lum) * 0.14;
                    break;
                }
                case EffectPreset::Posterize: {
                    const int levels = std::max(2, qRound(8.0 - blend * 5.0));
                    const double step = 255.0 / levels;
                    er = std::round(r / step) * step;
                    eg = std::round(g / step) * step;
                    eb = std::round(b / step) * step;
                    break;
                }
                case EffectPreset::DuoTone: {
                    const double t = std::clamp(lum / 255.0, 0.0, 1.0);
                    er = duoLo.red() + (duoHi.red() - duoLo.red()) * t;
                    eg = duoLo.green() + (duoHi.green() - duoLo.green()) * t;
                    eb = duoLo.blue() + (duoHi.blue() - duoLo.blue()) * t;
                    break;
                }
                case EffectPreset::Boost: {
                    er = ((lum + (r - lum) * 1.35) - 128.0) * 1.28 + 128.0;
                    eg = ((lum + (g - lum) * 1.35) - 128.0) * 1.28 + 128.0;
                    eb = ((lum + (b - lum) * 1.35) - 128.0) * 1.28 + 128.0;
                    break;
                }
                case EffectPreset::ComicHalftone: {
                    const double step = 64.0;
                    const double pr = std::round(r / step) * step;
                    const double pg = std::round(g / step) * step;
                    const double pb = std::round(b / step) * step;
                    const bool dot = ((x % 4) == 0 && (y % 4) == 0) && lum < 170.0;
                    er = dot ? pr * 0.55 : pr;
                    eg = dot ? pg * 0.55 : pg;
                    eb = dot ? pb * 0.55 : pb;
                    break;
                }
                default:
                    break;
            }

            line[0] = static_cast<uint8_t>(std::clamp(r + (er - r) * blend, 0.0, 255.0));
            line[1] = static_cast<uint8_t>(std::clamp(g + (eg - g) * blend, 0.0, 255.0));
            line[2] = static_cast<uint8_t>(std::clamp(b + (eb - b) * blend, 0.0, 255.0));
            line += 4;
        }
    }
}

} // namespace

void ImageLoader::drawTextOverlay(QPainter& p, const QRectF& targetRect, const EffectLayer& layer) {
    if (layer.text.trimmed().isEmpty() || targetRect.width() < 4.0 || targetRect.height() < 4.0) return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const int fontSize = std::max(8, qRound(targetRect.height() * (std::clamp(layer.textSize, 10, 160) / 360.0)));
    QFont f(QStringLiteral("Segoe UI"), -1, QFont::Bold);
    f.setPixelSize(fontSize);
    p.setFont(f);

    const QFontMetricsF fm(f);
    const QRectF bound = fm.boundingRect(
        QRectF(0.0, 0.0, std::max(40.0, targetRect.width() - 16.0), targetRect.height()),
        Qt::TextWordWrap | Qt::AlignCenter,
        layer.text);
    const double boxW = std::min(targetRect.width() - 8.0, bound.width() + std::max(12.0, fontSize * 0.6));
    const double boxH = std::min(targetRect.height() - 8.0, bound.height() + std::max(8.0, fontSize * 0.35));
    const double cx = targetRect.left() + std::clamp(layer.posX, 0.02, 0.98) * targetRect.width();
    const double cy = targetRect.top() + std::clamp(layer.posY, 0.02, 0.98) * targetRect.height();
    const double left = std::clamp(cx - boxW * 0.5, targetRect.left() + 4.0,
                                   std::max(targetRect.left() + 4.0, targetRect.right() - boxW - 4.0));
    const double top = std::clamp(cy - boxH * 0.5, targetRect.top() + 4.0,
                                  std::max(targetRect.top() + 4.0, targetRect.bottom() - boxH - 4.0));
    const QRectF textRect(left, top, boxW, boxH);

    const int alpha = qRound(255.0 * (std::clamp(layer.intensity, 0, 100) / 100.0));
    QColor mainCol(layer.textColor);
    if (!mainCol.isValid()) mainCol = QColor(255, 255, 255);
    mainCol.setAlpha(alpha);

    const double shadowOff = std::max(1.0, fontSize * 0.06);
    QColor shadowCol(0, 0, 0, qRound(alpha * 0.72));
    p.setPen(shadowCol);
    p.drawText(textRect.translated(shadowOff, shadowOff), Qt::AlignCenter | Qt::TextWordWrap, layer.text);
    p.setPen(mainCol);
    p.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, layer.text);
    p.restore();
}

QImage ImageLoader::applyEdits(const QImage& source,
                               const PhotoEdits& edits,
                               bool skipCrop,
                               const QString& skipLayerId) {
    if (source.isNull()) return source;
    if (edits.isDefault()) return source;

    QImage img = source.convertToFormat(QImage::Format_RGBA8888);

    // 1. Free-focus Crop Box
    if (!skipCrop && edits.cropEnabled && edits.hasCrop()) {
        const double l = std::clamp(edits.cropLeft, 0.0, 0.98);
        const double t = std::clamp(edits.cropTop, 0.0, 0.98);
        const double r = std::clamp(edits.cropRight, l + 0.02, 1.0);
        const double b = std::clamp(edits.cropBottom, t + 0.02, 1.0);
        const int x = qRound(l * img.width());
        const int y = qRound(t * img.height());
        const int cw = std::max(8, qRound((r - l) * img.width()));
        const int ch = std::max(8, qRound((b - t) * img.height()));
        img = img.copy(x, y, std::min(cw, img.width() - x), std::min(ch, img.height() - y));
    }

    // 2. Light, Contrast, Saturation, Temperature, Tint, Full 360° Hue Rotation
    const bool needsColorPass = edits.lightColorEnabled && (
        edits.brightness != 0 ||
        edits.highlights != 0 ||
        edits.shadows != 0 ||
        edits.contrast != 0 ||
        edits.saturation != 0 ||
        edits.temperature != 0 ||
        edits.tint != 0 ||
        edits.hueShift != 0);

    if (needsColorPass) {
        const double brightOffset = edits.brightness * 1.4;
        const double contrastFactor = std::pow((100.0 + std::clamp(edits.contrast, -95, 100)) / 100.0, 2.0);
        const double satFactor = (100.0 + edits.saturation) / 100.0;
        const double tempShift = edits.temperature * 0.7;
        const double tintShift = edits.tint * 0.55;

        const bool hasHue = (edits.hueShift != 0);
        const double rad = edits.hueShift * 3.14159265358979323846 / 180.0;
        const double cosA = std::cos(rad);
        const double sinA = std::sin(rad);
        constexpr double kSq = 0.5773502691896258;
        const double m00 = cosA + (1.0 - cosA) / 3.0;
        const double m01 = (1.0 - cosA) / 3.0 - kSq * sinA;
        const double m02 = (1.0 - cosA) / 3.0 + kSq * sinA;
        const double m10 = (1.0 - cosA) / 3.0 + kSq * sinA;
        const double m11 = cosA + (1.0 - cosA) / 3.0;
        const double m12 = (1.0 - cosA) / 3.0 - kSq * sinA;
        const double m20 = (1.0 - cosA) / 3.0 - kSq * sinA;
        const double m21 = (1.0 - cosA) / 3.0 + kSq * sinA;
        const double m22 = cosA + (1.0 - cosA) / 3.0;

        const int w = img.width();
        const int h = img.height();

        for (int y = 0; y < h; ++y) {
            uint8_t* line = img.scanLine(y);
            for (int x = 0; x < w; ++x) {
                double r = line[0];
                double g = line[1];
                double b = line[2];

                r += brightOffset + tempShift + tintShift * 0.5;
                g += brightOffset - tintShift;
                b += brightOffset - tempShift + tintShift * 0.5;

                const double lumNorm = std::clamp((0.299 * r + 0.587 * g + 0.114 * b) / 255.0, 0.0, 1.0);
                if (edits.shadows != 0) {
                    const double shadowWeight = (1.0 - lumNorm) * (1.0 - lumNorm);
                    const double delta = edits.shadows * 1.35 * shadowWeight;
                    r += delta;
                    g += delta;
                    b += delta;
                }
                if (edits.highlights != 0) {
                    const double hiWeight = lumNorm * lumNorm;
                    const double delta = edits.highlights * 1.35 * hiWeight;
                    r += delta;
                    g += delta;
                    b += delta;
                }

                r = (r - 128.0) * contrastFactor + 128.0;
                g = (g - 128.0) * contrastFactor + 128.0;
                b = (b - 128.0) * contrastFactor + 128.0;

                const double lum = 0.299 * r + 0.587 * g + 0.114 * b;
                r = lum + (r - lum) * satFactor;
                g = lum + (g - lum) * satFactor;
                b = lum + (b - lum) * satFactor;

                if (hasHue) {
                    const double hr = r * m00 + g * m01 + b * m02;
                    const double hg = r * m10 + g * m11 + b * m12;
                    const double hb = r * m20 + g * m21 + b * m22;
                    r = hr;
                    g = hg;
                    b = hb;
                }

                line[0] = static_cast<uint8_t>(std::clamp(r, 0.0, 255.0));
                line[1] = static_cast<uint8_t>(std::clamp(g, 0.0, 255.0));
                line[2] = static_cast<uint8_t>(std::clamp(b, 0.0, 255.0));
                line += 4;
            }
        }
    }

    // 3. Base Sharpening
    if (edits.basicEnabled && edits.sharpen > 0) {
        img = applyUnsharpMask(img, edits.sharpen);
    }

    // 4. Rotation (always active) + Straighten before overlay effects / text so text coordinates align visually
    {
        const double totalAngle = static_cast<double>(edits.rotationDeg) +
                                  (edits.basicEnabled ? edits.straightenDeg : 0.0);
        if (!qFuzzyIsNull(totalAngle)) {
            QTransform transform;
            transform.rotate(totalAngle);
            img = img.transformed(transform, Qt::SmoothTransformation);
        }
    }

    // 5. Photoshop-style Stacked Effect Layers (including AutoFixes & Free-Positioned TextOverlay)
    for (const EffectLayer& layer : edits.effectLayers) {
        if (!skipLayerId.isEmpty() && layer.id == skipLayerId) {
            continue;
        }
        applySingleEffectLayer(img, layer);
    }

    // 6. Legacy fallback if overlayText somehow still populated
    if (!edits.overlayText.trimmed().isEmpty()) {
        EffectLayer leg = EffectLayer::createText(edits.overlayText, 0.5, 0.86, std::max(16, edits.overlayTextSize));
        drawFreeTextLayer(img, leg);
    }

    return img;
}

} // namespace lumina
