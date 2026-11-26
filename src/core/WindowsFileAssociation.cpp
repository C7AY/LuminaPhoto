#include "WindowsFileAssociation.h"

#include <QCoreApplication>
#include <QDir>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <cstdint>
#endif

namespace lumina {

QStringList WindowsFileAssociation::supportedImageExtensions() {
    return {
        QStringLiteral(".jpg"), QStringLiteral(".jpeg"), QStringLiteral(".jfif"),
        QStringLiteral(".png"), QStringLiteral(".webp"), QStringLiteral(".gif"),
        QStringLiteral(".svg"), QStringLiteral(".svgz"), QStringLiteral(".ico"),
        QStringLiteral(".bmp"), QStringLiteral(".tif"), QStringLiteral(".tiff"),
        QStringLiteral(".psd"), QStringLiteral(".avif"), QStringLiteral(".heic"),
        QStringLiteral(".heif"), QStringLiteral(".tga")
    };
}

#ifdef _WIN32
static bool writeRegistryString(const QString& subKey, const QString& valueName, const QString& data) {
    HKEY hKey = nullptr;
    const std::wstring wSubKey = subKey.toStdWString();
    if (RegCreateKeyExW(HKEY_CURRENT_USER, wSubKey.c_str(), 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
        return false;
    }

    const std::wstring wName = valueName.toStdWString();
    const std::wstring wData = data.toStdWString();
    const LONG res = RegSetValueExW(
        hKey,
        valueName.isEmpty() ? nullptr : wName.c_str(),
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(wData.c_str()),
        static_cast<DWORD>((wData.size() + 1) * sizeof(wchar_t))
    );
    RegCloseKey(hKey);
    return res == ERROR_SUCCESS;
}

static QString readRegistryString(HKEY root, const QString& subKey, const QString& valueName) {
    HKEY hKey = nullptr;
    const std::wstring wSubKey = subKey.toStdWString();
    if (RegOpenKeyExW(root, wSubKey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return {};
    }

    DWORD type = 0;
    DWORD cbData = 0;
    const std::wstring wName = valueName.toStdWString();
    const wchar_t* valPtr = valueName.isEmpty() ? nullptr : wName.c_str();
    if (RegQueryValueExW(hKey, valPtr, nullptr, &type, nullptr, &cbData) != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ) || cbData == 0) {
        RegCloseKey(hKey);
        return {};
    }

    std::wstring buf(cbData / sizeof(wchar_t) + 1, L'\0');
    if (RegQueryValueExW(hKey, valPtr, nullptr, &type, reinterpret_cast<BYTE*>(buf.data()), &cbData) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return {};
    }
    RegCloseKey(hKey);
    return QString::fromWCharArray(buf.c_str()).trimmed();
}
#endif

bool WindowsFileAssociation::isExtensionAssociatedWithLumina(const QString& ext) {
#ifdef _WIN32
    QString cleanExt = ext.trimmed().toLower();
    if (!cleanExt.startsWith(QLatin1Char('.'))) {
        cleanExt.prepend(QLatin1Char('.'));
    }
    const QString userChoice = readRegistryString(
        HKEY_CURRENT_USER,
        QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%1\\UserChoice").arg(cleanExt),
        QStringLiteral("ProgId"));
    if (!userChoice.isEmpty()) {
        return userChoice.startsWith(QStringLiteral("LuminaPhoto."), Qt::CaseInsensitive);
    }
    const QString hkcuClass = readRegistryString(
        HKEY_CURRENT_USER,
        QStringLiteral("Software\\Classes\\%1").arg(cleanExt),
        QString());
    if (!hkcuClass.isEmpty()) {
        return hkcuClass.startsWith(QStringLiteral("LuminaPhoto."), Qt::CaseInsensitive);
    }
    const QString hkcrClass = readRegistryString(
        HKEY_CLASSES_ROOT,
        cleanExt,
        QString());
    return hkcrClass.startsWith(QStringLiteral("LuminaPhoto."), Qt::CaseInsensitive);
#else
    Q_UNUSED(ext);
    return false;
#endif
}

bool WindowsFileAssociation::registerFileTypes(const QStringList& extensions, QString* errorOut) {
#ifdef _WIN32
    const QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString progId = QStringLiteral("LuminaPhoto.ImageFile");

    if (!writeRegistryString(QStringLiteral("Software\\Classes\\") + progId, QString(),
                             QStringLiteral("Lumina Photo Image"))) {
        if (errorOut) *errorOut = QStringLiteral("Не удалось записать ProgId в реестр.");
        return false;
    }
    writeRegistryString(QStringLiteral("Software\\Classes\\") + progId + QStringLiteral("\\DefaultIcon"),
                        QString(), QStringLiteral("\"") + exePath + QStringLiteral("\",0"));
    writeRegistryString(QStringLiteral("Software\\Classes\\") + progId + QStringLiteral("\\shell\\open\\command"),
                        QString(), QStringLiteral("\"") + exePath + QStringLiteral("\" \"%1\""));

    // Clear previous Capabilities\FileAssociations so only user-selected extensions appear
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Lumina\\LuminaPhoto\\Capabilities\\FileAssociations");

    writeRegistryString(QStringLiteral("Software\\Lumina\\LuminaPhoto\\Capabilities"),
                        QStringLiteral("ApplicationName"), QStringLiteral("Lumina Photo"));
    writeRegistryString(QStringLiteral("Software\\Lumina\\LuminaPhoto\\Capabilities"),
                        QStringLiteral("ApplicationDescription"),
                        QStringLiteral("Быстрый просмотрщик и редактор фотографий (JPG, PNG, WebP, GIF, SVG, ICO, PSD, AVIF, HEIC)"));
    writeRegistryString(QStringLiteral("Software\\Lumina\\LuminaPhoto\\Capabilities"),
                        QStringLiteral("ApplicationIcon"),
                        QStringLiteral("\"") + exePath + QStringLiteral("\",0"));

    const QStringList cleanExts = extensions.isEmpty() ? supportedImageExtensions() : extensions;
    for (const QString& rawExt : cleanExts) {
        QString ext = rawExt.trimmed().toLower();
        if (ext.isEmpty()) continue;
        if (!ext.startsWith(QLatin1Char('.'))) ext.prepend(QLatin1Char('.'));

        writeRegistryString(QStringLiteral("Software\\Classes\\%1\\OpenWithProgids").arg(ext), progId, QString());
        writeRegistryString(QStringLiteral("Software\\Classes\\Applications\\LuminaPhoto.exe\\SupportedTypes"), ext, QString());
        writeRegistryString(QStringLiteral("Software\\Lumina\\LuminaPhoto\\Capabilities\\FileAssociations"), ext, progId);
    }

    writeRegistryString(QStringLiteral("Software\\Classes\\Applications\\LuminaPhoto.exe"),
                        QStringLiteral("FriendlyAppName"), QStringLiteral("Lumina Photo"));
    writeRegistryString(QStringLiteral("Software\\Classes\\Applications\\LuminaPhoto.exe\\shell\\open\\command"),
                        QString(), QStringLiteral("\"") + exePath + QStringLiteral("\" \"%1\""));

    writeRegistryString(QStringLiteral("Software\\RegisteredApplications"),
                        QStringLiteral("LuminaPhoto"),
                        QStringLiteral("Software\\Lumina\\LuminaPhoto\\Capabilities"));

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
#else
    Q_UNUSED(extensions);
    if (errorOut) *errorOut = QStringLiteral("Доступно только в Windows.");
    return false;
#endif
}

bool WindowsFileAssociation::openWindowsDefaultAppsSettings() {
#ifdef _WIN32
    HINSTANCE h = ShellExecuteW(
        nullptr,
        L"open",
        L"ms-settings:defaultapps?registeredAppUser=LuminaPhoto",
        nullptr,
        nullptr,
        SW_SHOWNORMAL
    );
    if (reinterpret_cast<std::intptr_t>(h) > 32) {
        return true;
    }
    h = ShellExecuteW(
        nullptr,
        L"open",
        L"ms-settings:defaultapps",
        nullptr,
        nullptr,
        SW_SHOWNORMAL
    );
    return reinterpret_cast<std::intptr_t>(h) > 32;
#else
    return false;
#endif
}

} // namespace lumina
