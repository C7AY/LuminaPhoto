#include "PsdParser.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <QFile>
#include <QPainter>
#include <QStringDecoder>
#include <QtEndian>
#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

namespace lumina {

namespace {

bool readBytes(QFile& file, void* dst, qint64 len) {
    if (len <= 0) return len == 0;
    return file.read(reinterpret_cast<char*>(dst), len) == len;
}

uint16_t readU16BE(QFile& file, bool& ok) {
    uint16_t v = 0;
    ok = readBytes(file, &v, 2);
    return qFromBigEndian(v);
}

uint32_t readU32BE(QFile& file, bool& ok) {
    uint32_t v = 0;
    ok = readBytes(file, &v, 4);
    return qFromBigEndian(v);
}

int16_t readI16BE(QFile& file, bool& ok) {
    return static_cast<int16_t>(readU16BE(file, ok));
}

int32_t readI32BE(QFile& file, bool& ok) {
    return static_cast<int32_t>(readU32BE(file, ok));
}

QString decodeAnsiFallback(const QByteArray& bytes) {
    if (bytes.isEmpty()) return QStringLiteral("Слой");
#ifdef _WIN32
    const int needed = MultiByteToWideChar(CP_ACP, 0, bytes.constData(), bytes.size(), nullptr, 0);
    if (needed > 0) {
        std::wstring wide(static_cast<size_t>(needed), L'\0');
        MultiByteToWideChar(CP_ACP, 0, bytes.constData(), bytes.size(), wide.data(), needed);
        return QString::fromStdWString(wide);
    }
#endif
    QStringDecoder dec(QStringDecoder::System);
    QString decoded = dec.decode(bytes);
    if (decoded.isEmpty()) {
        decoded = QString::fromLatin1(bytes);
    }
    return decoded;
}

bool decodePackBitsLine(const uint8_t* src, size_t srcLen, uint8_t* dst, size_t dstLen) {
    size_t si = 0;
    size_t di = 0;
    while (si < srcLen && di < dstLen) {
        const int8_t n = static_cast<int8_t>(src[si++]);
        if (n >= 0) {
            const size_t count = static_cast<size_t>(n) + 1;
            if (si + count > srcLen) break;
            const size_t copyCount = std::min(count, dstLen - di);
            std::copy_n(src + si, copyCount, dst + di);
            si += count;
            di += copyCount;
        } else if (n > -128) {
            if (si >= srcLen) break;
            const size_t count = static_cast<size_t>(1 - n);
            const uint8_t val = src[si++];
            const size_t fillCount = std::min(count, dstLen - di);
            std::fill_n(dst + di, fillCount, val);
            di += fillCount;
        }
    }
    return true;
}

struct ParsedChannelMeta {
    int16_t channelId = 0; // 0=R, 1=G, 2=B, -1=Transparency mask
    uint32_t dataLen = 0;
};

struct ParsedLayerHeader {
    QRect bounds;
    QString name;
    bool visible = true;
    uint8_t opacity = 255;
    std::vector<ParsedChannelMeta> channels;
};

} // namespace

PsdLoadResult PsdParser::load(const QString& filePath, bool readLayers) {
    PsdLoadResult res;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        res.errorMessage = QStringLiteral("Не удалось открыть PSD-файл.");
        return res;
    }

    char sig[4] = {};
    if (!readBytes(file, sig, 4) || std::string_view(sig, 4) != "8BPS") {
        res.errorMessage = QStringLiteral("Некорректная сигнатура PSD (ожидалось 8BPS).");
        return res;
    }

    bool ok = true;
    const uint16_t version = readU16BE(file, ok);
    if (!ok || (version != 1 && version != 2)) {
        res.errorMessage = QStringLiteral("Неподдерживаемая версия PSD.");
        return res;
    }

    file.seek(file.pos() + 6); // Reserved
    const uint16_t channels = readU16BE(file, ok);
    const uint32_t height = readU32BE(file, ok);
    const uint32_t width = readU32BE(file, ok);
    const uint16_t depth = readU16BE(file, ok);
    const uint16_t colorMode = readU16BE(file, ok);

    if (!ok || width == 0 || height == 0 || channels == 0) {
        res.errorMessage = QStringLiteral("Повреждённый заголовок PSD.");
        return res;
    }
    if (depth != 8 && depth != 16) {
        res.errorMessage = QStringLiteral("Поддерживаются только PSD 8 или 16 бит на канал.");
        return res;
    }
    if (version == 2) {
        res.errorMessage = QStringLiteral("Формат PSB (>2 ГБ) не поддерживается в быстром декодере.");
        return res;
    }

