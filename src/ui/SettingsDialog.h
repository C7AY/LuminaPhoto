#pragma once

#include "../core/AppSettings.h"
#include "SmoothUi.h"

#include <QComboBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace lumina {

class SettingsDialog : public QDialog {
public:
    explicit SettingsDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowIcon(QIcon(QStringLiteral(":/icons/lumina.png")));
        setModal(true);
        setFixedWidth(400);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(20, 18, 20, 18);
        root->setSpacing(14);

        headerLbl_ = new QLabel(this);
        subLbl_ = new QLabel(this);
        subLbl_->setWordWrap(true);

        themeTitleLbl_ = new QLabel(this);
        themeCombo_ = new QComboBox(this);
        themeCombo_->setFixedHeight(34);
        themeCombo_->addItem(QString(), static_cast<int>(ThemePreference::System));
        themeCombo_->addItem(QString(), static_cast<int>(ThemePreference::Light));
        themeCombo_->addItem(QString(), static_cast<int>(ThemePreference::Dark));

        langTitleLbl_ = new QLabel(this);
        langCombo_ = new QComboBox(this);
        langCombo_->setFixedHeight(34);
        langCombo_->addItem(QString(), static_cast<int>(LanguagePreference::System));
        langCombo_->addItem(QStringLiteral("Русский"), static_cast<int>(LanguagePreference::Russian));
        langCombo_->addItem(QStringLiteral("English"), static_cast<int>(LanguagePreference::English));

        auto* btnRow = new QHBoxLayout();
        btnRow->addStretch();
        closeBtn_ = new QPushButton(this);
        closeBtn_->setObjectName(QStringLiteral("primaryBtn"));
        closeBtn_->setFixedHeight(34);
        closeBtn_->setMinimumWidth(110);
        closeBtn_->setFocusPolicy(Qt::NoFocus);
        btnRow->addWidget(closeBtn_);

        root->addWidget(headerLbl_);
        root->addWidget(subLbl_);
        root->addSpacing(2);
        root->addWidget(themeTitleLbl_);
        root->addWidget(themeCombo_);
        root->addWidget(langTitleLbl_);
        root->addWidget(langCombo_);
        root->addSpacing(6);
        root->addLayout(btnRow);

        btnFilter_ = installSmoothButtons(this, AppSettings::instance().effectiveDarkTheme());

        auto syncSelections = [this]() {
            const int tIdx = themeCombo_->findData(static_cast<int>(AppSettings::instance().themePreference()));
            if (tIdx >= 0) themeCombo_->setCurrentIndex(tIdx);
            const int lIdx = langCombo_->findData(static_cast<int>(AppSettings::instance().languagePreference()));
            if (lIdx >= 0) langCombo_->setCurrentIndex(lIdx);
        };
        syncSelections();
        retranslate();
        applyStyle();

        connect(themeCombo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
            if (idx < 0) return;
            const auto pref = static_cast<ThemePreference>(themeCombo_->itemData(idx).toInt());
            AppSettings::instance().setThemePreference(pref);
            applyStyle();
        });
        connect(langCombo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
            if (idx < 0) return;
            const auto pref = static_cast<LanguagePreference>(langCombo_->itemData(idx).toInt());
            AppSettings::instance().setLanguagePreference(pref);
            retranslate();
        });
        connect(closeBtn_, &QPushButton::clicked, this, &QDialog::accept);
    }

private:
    void retranslate() {
        setWindowTitle(trUi(QStringLiteral("Настройки — Lumina Photo"), QStringLiteral("Settings — Lumina Photo")));
        headerLbl_->setText(trUi(QStringLiteral("Настройки приложения"), QStringLiteral("Application Settings")));
        subLbl_->setText(trUi(QStringLiteral("Выберите тему оформления и язык интерфейса. По умолчанию используются параметры Windows."),
                              QStringLiteral("Choose appearance theme and UI language. System defaults follow Windows settings.")));
        themeTitleLbl_->setText(trUi(QStringLiteral("Тема оформления"), QStringLiteral("Appearance theme")));
        langTitleLbl_->setText(trUi(QStringLiteral("Язык интерфейса"), QStringLiteral("Interface language")));

        themeCombo_->setItemText(0, trUi(QStringLiteral("Тема устройства (авто)"), QStringLiteral("Device theme (auto)")));
        themeCombo_->setItemText(1, trUi(QStringLiteral("Светлая"), QStringLiteral("Light")));
        themeCombo_->setItemText(2, trUi(QStringLiteral("Тёмная"), QStringLiteral("Dark")));
        langCombo_->setItemText(0, trUi(QStringLiteral("Язык устройства (авто)"), QStringLiteral("Device language (auto)")));
        closeBtn_->setText(trUi(QStringLiteral("Готово"), QStringLiteral("Done")));
    }

    void applyStyle() {
        const bool dark = AppSettings::instance().effectiveDarkTheme();
        if (btnFilter_) btnFilter_->setDarkTheme(dark);

        if (dark) {
            setStyleSheet(QStringLiteral(
                "QDialog { background-color: #181920; color: #F1F5F9; }"
                "QLabel { color: #E2E8F0; background: transparent; }"
                "QComboBox { background-color: #232530; color: #F8FAFC; border: 1px solid #333747; border-radius: 8px; padding: 6px 10px; font-size: 12px; }"
                "QComboBox:hover { border-color: #A855F7; }"
                "QComboBox QAbstractItemView { background-color: #1E2028; color: #F8FAFC; selection-background-color: #A855F7; selection-color: #FFFFFF; border: 1px solid #333747; }"
            ));
            headerLbl_->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: 700; color: #F8FAFC;"));
            subLbl_->setStyleSheet(QStringLiteral("font-size: 12px; color: #94A3B8;"));
            themeTitleLbl_->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 600; color: #CBD5E1;"));
            langTitleLbl_->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 600; color: #CBD5E1;"));
        } else {
            setStyleSheet(QStringLiteral(
                "QDialog { background-color: #FFFFFF; color: #0F172A; }"
                "QLabel { color: #1E293B; background: transparent; }"
                "QComboBox { background-color: #F1F5F9; color: #0F172A; border: 1px solid #E2E8F0; border-radius: 8px; padding: 6px 10px; font-size: 12px; }"
                "QComboBox:hover { border-color: #A855F7; }"
                "QComboBox QAbstractItemView { background-color: #FFFFFF; color: #0F172A; selection-background-color: #A855F7; selection-color: #FFFFFF; border: 1px solid #E2E8F0; }"
            ));
            headerLbl_->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: 700; color: #0F172A;"));
            subLbl_->setStyleSheet(QStringLiteral("font-size: 12px; color: #64748B;"));
            themeTitleLbl_->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 600; color: #334155;"));
            langTitleLbl_->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 600; color: #334155;"));
        }
        closeBtn_->update();
    }

    SmoothButtonFilter* btnFilter_ = nullptr;
    QLabel* headerLbl_ = nullptr;
    QLabel* subLbl_ = nullptr;
    QLabel* themeTitleLbl_ = nullptr;
    QLabel* langTitleLbl_ = nullptr;
    QComboBox* themeCombo_ = nullptr;
    QComboBox* langCombo_ = nullptr;
    QPushButton* closeBtn_ = nullptr;
};

} // namespace lumina
