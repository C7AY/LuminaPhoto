#pragma once

#include <QString>
#include <QStringList>

namespace lumina {

class WindowsFileAssociation {
public:
    static QStringList supportedImageExtensions();
    static bool isExtensionAssociatedWithLumina(const QString& ext);

    // Registers LuminaPhoto ProgId + Capabilities + RegisteredApplications in HKCU
    // (no UAC required) and notifies Windows Explorer.
    static bool registerFileTypes(const QStringList& extensions, QString* errorOut = nullptr);
    static bool registerFileTypes(QString* errorOut = nullptr) {
        return registerFileTypes(supportedImageExtensions(), errorOut);
    }

    // Opens Windows Settings -> Default Apps directly to Lumina Photo when supported.
    static bool openWindowsDefaultAppsSettings();
};

} // namespace lumina