    // 2. Color Mode Data Section
    const uint32_t colorModeLen = readU32BE(file, ok);
    if (!ok) return res;
    file.seek(file.pos() + colorModeLen);

    // 3. Image Resources Section
    const uint32_t imgResLen = readU32BE(file, ok);
    if (!ok) return res;
    file.seek(file.pos() + imgResLen);

    // 4. Layer and Mask Information Section
    const uint32_t layerMaskLen = readU32BE(file, ok);
    if (!ok) return res;
    const qint64 afterLayerSectionPos = file.pos() + layerMaskLen;

    if (readLayers && layerMaskLen > 8) {
        const uint32_t layerInfoLen = readU32BE(file, ok);
        if (ok && layerInfoLen > 2) {
            int16_t rawLayerCount = readI16BE(file, ok);
            const int layerCount = std::abs(static_cast<int>(rawLayerCount));
            std::vector<ParsedLayerHeader> headers;
            headers.reserve(static_cast<size_t>(std::min(layerCount, 512)));

            for (int i = 0; i < layerCount && ok && file.pos() < afterLayerSectionPos; ++i) {
                ParsedLayerHeader lh;
                const int32_t top = readI32BE(file, ok);
                const int32_t left = readI32BE(file, ok);
                const int32_t bottom = readI32BE(file, ok);
                const int32_t right = readI32BE(file, ok);
                lh.bounds = QRect(left, top, std::max(0, right - left), std::max(0, bottom - top));

                const uint16_t numCh = readU16BE(file, ok);
                if (!ok) break;
                lh.channels.reserve(numCh);
                for (uint16_t c = 0; c < numCh && ok; ++c) {
                    ParsedChannelMeta cm;
                    cm.channelId = readI16BE(file, ok);
                    cm.dataLen = readU32BE(file, ok);
                    lh.channels.push_back(cm);
                }

                char blendSig[4] = {};
                readBytes(file, blendSig, 4); // 8BIM
                file.seek(file.pos() + 4);    // blend key
                uint8_t opacity = 255;
                readBytes(file, &opacity, 1);
                lh.opacity = opacity;
                uint8_t clipping = 0;
                readBytes(file, &clipping, 1);
                uint8_t flags = 0;
                readBytes(file, &flags, 1);
                lh.visible = (flags & 0x02) == 0;
                file.seek(file.pos() + 1); // filler

                const uint32_t extraLen = readU32BE(file, ok);
                if (!ok) break;
                const qint64 nextLayerPos = file.pos() + extraLen;

                const uint32_t maskLen = readU32BE(file, ok);
                if (ok) file.seek(file.pos() + maskLen);
                const uint32_t blendLen = readU32BE(file, ok);
                if (ok) file.seek(file.pos() + blendLen);

                QString ansiName;
                uint8_t nameLen = 0;
                if (ok && readBytes(file, &nameLen, 1)) {
                    const QByteArray nameBytes = file.read(nameLen);
                    ansiName = decodeAnsiFallback(nameBytes);
                    // Pascal string is padded to a multiple of 4 bytes (1 + nameLen + pad)
                    const int totalPascal = 1 + static_cast<int>(nameLen);
                    const int pad = (4 - (totalPascal % 4)) % 4;
                    if (pad > 0) file.seek(file.pos() + pad);
                }

                // Scan Additional Layer Information for 'luni' (UTF-16BE Unicode layer name), independent of 2/4-byte padding
                QString unicodeName;
                const qint64 remainingExtra = nextLayerPos - file.pos();
                if (ok && remainingExtra >= 16 && remainingExtra <= 4 * 1024 * 1024) {
                    const QByteArray extraTail = file.read(remainingExtra);
                    int luniPos = extraTail.indexOf("8BIMluni");
                    if (luniPos < 0) luniPos = extraTail.indexOf("8B64luni");
                    if (luniPos >= 0 && luniPos + 16 <= extraTail.size()) {
                        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(extraTail.constData() + luniPos);
                        const uint32_t blkLen = (static_cast<uint32_t>(ptr[8]) << 24) |
                                                (static_cast<uint32_t>(ptr[9]) << 16) |
                                                (static_cast<uint32_t>(ptr[10]) << 8) |
                                                static_cast<uint32_t>(ptr[11]);
                        const uint32_t charCount = (static_cast<uint32_t>(ptr[12]) << 24) |
                                                   (static_cast<uint32_t>(ptr[13]) << 16) |
                                                   (static_cast<uint32_t>(ptr[14]) << 8) |
                                                   static_cast<uint32_t>(ptr[15]);
                        if (charCount > 0 && charCount <= 1024 &&
                            blkLen >= 4 + charCount * 2 &&
                            luniPos + 16 + static_cast<int>(charCount) * 2 <= extraTail.size()) {
                            std::vector<char16_t> utf16(charCount);
                            for (uint32_t u = 0; u < charCount; ++u) {
                                const int off = luniPos + 16 + static_cast<int>(u) * 2;
                                utf16[u] = static_cast<char16_t>(
                                    (static_cast<uint16_t>(static_cast<uint8_t>(extraTail[off])) << 8) |
                                    static_cast<uint16_t>(static_cast<uint8_t>(extraTail[off + 1])));
                            }
                            unicodeName = QString::fromUtf16(utf16.data(), static_cast<qsizetype>(charCount));
                        }
                    }
                }

                lh.name = !unicodeName.isEmpty() ? unicodeName : ansiName;
                file.seek(nextLayerPos);
                headers.push_back(std::move(lh));
            }

            // Decode each layer's channel pixel data
            const size_t bytesPerSample = (depth == 16) ? 2 : 1;
            for (const ParsedLayerHeader& lh : headers) {
                const int lw = lh.bounds.width();
                const int lhPx = lh.bounds.height();
                PsdLayerInfo info;
                info.name = lh.name;
                info.bounds = lh.bounds;
                info.visible = lh.visible;

                bool canDecodePixels = (lw > 0 && lhPx > 0 && lw <= 8192 && lhPx <= 8192);
                const size_t planeSize = canDecodePixels ? static_cast<size_t>(lw) * static_cast<size_t>(lhPx) : 0;
                std::vector<uint8_t> chR, chG, chB, chA;
                bool hasR = false, hasG = false, hasB = false, hasA = false;

                for (const ParsedChannelMeta& cm : lh.channels) {
                    const qint64 nextChanPos = file.pos() + static_cast<qint64>(cm.dataLen);
                    const bool wantChannel = canDecodePixels &&
                                             (cm.channelId == 0 || cm.channelId == 1 ||
                                              cm.channelId == 2 || cm.channelId == -1) &&
                                             cm.dataLen >= 2;
                    if (!wantChannel) {
                        file.seek(nextChanPos);
                        continue;
                    }

                    bool chOk = true;
                    const uint16_t chComp = readU16BE(file, chOk);
                    if (!chOk || (chComp != 0 && chComp != 1)) {
                        file.seek(nextChanPos);
                        continue;
                    }

                    std::vector<uint8_t> plane(planeSize, cm.channelId == -1 ? 255 : 0);
                    const size_t rowBytes = static_cast<size_t>(lw) * bytesPerSample;

                    if (chComp == 0) {
                        std::vector<uint8_t> rowBuf(rowBytes);
                        for (int y = 0; y < lhPx; ++y) {
                            if (!readBytes(file, rowBuf.data(), static_cast<qint64>(rowBytes))) break;
                            uint8_t* dst = plane.data() + static_cast<size_t>(y) * lw;
                            for (int x = 0; x < lw; ++x) {
                                dst[x] = rowBuf[x * bytesPerSample];
                            }
                        }
                    } else {
                        std::vector<uint16_t> counts(static_cast<size_t>(lhPx));
                        for (int y = 0; y < lhPx && chOk; ++y) {
                            counts[static_cast<size_t>(y)] = readU16BE(file, chOk);
                        }
                        if (chOk) {
                            std::vector<uint8_t> compRow;
                            std::vector<uint8_t> decRow(rowBytes);
                            for (int y = 0; y < lhPx; ++y) {
                                const uint16_t cnt = counts[static_cast<size_t>(y)];
                                compRow.resize(cnt);
                                if (!readBytes(file, compRow.data(), cnt)) break;
                                decodePackBitsLine(compRow.data(), cnt, decRow.data(), rowBytes);
                                uint8_t* dst = plane.data() + static_cast<size_t>(y) * lw;
                                for (int x = 0; x < lw; ++x) {
                                    dst[x] = decRow[x * bytesPerSample];
                                }
                            }
                        }
                    }

                    if (cm.channelId == 0) { chR = std::move(plane); hasR = true; }
                    else if (cm.channelId == 1) { chG = std::move(plane); hasG = true; }
                    else if (cm.channelId == 2) { chB = std::move(plane); hasB = true; }
                    else if (cm.channelId == -1) { chA = std::move(plane); hasA = true; }

                    file.seek(nextChanPos);
                }

                if (canDecodePixels && (hasR || hasG || hasB)) {
                    QImage layerImg(lw, lhPx, QImage::Format_RGBA8888);
                    for (int y = 0; y < lhPx; ++y) {
                        uint8_t* dst = layerImg.scanLine(y);
                        const size_t rowOff = static_cast<size_t>(y) * lw;
                        for (int x = 0; x < lw; ++x) {
                            const size_t idx = rowOff + x;
                            const uint8_t r = hasR ? chR[idx] : 0;
                            const uint8_t g = hasG ? chG[idx] : r;
                            const uint8_t b = hasB ? chB[idx] : r;
                            const uint8_t a = hasA ? chA[idx] : 255;
                            dst[0] = r;
                            dst[1] = g;
                            dst[2] = b;
                            dst[3] = static_cast<uint8_t>((static_cast<int>(a) * lh.opacity) / 255);
                            dst += 4;
                        }
                    }
                    info.image = std::move(layerImg);
                }

                res.layerNames.append(info.name);
                res.layers.append(std::move(info));
            }
        }
    }
    file.seek(afterLayerSectionPos);

