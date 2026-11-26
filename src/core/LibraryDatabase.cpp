#include "LibraryDatabase.h"
#include "ImageLoader.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace lumina {

LibraryDatabase::LibraryDatabase() = default;

LibraryDatabase::~LibraryDatabase() {
    if (db_.isOpen()) db_.close();
}

bool LibraryDatabase::initialize() {
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataDir);
    const QString dbPath = dataDir + QStringLiteral("/lumina_library.db");

    if (QSqlDatabase::contains(QStringLiteral("lumina_conn"))) {
        db_ = QSqlDatabase::database(QStringLiteral("lumina_conn"));
    } else {
        db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("lumina_conn"));
        db_.setDatabaseName(dbPath);
    }

    if (!db_.open()) return false;

    QSqlQuery q(db_);
    q.exec(QStringLiteral("PRAGMA foreign_keys = ON;"));
    q.exec(QStringLiteral("PRAGMA journal_mode = WAL;"));
    q.exec(QStringLiteral("PRAGMA synchronous = NORMAL;"));

    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS watched_folders ("
        "  folder_path TEXT PRIMARY KEY,"
        "  added_at TEXT NOT NULL"
        ");"
    ));

    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS photos ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  file_path TEXT UNIQUE NOT NULL,"
        "  file_name TEXT NOT NULL,"
        "  folder_path TEXT NOT NULL,"
        "  caption TEXT DEFAULT '',"
        "  starred INTEGER DEFAULT 0,"
        "  width INTEGER DEFAULT 0,"
        "  height INTEGER DEFAULT 0,"
        "  file_size INTEGER DEFAULT 0,"
        "  date_taken TEXT NOT NULL,"
        "  camera TEXT DEFAULT '',"
        "  aperture TEXT DEFAULT '',"
        "  shutter TEXT DEFAULT '',"
        "  focal_length TEXT DEFAULT '',"
        "  iso INTEGER DEFAULT 0,"
        "  edits_json TEXT DEFAULT '{}'"
        ");"
    ));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_photos_folder ON photos(folder_path);"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_photos_starred ON photos(starred);"));

    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS albums ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT UNIQUE NOT NULL,"
        "  icon_key TEXT DEFAULT '',"
        "  created_at TEXT NOT NULL"
        ");"
    ));

    {
        bool hasIconKey = false;
        QSqlQuery info(db_);
        if (info.exec(QStringLiteral("PRAGMA table_info(albums)"))) {
            while (info.next()) {
                if (info.value(1).toString() == QLatin1String("icon_key")) {
                    hasIconKey = true;
                    break;
                }
            }
        }
        if (!hasIconKey) {
            q.exec(QStringLiteral("ALTER TABLE albums ADD COLUMN icon_key TEXT DEFAULT '';"));
        }
    }

    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS album_photos ("
        "  album_id INTEGER NOT NULL REFERENCES albums(id) ON DELETE CASCADE,"
        "  photo_id INTEGER NOT NULL REFERENCES photos(id) ON DELETE CASCADE,"
        "  PRIMARY KEY (album_id, photo_id)"
        ");"
    ));

    return true;
}

QList<ScannedPhotoMeta> LibraryDatabase::collectFolderImages(
    const QString& folderPath,
    const std::function<bool(int foundCount, const QString& currentFile)>& progressCb) {
    QList<ScannedPhotoMeta> out;
    const QString cleanRoot = QDir::toNativeSeparators(QFileInfo(folderPath).absoluteFilePath());
    QDirIterator it(cleanRoot, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = it.next();
        if (!ImageLoader::isSupportedFile(file)) continue;

        const QFileInfo fi(file);
        const QSize dims = ImageLoader::readHeaderDimensions(file);

        ScannedPhotoMeta meta;
        meta.filePath = QDir::toNativeSeparators(fi.absoluteFilePath());
        meta.fileName = fi.fileName();
        meta.folderPath = QDir::toNativeSeparators(fi.absolutePath());
        meta.width = std::max(0, dims.width());
        meta.height = std::max(0, dims.height());
        meta.fileSizeBytes = fi.size();
        meta.dateTaken = fi.lastModified();
        out.append(std::move(meta));

        if (progressCb) {
            if (!progressCb(out.size(), meta.fileName)) {
                break;
            }
        }
    }
    return out;
}

