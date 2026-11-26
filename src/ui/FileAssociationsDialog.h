#pragma once

#include "../core/AppSettings.h"
#include "../core/WindowsFileAssociation.h"
#include "SmoothUi.h"

#include <QCheckBox>
#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace lumina {

class FileAssociationsDialog : public QDialog {
public:
    explicit FileAssociationsDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowIcon(QIcon(QStringLiteral(":/icons/lumina.png")));
        setModal(true);
        setMinimumWidth(520);
        resize(540, 480);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(20, 18, 20, 18);
        root->setSpacing(12);

        headerLbl_ = new QLabel(this);
        descLbl_ = new QLabel(this);
        descLbl_->setWordWrap(true);

        auto* quickRow = new QHBoxLayout();
        quickRow->setSpacing(8);
        selAllBtn_ = new QPushButton(this);
        selAllBtn_->setFixedHeight(30);
        selCommonBtn_ = new QPushButton(this);
        selCommonBtn_->setFixedHeight(30);
        selNoneBtn_ = new QPushButton(this);
        selNoneBtn_->setFixedHeight(30);
        summaryLbl_ = new QLabel(this);
        quickRow->addWidget(selAllBtn_);
        quickRow->addWidget(selCommonBtn_);
        quickRow->addWidget(selNoneBtn_);
        quickRow->addStretch();
        quickRow->addWidget(summaryLbl_);

        auto* scroll = new QScrollArea(this);
        scroll->setObjectName(QStringLiteral("assocScroll"));
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto* listHost = new QWidget(scroll);
        listHost->setObjectName(QStringLiteral("assocInner"));
        auto* grid = new QGridLayout(listHost);
        grid->setContentsMargins(10, 10, 10, 10);
        grid->setHorizontalSpacing(14);
        grid->setVerticalSpacing(8);

        const QStringList exts = WindowsFileAssociation::supportedImageExtensions();
        for (int i = 0; i < exts.size(); ++i) {
            const QString& ext = exts[i];
            auto* cb = new QCheckBox(ext.toUpper(), listHost);
            cb->setProperty("ext", ext);
            cb->setChecked(true);
            auto* badge = new QLabel(listHost);
            badge->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            auto* cell = new QHBoxLayout();
            cell->setContentsMargins(2, 2, 2, 2);
            cell->setSpacing(6);
            cell->addWidget(cb);
            cell->addStretch();
            cell->addWidget(badge);

            auto* cellWrap = new QWidget(listHost);
            cellWrap->setLayout(cell);
            grid->addWidget(cellWrap, i / 2, i % 2);
            rows_.append({ext, cb, badge});
        }
        scroll->setWidget(listHost);

        statusLbl_ = new QLabel(this);
        statusLbl_->setWordWrap(true);
        statusLbl_->setVisible(false);

        auto* btnRow = new QHBoxLayout();
        btnRow->setSpacing(8);
        regOnlyBtn_ = new QPushButton(this);
        regOnlyBtn_->setFixedHeight(34);
        regAndOpenBtn_ = new QPushButton(this);
        regAndOpenBtn_->setObjectName(QStringLiteral("primaryBtn"));
        regAndOpenBtn_->setFixedHeight(34);
        closeBtn_ = new QPushButton(this);
        closeBtn_->setFixedHeight(34);

        btnRow->addWidget(regOnlyBtn_);
        btnRow->addStretch();
        btnRow->addWidget(regAndOpenBtn_);
        btnRow->addWidget(closeBtn_);

        root->addWidget(headerLbl_);
        root->addWidget(descLbl_);
        root->addLayout(quickRow);
        root->addWidget(scroll, 1);
        root->addWidget(statusLbl_);
        root->addLayout(btnRow);

        btnFilter_ = installSmoothButtons(this, AppSettings::instance().effectiveDarkTheme());

