#include "AppSettings.h"

#include <QFont>
#include <QGuiApplication>
#include <QLocale>
#include <QPalette>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>

#ifdef _WIN32
#include <windows.h>
#endif

namespace lumina {

namespace {

bool detectSystemDarkMode() {
    if (const QStyleHints* hints = QGuiApplication::styleHints()) {
        const Qt::ColorScheme cs = hints->colorScheme();
        if (cs == Qt::ColorScheme::Dark) return true;
        if (cs == Qt::ColorScheme::Light) return false;
    }
#ifdef _WIN32
    DWORD val = 1;
    DWORD sz = sizeof(val);
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme",
                     RRF_RT_REG_DWORD,
                     nullptr,
                     &val,
                     &sz) == ERROR_SUCCESS) {
        return (val == 0);
    }
#endif
    return false;
}

bool detectSystemEnglish() {
    const QLocale::Language lang = QLocale::system().language();
    if (lang == QLocale::Russian ||
        lang == QLocale::Ukrainian ||
        lang == QLocale::Belarusian ||
        lang == QLocale::Kazakh) {
        return false;
    }
    return true;
}

} // namespace

AppSettings& AppSettings::instance() {
    static AppSettings inst;
    return inst;
}

AppSettings::AppSettings(QObject* parent) : QObject(parent) {
    load();
    if (QStyleHints* hints = QGuiApplication::styleHints()) {
        connect(hints, &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
            if (themePref_ == ThemePreference::System) {
                if (auto* app = qobject_cast<QApplication*>(QCoreApplication::instance())) {
                    applyApplicationPalette(*app, effectiveDarkTheme());
                }
                emit themeChanged(effectiveDarkTheme());
            }
        });
    }
}

void AppSettings::load() {
    QSettings s(QStringLiteral("Lumina"), QStringLiteral("LuminaPhoto"));
    themePref_ = static_cast<ThemePreference>(s.value(QStringLiteral("ui/theme"), 0).toInt());
    langPref_ = static_cast<LanguagePreference>(s.value(QStringLiteral("ui/language"), 0).toInt());
}

void AppSettings::setThemePreference(ThemePreference pref) {
    if (themePref_ == pref) return;
    themePref_ = pref;
    QSettings s(QStringLiteral("Lumina"), QStringLiteral("LuminaPhoto"));
    s.setValue(QStringLiteral("ui/theme"), static_cast<int>(pref));
    const bool isDark = effectiveDarkTheme();
    if (auto* app = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        applyApplicationPalette(*app, isDark);
    }
    emit themeChanged(isDark);
}

void AppSettings::setLanguagePreference(LanguagePreference pref) {
    if (langPref_ == pref) return;
    langPref_ = pref;
    QSettings s(QStringLiteral("Lumina"), QStringLiteral("LuminaPhoto"));
    s.setValue(QStringLiteral("ui/language"), static_cast<int>(pref));
    emit languageChanged(effectiveEnglish());
}

bool AppSettings::effectiveDarkTheme() const {
    if (themePref_ == ThemePreference::Dark) return true;
    if (themePref_ == ThemePreference::Light) return false;
    return detectSystemDarkMode();
}

bool AppSettings::effectiveEnglish() const {
    if (langPref_ == LanguagePreference::English) return true;
    if (langPref_ == LanguagePreference::Russian) return false;
    return detectSystemEnglish();
}

void AppSettings::applyApplicationPalette(QApplication& app, bool darkTheme) {
    if (!app.style() || app.style()->objectName().compare(QStringLiteral("fusion"), Qt::CaseInsensitive) != 0) {
        app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    }

    QFont uiFont(QStringLiteral("Segoe UI"), 9);
    app.setFont(uiFont);

    QPalette pal;
    if (darkTheme) {
        pal.setColor(QPalette::Window, QColor(0x11, 0x12, 0x16));
        pal.setColor(QPalette::WindowText, QColor(0xF1, 0xF5, 0xF9));
        pal.setColor(QPalette::Base, QColor(0x18, 0x19, 0x20));
        pal.setColor(QPalette::AlternateBase, QColor(0x1E, 0x20, 0x28));
        pal.setColor(QPalette::ToolTipBase, QColor(0x27, 0x27, 0x2A));
        pal.setColor(QPalette::ToolTipText, QColor(0xFF, 0xFF, 0xFF));
        pal.setColor(QPalette::Text, QColor(0xF1, 0xF5, 0xF9));
        pal.setColor(QPalette::PlaceholderText, QColor(0x94, 0xA3, 0xB8));
        pal.setColor(QPalette::Button, QColor(0x22, 0x24, 0x2E));
        pal.setColor(QPalette::ButtonText, QColor(0xF1, 0xF5, 0xF9));
        pal.setColor(QPalette::BrightText, QColor(0xFF, 0xFF, 0xFF));
        pal.setColor(QPalette::Highlight, QColor(0xA8, 0x55, 0xF7));
        pal.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF));
        pal.setColor(QPalette::Mid, QColor(0x2E, 0x31, 0x3D));
        pal.setColor(QPalette::Dark, QColor(0x1A, 0x1C, 0x24));
    } else {
        pal.setColor(QPalette::Window, QColor(0xF4, 0xF5, 0xF7));
        pal.setColor(QPalette::WindowText, QColor(0x11, 0x18, 0x27));
        pal.setColor(QPalette::Base, QColor(0xFF, 0xFF, 0xFF));
        pal.setColor(QPalette::AlternateBase, QColor(0xF9, 0xFA, 0xFB));
        pal.setColor(QPalette::ToolTipBase, QColor(0x18, 0x18, 0x1B));
        pal.setColor(QPalette::ToolTipText, QColor(0xFF, 0xFF, 0xFF));
        pal.setColor(QPalette::Text, QColor(0x11, 0x18, 0x27));
        pal.setColor(QPalette::PlaceholderText, QColor(0x71, 0x71, 0x7A));
        pal.setColor(QPalette::Button, QColor(0xFF, 0xFF, 0xFF));
        pal.setColor(QPalette::ButtonText, QColor(0x11, 0x18, 0x27));
        pal.setColor(QPalette::BrightText, QColor(0xFF, 0xFF, 0xFF));
        pal.setColor(QPalette::Highlight, QColor(0xA8, 0x55, 0xF7));
        pal.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF));
        pal.setColor(QPalette::Mid, QColor(0xE4, 0xE4, 0xE7));
        pal.setColor(QPalette::Dark, QColor(0xD4, 0xD4, 0xD8));
    }
    app.setPalette(pal);
}

} // namespace lumina