int LibraryDatabase::indexScannedBatch(const QString& rootFolder, const QList<ScannedPhotoMeta>& items) {
    const QString cleanRoot = QDir::toNativeSeparators(QFileInfo(rootFolder).absoluteFilePath());
    db_.transaction();

    QSqlQuery wf(db_);
    wf.prepare(QStringLiteral("INSERT OR IGNORE INTO watched_folders (folder_path, added_at) VALUES (:dir, :dt)"));
    wf.bindValue(":dir", cleanRoot);
    wf.bindValue(":dt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    wf.exec();

    QSqlQuery ins(db_);
    ins.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO photos "
        "(file_path, file_name, folder_path, width, height, file_size, date_taken) "
        "VALUES (:fp, :fn, :dir, :w, :h, :sz, :dt)"
    ));

    QSet<QString> seenDirs;
    const QString nowStr = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    for (const ScannedPhotoMeta& item : items) {
        ins.bindValue(":fp", item.filePath);
        ins.bindValue(":fn", item.fileName);
        ins.bindValue(":dir", item.folderPath);
        ins.bindValue(":w", item.width);
        ins.bindValue(":h", item.height);
        ins.bindValue(":sz", item.fileSizeBytes);
        ins.bindValue(":dt", item.dateTaken.toString(Qt::ISODate));
        ins.exec();
        seenDirs.insert(item.folderPath);
    }

    for (const QString& dir : seenDirs) {
        wf.bindValue(":dir", dir);
        wf.bindValue(":dt", nowStr);
        wf.exec();
    }

    db_.commit();
    return items.size();
}

PhotoRecord LibraryDatabase::ensurePhotoIndexed(const QString& filePath) {
    const QString cleanPath = QDir::toNativeSeparators(QFileInfo(filePath).absoluteFilePath());
    QSqlQuery check(db_);
    check.prepare(QStringLiteral("SELECT * FROM photos WHERE file_path = :fp"));
    check.bindValue(":fp", cleanPath);
    if (check.exec() && check.next()) {
        return rowToPhoto(check);
    }

    const QFileInfo fi(cleanPath);
    if (!fi.exists() || !fi.isFile()) {
        return PhotoRecord{};
    }
    const QSize dims = ImageLoader::readHeaderDimensions(cleanPath);
    const ExifMetadata meta = ImageLoader::readMetadata(cleanPath);

    QSqlQuery ins(db_);
    ins.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO photos "
        "(file_path, file_name, folder_path, width, height, file_size, date_taken, camera, aperture, shutter, focal_length, iso) "
        "VALUES (:fp, :fn, :dir, :w, :h, :sz, :dt, :cam, :ap, :sh, :fl, :iso)"
    ));
    ins.bindValue(":fp", cleanPath);
    ins.bindValue(":fn", fi.fileName());
    ins.bindValue(":dir", QDir::toNativeSeparators(fi.absolutePath()));
    ins.bindValue(":w", std::max(0, dims.width()));
    ins.bindValue(":h", std::max(0, dims.height()));
    ins.bindValue(":sz", fi.size());
    ins.bindValue(":dt", fi.lastModified().toString(Qt::ISODate));
    ins.bindValue(":cam", meta.camera);
    ins.bindValue(":ap", meta.aperture);
    ins.bindValue(":sh", meta.shutterSpeed);
    ins.bindValue(":fl", meta.focalLength);
    ins.bindValue(":iso", meta.iso);
    ins.exec();

    QSqlQuery wf(db_);
    wf.prepare(QStringLiteral("INSERT OR IGNORE INTO watched_folders (folder_path, added_at) VALUES (:dir, :dt)"));
    wf.bindValue(":dir", QDir::toNativeSeparators(fi.absolutePath()));
    wf.bindValue(":dt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    wf.exec();

    check.exec();
    if (check.next()) return rowToPhoto(check);
    return PhotoRecord{};
}

int LibraryDatabase::scanFolder(const QString& folderPath) {
    const QList<ScannedPhotoMeta> items = collectFolderImages(folderPath);
    return indexScannedBatch(folderPath, items);
}

