#pragma once

#include "PhotoModels.h"
#include <QDateTime>
#include <QList>
#include <QPair>
#include <QSqlDatabase>
#include <QString>
#include <functional>

namespace lumina {

struct ScannedPhotoMeta {
    QString filePath;
    QString fileName;
    QString folderPath;
    int width = 0;
    int height = 0;
    qint64 fileSizeBytes = 0;
    QDateTime dateTaken;
};

class LibraryDatabase {
public:
    explicit LibraryDatabase();
    ~LibraryDatabase();

    bool initialize();

    // Thread-safe static helper: walks folder and reads ONLY fast image header dimensions (no full pixel decode)
    static QList<ScannedPhotoMeta> collectFolderImages(
        const QString& folderPath,
        const std::function<bool(int foundCount, const QString& currentFile)>& progressCb = {});

    // Single-transaction batch insert on main DB thread (< 20ms for 1000+ photos)
    int indexScannedBatch(const QString& rootFolder, const QList<ScannedPhotoMeta>& items);

    // Synchronous convenience wrapper (uses fast header scan + single transaction)
    int scanFolder(const QString& folderPath);
    PhotoRecord ensurePhotoIndexed(const QString& filePath);

    // Queries
    QList<PhotoRecord> queryPhotos(const QString& filterFolder = {},
                                     qint64 filterAlbumId = 0,
                                     bool onlyStarred = false,
                                     const QString& searchText = {});
    QList<FolderSummary> listFolders();
    QList<AlbumRecord> listAlbums();
    QPair<int, int> countCollectionStats();

    // Mutations
    bool setStarred(const QString& filePath, bool starred);
    bool setCaption(const QString& filePath, const QString& caption);
    bool setEdits(const QString& filePath, const PhotoEdits& edits);
    bool clearPhotoMetadata(const QString& filePath);

    qint64 createAlbum(const QString& name, const QString& iconKey = {});
    bool setAlbumIcon(qint64 albumId, const QString& iconKey);
    bool addPhotoToAlbum(qint64 photoId, qint64 albumId);
    bool deleteAlbum(qint64 albumId);
    bool removePhotoFromLibrary(const QString& filePath);
    bool hasSubfoldersInLibrary(const QString& folderPath);
    QStringList removeFolderFromLibrary(const QString& folderPath, bool includeSubfolders = false);
    QList<QPair<qint64, QString>> listAllPhotoIdsAndPaths();
    QStringList removePhotosByIds(const QList<qint64>& ids);

private:
    QSqlDatabase db_;
    PhotoRecord rowToPhoto(const class QSqlQuery& q) const;
};

} // namespace lumina