    // 5. Composite Image Data
    const uint16_t compression = readU16BE(file, ok);
    if (!ok || (compression != 0 && compression != 1)) {
        res.errorMessage = QStringLiteral("Неподдерживаемый метод сжатия композита PSD.");
        return res;
    }

    const size_t bytesPerSample = (depth == 16) ? 2 : 1;
    const size_t lineBytes = static_cast<size_t>(width) * bytesPerSample;
    const size_t planePixels = static_cast<size_t>(width) * height;
    const int planesToRead = std::min<int>(channels, 4);

    std::vector<std::vector<uint8_t>> planes(planesToRead, std::vector<uint8_t>(planePixels, 255));

    if (compression == 0) {
        std::vector<uint8_t> rowBuf(lineBytes);
        for (int c = 0; c < planesToRead; ++c) {
            for (uint32_t y = 0; y < height; ++y) {
                if (!readBytes(file, rowBuf.data(), static_cast<qint64>(lineBytes))) break;
                uint8_t* dstRow = planes[c].data() + static_cast<size_t>(y) * width;
                for (uint32_t x = 0; x < width; ++x) {
                    dstRow[x] = rowBuf[x * bytesPerSample];
                }
            }
        }
    } else {
        const size_t totalScanlines = static_cast<size_t>(channels) * height;
        std::vector<uint16_t> byteCounts(totalScanlines);
        for (size_t i = 0; i < totalScanlines; ++i) {
            byteCounts[i] = readU16BE(file, ok);
            if (!ok) {
                res.errorMessage = QStringLiteral("Ошибка чтения таблицы RLE PSD.");
                return res;
            }
        }

        std::vector<uint8_t> compressedRow;
        std::vector<uint8_t> decodedRow(lineBytes);
        for (int c = 0; c < channels; ++c) {
            for (uint32_t y = 0; y < height; ++y) {
                const uint16_t cnt = byteCounts[static_cast<size_t>(c) * height + y];
                if (c < planesToRead) {
                    compressedRow.resize(cnt);
                    if (!readBytes(file, compressedRow.data(), cnt)) break;
                    decodePackBitsLine(compressedRow.data(), cnt, decodedRow.data(), lineBytes);
                    uint8_t* dstRow = planes[c].data() + static_cast<size_t>(y) * width;
                    for (uint32_t x = 0; x < width; ++x) {
                        dstRow[x] = decodedRow[x * bytesPerSample];
                    }
                } else {
                    file.seek(file.pos() + cnt);
                }
            }
        }
    }