QList<PhotoRecord> LibraryDatabase::queryPhotos(const QString& filterFolder,
                                                qint64 filterAlbumId,
                                                bool onlyStarred,
                                                const QString& searchText) {
    QList<PhotoRecord> out;
    QString sql = QStringLiteral("SELECT p.* FROM photos p ");
    QStringList where;

    if (filterAlbumId > 0) {
        sql += QStringLiteral("INNER JOIN album_photos ap ON ap.photo_id = p.id ");
        where << QStringLiteral("ap.album_id = :aid");
    }
    if (!filterFolder.isEmpty()) {
        where << QStringLiteral("LOWER(p.folder_path) = LOWER(:dir)");
    }
    if (onlyStarred) {
        where << QStringLiteral("p.starred = 1");
    }
    if (!searchText.trimmed().isEmpty()) {
        where << QStringLiteral("(p.file_name LIKE :q OR p.caption LIKE :q OR p.camera LIKE :q)");
    }

    if (!where.isEmpty()) {
        sql += QStringLiteral("WHERE ") + where.join(QStringLiteral(" AND "));
    }
    sql += QStringLiteral(" ORDER BY p.date_taken DESC");

    QSqlQuery q(db_);
    q.prepare(sql);
    if (filterAlbumId > 0) q.bindValue(":aid", filterAlbumId);
    if (!filterFolder.isEmpty()) q.bindValue(":dir", QDir::toNativeSeparators(QFileInfo(filterFolder).absoluteFilePath()));
    if (!searchText.trimmed().isEmpty()) {
        q.bindValue(":q", QString("%%1%").arg(searchText.trimmed()));
    }

    if (q.exec()) {
        while (q.next()) {
            out.append(rowToPhoto(q));
        }
    }
    return out;
}

QList<FolderSummary> LibraryDatabase::listFolders() {
    QList<FolderSummary> out;
    QSqlQuery q(db_);
    if (q.exec(QStringLiteral(
            "SELECT folder_path, COUNT(*) AS cnt, MIN(file_path) AS preview_fp "
            "FROM photos GROUP BY folder_path ORDER BY folder_path ASC"))) {
        while (q.next()) {
            FolderSummary fs;
            fs.folderPath = q.value(0).toString();
            fs.displayName = QFileInfo(fs.folderPath).fileName();
            if (fs.displayName.isEmpty()) fs.displayName = fs.folderPath;
            fs.photoCount = q.value(1).toInt();
            fs.previewFilePath = q.value(2).toString();
            out.append(fs);
        }
    }
    return out;
}

QList<AlbumRecord> LibraryDatabase::listAlbums() {
    QList<AlbumRecord> out;
    QSqlQuery q(db_);
    if (q.exec(QStringLiteral(
            "SELECT a.id, a.name, COALESCE(a.icon_key, ''), COUNT(ap.photo_id) AS cnt, MIN(p.file_path) AS preview_fp "
            "FROM albums a "
            "LEFT JOIN album_photos ap ON ap.album_id = a.id "
            "LEFT JOIN photos p ON p.id = ap.photo_id "
            "GROUP BY a.id ORDER BY a.name ASC"))) {
        while (q.next()) {
            AlbumRecord rec;
            rec.id = q.value(0).toLongLong();
            rec.name = q.value(1).toString();
            rec.iconKey = q.value(2).toString();
            rec.photoCount = q.value(3).toInt();
            rec.previewFilePath = q.value(4).toString();
            out.append(rec);
        }
    }
    return out;
}

QPair<int, int> LibraryDatabase::countCollectionStats() {
    int total = 0;
    int starred = 0;
    QSqlQuery q(db_);
    if (q.exec(QStringLiteral("SELECT COUNT(*), COALESCE(SUM(CASE WHEN starred = 1 THEN 1 ELSE 0 END), 0) FROM photos")) && q.next()) {
        total = q.value(0).toInt();
        starred = q.value(1).toInt();
    }
    return {total, starred};
}

bool LibraryDatabase::setStarred(const QString& filePath, bool starred) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("UPDATE photos SET starred = :s WHERE file_path = :fp"));
    q.bindValue(":s", starred ? 1 : 0);
    q.bindValue(":fp", QDir::toNativeSeparators(QFileInfo(filePath).absoluteFilePath()));
    return q.exec();
}

