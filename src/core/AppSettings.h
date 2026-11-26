#pragma once

#include <QApplication>
#include <QColor>
#include <QObject>
#include <QString>

namespace lumina {

enum class ThemePreference {
    System = 0,
    Light = 1,
    Dark = 2
};

enum class LanguagePreference {
    System = 0,
    Russian = 1,
    English = 2
};

inline constexpr const char* kBrandHex = "#A855F7";
inline constexpr const char* kBrandHoverHex = "#9333EA";
inline constexpr const char* kBrandPressedHex = "#7E22CE";

class AppSettings : public QObject {
    Q_OBJECT
public:
    static AppSettings& instance();

    ThemePreference themePreference() const { return themePref_; }
    void setThemePreference(ThemePreference pref);

    LanguagePreference languagePreference() const { return langPref_; }
    void setLanguagePreference(LanguagePreference pref);

    bool effectiveDarkTheme() const;
    bool effectiveEnglish() const;

    QString tr(const QString& ru, const QString& en) const {
        return effectiveEnglish() ? en : ru;
    }

    static void applyApplicationPalette(QApplication& app, bool darkTheme);

signals:
    void themeChanged(bool isDark);
    void languageChanged(bool isEnglish);

private:
    explicit AppSettings(QObject* parent = nullptr);
    void load();

    ThemePreference themePref_ = ThemePreference::System;
    LanguagePreference langPref_ = LanguagePreference::System;
};

inline QString trUi(const QString& ru, const QString& en) {
    return AppSettings::instance().tr(ru, en);
}

} // namespace lumina