    QImage img(static_cast<int>(width), static_cast<int>(height), QImage::Format_RGBA8888);
    for (uint32_t y = 0; y < height; ++y) {
        uint8_t* dst = img.scanLine(static_cast<int>(y));
        const size_t rowOffset = static_cast<size_t>(y) * width;
        for (uint32_t x = 0; x < width; ++x) {
            const size_t idx = rowOffset + x;
            if (colorMode == 1 || planesToRead == 1) {
                const uint8_t g = planes[0][idx];
                dst[0] = dst[1] = dst[2] = g;
                dst[3] = (planesToRead >= 2) ? planes[1][idx] : 255;
            } else if (planesToRead >= 3) {
                dst[0] = planes[0][idx];
                dst[1] = planes[1][idx];
                dst[2] = planes[2][idx];
                dst[3] = (planesToRead == 4) ? planes[3][idx] : 255;
            }
            dst += 4;
        }
    }

    res.image = std::move(img);
    res.success = true;
    return res;
}

QSize PsdParser::readHeaderDimensions(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return QSize();
    char hdr[26];
    if (file.read(hdr, 26) != 26) return QSize();
    if (hdr[0] != '8' || hdr[1] != 'B' || hdr[2] != 'P' || hdr[3] != 'S') return QSize();
    const uint32_t h = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar*>(hdr + 14));
    const uint32_t w = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar*>(hdr + 18));
    return QSize(static_cast<int>(w), static_cast<int>(h));
}

} // namespace lumina