bool LibraryDatabase::setCaption(const QString& filePath, const QString& caption) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("UPDATE photos SET caption = :c WHERE file_path = :fp"));
    q.bindValue(":c", caption);
    q.bindValue(":fp", QDir::toNativeSeparators(QFileInfo(filePath).absoluteFilePath()));
    return q.exec();
}

bool LibraryDatabase::setEdits(const QString& filePath, const PhotoEdits& edits) {
    const QByteArray rawJson = QJsonDocument(edits.toJson()).toJson(QJsonDocument::Compact);
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("UPDATE photos SET edits_json = :ej WHERE file_path = :fp"));
    q.bindValue(":ej", QString::fromUtf8(rawJson));
    q.bindValue(":fp", QDir::toNativeSeparators(QFileInfo(filePath).absoluteFilePath()));
    return q.exec();
}

bool LibraryDatabase::clearPhotoMetadata(const QString& filePath) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral(
        "UPDATE photos SET camera = '', aperture = '', shutter = '', focal_length = '', iso = 0 WHERE file_path = :fp"));
    q.bindValue(":fp", QDir::toNativeSeparators(QFileInfo(filePath).absoluteFilePath()));
    return q.exec();
}

qint64 LibraryDatabase::createAlbum(const QString& name, const QString& iconKey) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("INSERT OR IGNORE INTO albums (name, icon_key, created_at) VALUES (:n, :ik, :dt)"));
    q.bindValue(":n", name.trimmed());
    q.bindValue(":ik", iconKey.trimmed());
    q.bindValue(":dt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!q.exec()) return 0;

    QSqlQuery sel(db_);
    sel.prepare(QStringLiteral("SELECT id FROM albums WHERE name = :n"));
    sel.bindValue(":n", name.trimmed());
    if (sel.exec() && sel.next()) return sel.value(0).toLongLong();
    return 0;
}

bool LibraryDatabase::setAlbumIcon(qint64 albumId, const QString& iconKey) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("UPDATE albums SET icon_key = :ik WHERE id = :aid"));
    q.bindValue(":ik", iconKey.trimmed());
    q.bindValue(":aid", albumId);
    return q.exec();
}

bool LibraryDatabase::addPhotoToAlbum(qint64 photoId, qint64 albumId) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("INSERT OR IGNORE INTO album_photos (album_id, photo_id) VALUES (:aid, :pid)"));
    q.bindValue(":aid", albumId);
    q.bindValue(":pid", photoId);
    return q.exec();
}

bool LibraryDatabase::deleteAlbum(qint64 albumId) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("DELETE FROM albums WHERE id = :aid"));
    q.bindValue(":aid", albumId);
    return q.exec();
}

bool LibraryDatabase::removePhotoFromLibrary(const QString& filePath) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("DELETE FROM photos WHERE LOWER(file_path) = LOWER(:fp)"));
    q.bindValue(":fp", QDir::toNativeSeparators(QFileInfo(filePath).absoluteFilePath()));
    return q.exec();
}

bool LibraryDatabase::hasSubfoldersInLibrary(const QString& folderPath) {
    QString clean = QDir::toNativeSeparators(QFileInfo(folderPath).absoluteFilePath());
    while (clean.endsWith(QLatin1Char('\\')) || clean.endsWith(QLatin1Char('/'))) {
        if (clean.size() <= 3 && clean.contains(QLatin1Char(':'))) break;
        clean.chop(1);
    }
    const QString sep = QString(QDir::separator());
    const QString prefix = clean.endsWith(sep) ? (clean + "%") : (clean + sep + "%");
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("SELECT 1 FROM photos WHERE LOWER(folder_path) LIKE LOWER(:pfx) LIMIT 1"));
    q.bindValue(":pfx", prefix);
    return q.exec() && q.next();
}