        connect(selAllBtn_, &QPushButton::clicked, this, [this]() {
            for (const ExtRow& r : rows_) r.check->setChecked(true);
        });
        connect(selCommonBtn_, &QPushButton::clicked, this, [this]() {
            static const QStringList common = {
                QStringLiteral(".jpg"), QStringLiteral(".jpeg"), QStringLiteral(".png"),
                QStringLiteral(".webp"), QStringLiteral(".gif"), QStringLiteral(".psd")
            };
            for (const ExtRow& r : rows_) r.check->setChecked(common.contains(r.ext));
        });
        connect(selNoneBtn_, &QPushButton::clicked, this, [this]() {
            for (const ExtRow& r : rows_) r.check->setChecked(false);
        });
        connect(regOnlyBtn_, &QPushButton::clicked, this, [this]() { registerSelected(false); });
        connect(regAndOpenBtn_, &QPushButton::clicked, this, [this]() { registerSelected(true); });
        connect(closeBtn_, &QPushButton::clicked, this, &QDialog::accept);

        retranslate();
        applyStyle();
        refreshStatus();
    }

private:
    struct ExtRow {
        QString ext;
        QCheckBox* check = nullptr;
        QLabel* badge = nullptr;
    };

    QStringList selectedExtensions() const {
        QStringList out;
        for (const ExtRow& r : rows_) {
            if (r.check->isChecked()) out.append(r.ext);
        }
        return out;
    }

    void refreshStatus() {
        int activeCount = 0;
        for (const ExtRow& r : rows_) {
            const bool isDef = WindowsFileAssociation::isExtensionAssociatedWithLumina(r.ext);
            if (isDef) ++activeCount;
            r.badge->setText(isDef
                                 ? trUi(QStringLiteral("По умолчанию ✓"), QStringLiteral("Default ✓"))
                                 : QString());
        }
        summaryLbl_->setText(trUi(QString("По умолчанию: %1 / %2").arg(activeCount).arg(rows_.size()),
                                  QString("Defaults: %1 / %2").arg(activeCount).arg(rows_.size())));
    }

    void registerSelected(bool openSettings) {
        const QStringList chosen = selectedExtensions();
        if (chosen.isEmpty()) {
            QMessageBox::information(this,
                                     trUi(QStringLiteral("Ассоциации Windows"), QStringLiteral("Windows Associations")),
                                     trUi(QStringLiteral("Отметьте хотя бы один формат файлов."),
                                          QStringLiteral("Select at least one file format.")));
            return;
        }

        QString err;
        if (!WindowsFileAssociation::registerFileTypes(chosen, &err)) {
            QMessageBox::warning(this,
                                 trUi(QStringLiteral("Ошибка регистрации"), QStringLiteral("Registration Error")),
                                 err);
            return;
        }

        if (openSettings) {
            WindowsFileAssociation::openWindowsDefaultAppsSettings();
            statusLbl_->setText(trUi(
                QStringLiteral("Выбранные форматы зарегистрированы. В открывшихся Параметрах Windows выберите «Lumina Photo» и нажмите «Назначить по умолчанию»."),
                QStringLiteral("Selected formats registered. In Windows Settings, pick \"Lumina Photo\" and click \"Set default\".")));
        } else {
            statusLbl_->setText(trUi(
                QStringLiteral("Готово! Lumina Photo добавлена в «Открыть с помощью» и список приложений Windows для выбранных форматов."),
                QStringLiteral("Done! Lumina Photo is now available in \"Open with\" and Windows Default Apps for selected formats.")));
        }
        statusLbl_->setVisible(true);
        refreshStatus();
        QTimer::singleShot(1500, this, [this]() { refreshStatus(); });
    }

    void retranslate() {
        setWindowTitle(trUi(QStringLiteral("Ассоциации файлов Windows — Lumina Photo"),
                            QStringLiteral("Windows File Associations — Lumina Photo")));
        headerLbl_->setText(trUi(QStringLiteral("Ассоциации файлов в Windows"),
                                 QStringLiteral("Windows File Associations")));
        descLbl_->setText(trUi(
            QStringLiteral("Отметьте нужные форматы, чтобы добавить Lumina Photo в «Открыть с помощью» и Параметры Windows (Windows 10/11 требует подтвердить «Назначить по умолчанию» в системном окне)."),
            QStringLiteral("Select formats to register Lumina Photo for \"Open with\" and Windows Default Apps (Windows 10/11 requires confirming \"Set default\" inside System Settings).")));
        selAllBtn_->setText(trUi(QStringLiteral("Выбрать все"), QStringLiteral("Select All")));
        selCommonBtn_->setText(trUi(QStringLiteral("Основные"), QStringLiteral("Common")));
        selNoneBtn_->setText(trUi(QStringLiteral("Снять все"), QStringLiteral("Clear")));
        regOnlyBtn_->setText(trUi(QStringLiteral("Только зарегистрировать"), QStringLiteral("Register Only")));
        regAndOpenBtn_->setText(trUi(QStringLiteral("Применить и открыть Параметры Windows"),
                                     QStringLiteral("Apply & Open Windows Settings")));
        closeBtn_->setText(trUi(QStringLiteral("Закрыть"), QStringLiteral("Close")));
    }

    void applyStyle() {
        const bool dark = AppSettings::instance().effectiveDarkTheme();
        if (btnFilter_) btnFilter_->setDarkTheme(dark);

        if (dark) {
            setStyleSheet(QStringLiteral(
                "QDialog { background-color: #181920; color: #F1F5F9; }"
                "QWidget#assocInner { background-color: #1E212B; border-radius: 10px; }"
                "QScrollArea#assocScroll { background-color: #1E212B; border: 1px solid #2E3342; border-radius: 10px; }"
                "QLabel { color: #E2E8F0; background: transparent; }"
                "QCheckBox { color: #F1F5F9; font-size: 12px; font-weight: 600; spacing: 8px; }"
                "QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px; border: 1.5px solid #475569; background: #14161D; }"
                "QCheckBox::indicator:checked { background: #A855F7; border-color: #A855F7; }"
            ) + modernScrollBarQss(true));
            headerLbl_->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: 700; color: #F8FAFC;"));
            descLbl_->setStyleSheet(QStringLiteral("font-size: 12px; color: #94A3B8;"));
            summaryLbl_->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 600; color: #C084FC;"));
            statusLbl_->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 600; color: #C084FC; padding: 4px 0px;"));
            for (const ExtRow& r : rows_) {
                r.badge->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: 600; color: #C084FC;"));
            }
        } else {
            setStyleSheet(QStringLiteral(
                "QDialog { background-color: #FFFFFF; color: #0F172A; }"
                "QWidget#assocInner { background-color: #F8FAFC; border-radius: 10px; }"
                "QScrollArea#assocScroll { background-color: #F8FAFC; border: 1px solid #E2E8F0; border-radius: 10px; }"
                "QLabel { color: #1E293B; background: transparent; }"
                "QCheckBox { color: #0F172A; font-size: 12px; font-weight: 600; spacing: 8px; }"
                "QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px; border: 1.5px solid #CBD5E1; background: #FFFFFF; }"
                "QCheckBox::indicator:checked { background: #A855F7; border-color: #A855F7; }"
            ) + modernScrollBarQss(false));
            headerLbl_->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: 700; color: #0F172A;"));
            descLbl_->setStyleSheet(QStringLiteral("font-size: 12px; color: #64748B;"));
            summaryLbl_->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 600; color: #7E22CE;"));
            statusLbl_->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 600; color: #7E22CE; padding: 4px 0px;"));
            for (const ExtRow& r : rows_) {
                r.badge->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: 600; color: #7E22CE;"));
            }
        }
    }

    SmoothButtonFilter* btnFilter_ = nullptr;
    QLabel* headerLbl_ = nullptr;
    QLabel* descLbl_ = nullptr;
    QLabel* summaryLbl_ = nullptr;
    QLabel* statusLbl_ = nullptr;
    QPushButton* selAllBtn_ = nullptr;
    QPushButton* selCommonBtn_ = nullptr;
    QPushButton* selNoneBtn_ = nullptr;
    QPushButton* regOnlyBtn_ = nullptr;
    QPushButton* regAndOpenBtn_ = nullptr;
    QPushButton* closeBtn_ = nullptr;
    QList<ExtRow> rows_;
};

} // namespace lumina