QStringList LibraryDatabase::removeFolderFromLibrary(const QString& folderPath, bool includeSubfolders) {
    QStringList removedPaths;
    QString clean = QDir::toNativeSeparators(QFileInfo(folderPath).absoluteFilePath());
    while (clean.endsWith(QLatin1Char('\\')) || clean.endsWith(QLatin1Char('/'))) {
        if (clean.size() <= 3 && clean.contains(QLatin1Char(':'))) break;
        clean.chop(1);
    }
    const QString sep = QString(QDir::separator());
    const QString prefix = clean.endsWith(sep) ? (clean + "%") : (clean + sep + "%");

    QSqlQuery sel(db_);
    if (includeSubfolders) {
        sel.prepare(QStringLiteral(
            "SELECT file_path FROM photos WHERE LOWER(folder_path) = LOWER(:dir) OR LOWER(folder_path) LIKE LOWER(:pfx)"));
        sel.bindValue(":dir", clean);
        sel.bindValue(":pfx", prefix);
    } else {
        sel.prepare(QStringLiteral("SELECT file_path FROM photos WHERE LOWER(folder_path) = LOWER(:dir)"));
        sel.bindValue(":dir", clean);
    }
    if (sel.exec()) {
        while (sel.next()) {
            removedPaths.append(sel.value(0).toString());
        }
    }

    db_.transaction();
    QSqlQuery delP(db_);
    if (includeSubfolders) {
        delP.prepare(QStringLiteral(
            "DELETE FROM photos WHERE LOWER(folder_path) = LOWER(:dir) OR LOWER(folder_path) LIKE LOWER(:pfx)"));
        delP.bindValue(":dir", clean);
        delP.bindValue(":pfx", prefix);
    } else {
        delP.prepare(QStringLiteral("DELETE FROM photos WHERE LOWER(folder_path) = LOWER(:dir)"));
        delP.bindValue(":dir", clean);
    }
    delP.exec();

    QSqlQuery delW(db_);
    if (includeSubfolders) {
        delW.prepare(QStringLiteral(
            "DELETE FROM watched_folders WHERE LOWER(folder_path) = LOWER(:dir) OR LOWER(folder_path) LIKE LOWER(:pfx)"));
        delW.bindValue(":dir", clean);
        delW.bindValue(":pfx", prefix);
    } else {
        delW.prepare(QStringLiteral("DELETE FROM watched_folders WHERE LOWER(folder_path) = LOWER(:dir)"));
        delW.bindValue(":dir", clean);
    }
    delW.exec();
    db_.commit();
    return removedPaths;
}

QList<QPair<qint64, QString>> LibraryDatabase::listAllPhotoIdsAndPaths() {
    QList<QPair<qint64, QString>> out;
    QSqlQuery q(db_);
    if (q.exec(QStringLiteral("SELECT id, file_path FROM photos"))) {
        while (q.next()) {
            out.append({q.value(0).toLongLong(), q.value(1).toString()});
        }
    }
    return out;
}

QStringList LibraryDatabase::removePhotosByIds(const QList<qint64>& ids) {
    QStringList removedPaths;
    if (ids.isEmpty()) return removedPaths;

    db_.transaction();
    QSqlQuery sel(db_);
    sel.prepare(QStringLiteral("SELECT file_path FROM photos WHERE id = :id"));
    QSqlQuery del(db_);
    del.prepare(QStringLiteral("DELETE FROM photos WHERE id = :id"));
    for (qint64 id : ids) {
        sel.bindValue(":id", id);
        if (sel.exec() && sel.next()) {
            removedPaths.append(sel.value(0).toString());
        }
        del.bindValue(":id", id);
        del.exec();
    }
    db_.commit();
    return removedPaths;
}

PhotoRecord LibraryDatabase::rowToPhoto(const QSqlQuery& q) const {
    PhotoRecord r;
    r.id = q.value("id").toLongLong();
    r.filePath = q.value("file_path").toString();
    r.fileName = q.value("file_name").toString();
    r.folderPath = q.value("folder_path").toString();
    r.caption = q.value("caption").toString();
    r.starred = q.value("starred").toInt() != 0;
    r.width = q.value("width").toInt();
    r.height = q.value("height").toInt();
    r.fileSizeBytes = q.value("file_size").toLongLong();
    r.dateTaken = QDateTime::fromString(q.value("date_taken").toString(), Qt::ISODate);
    r.exif.camera = q.value("camera").toString();
    r.exif.aperture = q.value("aperture").toString();
    r.exif.shutterSpeed = q.value("shutter").toString();
    r.exif.focalLength = q.value("focal_length").toString();
    r.exif.iso = q.value("iso").toInt();

    const QByteArray jsonBytes = q.value("edits_json").toString().toUtf8();
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (doc.isObject()) {
        r.edits = PhotoEdits::fromJson(doc.object());
    }
    return r;
}

} // namespace lumina
