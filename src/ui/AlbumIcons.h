#pragma once

#include "../core/AppSettings.h"
#include "SmoothUi.h"

#include <QColor>
#include <QDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace lumina {

struct AlbumIconSpec {
    QString key;
    QString labelRu;
    QString labelEn;
};

inline const QList<AlbumIconSpec>& albumIconPack() {
    static const QList<AlbumIconSpec> kPack = {
        {QStringLiteral("heart"),    QStringLiteral("Любимое"),     QStringLiteral("Heart")},
        {QStringLiteral("camera"),   QStringLiteral("Фотографии"),  QStringLiteral("Camera")},
        {QStringLiteral("mountain"), QStringLiteral("Природа"),     QStringLiteral("Landscape")},
        {QStringLiteral("person"),   QStringLiteral("Люди"),        QStringLiteral("People")},
        {QStringLiteral("home"),     QStringLiteral("Семья / Дом"), QStringLiteral("Home")},
        {QStringLiteral("plane"),    QStringLiteral("Поездки"),     QStringLiteral("Travel")},
        {QStringLiteral("flame"),    QStringLiteral("Яркое"),       QStringLiteral("Featured")},
        {QStringLiteral("music"),    QStringLiteral("События"),     QStringLiteral("Music")},
        {QStringLiteral("palette"),  QStringLiteral("Арт"),         QStringLiteral("Art")},
        {QStringLiteral("gamepad"),  QStringLiteral("Игры"),        QStringLiteral("Gaming")},
        {QStringLiteral("crown"),    QStringLiteral("Лучшее"),      QStringLiteral("Best")},
        {QStringLiteral("bolt"),     QStringLiteral("Моменты"),     QStringLiteral("Moments")},
    };
    return kPack;
}

inline void drawAlbumPresetIcon(QPainter& p,
                                 const QRectF& box,
                                 const QString& key,
                                 const QColor& strokeCol = QColor(0x7E, 0x22, 0xCE),
                                 const QColor& fillCol = QColor(0xC0, 0x84, 0xFC),
                                 bool darkHalo = false) {
    if (key.isEmpty()) return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    const double x = box.x();
    const double y = box.y();
    const double w = box.width();
    const double h = box.height();
    const double penW = std::clamp(w * 0.085, 1.0, 1.7);

    auto pt = [&](double nx, double ny) {
        return QPointF(x + nx * w, y + ny * h);
    };

    QPainterPath mainPath;
    QPainterPath innerCut;

    if (key == QLatin1String("heart")) {
        mainPath.moveTo(pt(0.50, 0.86));
        mainPath.cubicTo(pt(0.14, 0.62), pt(0.08, 0.30), pt(0.28, 0.18));
        mainPath.cubicTo(pt(0.40, 0.11), pt(0.49, 0.20), pt(0.50, 0.28));
        mainPath.cubicTo(pt(0.51, 0.20), pt(0.60, 0.11), pt(0.72, 0.18));
        mainPath.cubicTo(pt(0.92, 0.30), pt(0.86, 0.62), pt(0.50, 0.86));
        mainPath.closeSubpath();
    } else if (key == QLatin1String("camera")) {
        mainPath.moveTo(pt(0.14, 0.30));
        mainPath.lineTo(pt(0.30, 0.30));
        mainPath.lineTo(pt(0.38, 0.18));
        mainPath.lineTo(pt(0.62, 0.18));
        mainPath.lineTo(pt(0.70, 0.30));
        mainPath.lineTo(pt(0.86, 0.30));
        mainPath.quadTo(pt(0.92, 0.30), pt(0.92, 0.38));
        mainPath.lineTo(pt(0.92, 0.76));
        mainPath.quadTo(pt(0.92, 0.84), pt(0.84, 0.84));
        mainPath.lineTo(pt(0.16, 0.84));
        mainPath.quadTo(pt(0.08, 0.84), pt(0.08, 0.76));
        mainPath.lineTo(pt(0.08, 0.38));
        mainPath.quadTo(pt(0.08, 0.30), pt(0.14, 0.30));
        mainPath.closeSubpath();
        innerCut.addEllipse(pt(0.50, 0.56), w * 0.16, h * 0.16);
    } else if (key == QLatin1String("mountain")) {
        QPolygonF m;
        m << pt(0.08, 0.82) << pt(0.38, 0.28) << pt(0.56, 0.58)
          << pt(0.70, 0.40) << pt(0.92, 0.82);
        mainPath.addPolygon(m);
        mainPath.closeSubpath();
        innerCut.addEllipse(pt(0.75, 0.24), w * 0.11, h * 0.11);
    } else if (key == QLatin1String("person")) {
        mainPath.addEllipse(pt(0.50, 0.30), w * 0.18, h * 0.18);
        QPainterPath shoulders;
        shoulders.moveTo(pt(0.16, 0.86));
        shoulders.cubicTo(pt(0.16, 0.56), pt(0.84, 0.56), pt(0.84, 0.86));
        shoulders.closeSubpath();
        mainPath.addPath(shoulders);
    } else if (key == QLatin1String("home")) {
        QPolygonF house;
        house << pt(0.50, 0.14) << pt(0.90, 0.48) << pt(0.78, 0.48)
              << pt(0.78, 0.86) << pt(0.22, 0.86) << pt(0.22, 0.48)
              << pt(0.10, 0.48);
        mainPath.addPolygon(house);
        mainPath.closeSubpath();
        innerCut.addRoundedRect(QRectF(x + w * 0.42, y + h * 0.56, w * 0.16, h * 0.28), 1.2, 1.2);
    } else if (key == QLatin1String("plane")) {
        QPolygonF pl;
        pl << pt(0.88, 0.14) << pt(0.62, 0.38) << pt(0.86, 0.72) << pt(0.74, 0.78)
           << pt(0.46, 0.54) << pt(0.30, 0.70) << pt(0.32, 0.86) << pt(0.22, 0.90)
           << pt(0.18, 0.74) << pt(0.10, 0.70) << pt(0.14, 0.60) << pt(0.30, 0.62)
           << pt(0.46, 0.46) << pt(0.22, 0.26) << pt(0.28, 0.14) << pt(0.62, 0.34);
        mainPath.addPolygon(pl);
        mainPath.closeSubpath();
    } else if (key == QLatin1String("flame")) {
        mainPath.moveTo(pt(0.52, 0.12));
        mainPath.cubicTo(pt(0.76, 0.32), pt(0.84, 0.54), pt(0.74, 0.74));
        mainPath.cubicTo(pt(0.66, 0.86), pt(0.52, 0.89), pt(0.46, 0.88));
        mainPath.cubicTo(pt(0.24, 0.84), pt(0.18, 0.62), pt(0.28, 0.44));
        mainPath.cubicTo(pt(0.33, 0.36), pt(0.38, 0.42), pt(0.40, 0.46));
        mainPath.cubicTo(pt(0.38, 0.30), pt(0.46, 0.18), pt(0.52, 0.12));
        mainPath.closeSubpath();
    } else if (key == QLatin1String("music")) {
        mainPath.addEllipse(pt(0.26, 0.74), w * 0.14, h * 0.12);
        mainPath.addEllipse(pt(0.72, 0.66), w * 0.14, h * 0.12);
        QPolygonF stems;
        stems << pt(0.34, 0.72) << pt(0.34, 0.22) << pt(0.86, 0.14)
              << pt(0.86, 0.64) << pt(0.78, 0.64) << pt(0.78, 0.27)
              << pt(0.42, 0.33) << pt(0.42, 0.72);
        mainPath.addPolygon(stems);
    } else if (key == QLatin1String("palette")) {
        mainPath.moveTo(pt(0.50, 0.14));
        mainPath.cubicTo(pt(0.82, 0.14), pt(0.92, 0.38), pt(0.84, 0.56));
        mainPath.cubicTo(pt(0.78, 0.66), pt(0.64, 0.62), pt(0.64, 0.74));
        mainPath.cubicTo(pt(0.64, 0.86), pt(0.50, 0.88), pt(0.38, 0.84));
        mainPath.cubicTo(pt(0.16, 0.78), pt(0.08, 0.56), pt(0.14, 0.36));
        mainPath.cubicTo(pt(0.20, 0.20), pt(0.34, 0.14), pt(0.50, 0.14));
        mainPath.closeSubpath();
        innerCut.addEllipse(pt(0.34, 0.38), w * 0.075, h * 0.075);
        innerCut.addEllipse(pt(0.54, 0.30), w * 0.075, h * 0.075);
        innerCut.addEllipse(pt(0.68, 0.44), w * 0.075, h * 0.075);
    } else if (key == QLatin1String("gamepad")) {
        mainPath.moveTo(pt(0.26, 0.28));
        mainPath.lineTo(pt(0.74, 0.28));
        mainPath.cubicTo(pt(0.88, 0.28), pt(0.94, 0.54), pt(0.88, 0.76));
        mainPath.cubicTo(pt(0.84, 0.86), pt(0.72, 0.80), pt(0.64, 0.68));
        mainPath.lineTo(pt(0.36, 0.68));
        mainPath.cubicTo(pt(0.28, 0.80), pt(0.16, 0.86), pt(0.12, 0.76));
        mainPath.cubicTo(pt(0.06, 0.54), pt(0.12, 0.28), pt(0.26, 0.28));
        mainPath.closeSubpath();
        innerCut.addEllipse(pt(0.32, 0.48), w * 0.08, h * 0.08);
        innerCut.addEllipse(pt(0.68, 0.48), w * 0.08, h * 0.08);
    } else if (key == QLatin1String("crown")) {
        QPolygonF cr;
        cr << pt(0.14, 0.78) << pt(0.10, 0.30) << pt(0.34, 0.50)
           << pt(0.50, 0.20) << pt(0.66, 0.50) << pt(0.90, 0.30)
           << pt(0.86, 0.78);
        mainPath.addPolygon(cr);
        mainPath.closeSubpath();
    } else if (key == QLatin1String("bolt")) {
        QPolygonF bt;
        bt << pt(0.56, 0.10) << pt(0.22, 0.54) << pt(0.48, 0.54)
           << pt(0.40, 0.90) << pt(0.78, 0.44) << pt(0.52, 0.44);
        mainPath.addPolygon(bt);
        mainPath.closeSubpath();
    }

    if (darkHalo) {
        p.setPen(QPen(QColor(15, 23, 42, 210), penW + 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(mainPath);
    }

    p.setPen(QPen(strokeCol, penW, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(fillCol);
    p.drawPath(mainPath);

    if (!innerCut.isEmpty()) {
        if (key == QLatin1String("mountain")) {
            p.drawPath(innerCut);
        } else {
            p.setPen(QPen(strokeCol, std::max(0.9, penW * 0.8), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(QColor(0xFA, 0xF5, 0xFF));
            p.drawPath(innerCut);
        }
    }

    p.restore();
}

inline QIcon makeAlbumPresetIcon(const QString& key, int size = 20, bool darkHalo = false) {
    QPixmap pm(size * 2, size * 2);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    drawAlbumPresetIcon(p, QRectF(1.5, 1.5, size - 3.0, size - 3.0), key,
                        QColor(0x7E, 0x22, 0xCE), QColor(0xC0, 0x84, 0xFC), darkHalo);
    return QIcon(pm);
}

class AlbumPropertiesDialog : public QDialog {
public:
    AlbumPropertiesDialog(bool createMode,
                          const QString& initialName,
                          const QString& initialIconKey,
                          QWidget* parent = nullptr)
        : QDialog(parent), createMode_(createMode), selectedKey_(initialIconKey) {
        setWindowIcon(QIcon(QStringLiteral(":/icons/lumina.png")));
        setModal(true);
        setMinimumWidth(400);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(20, 18, 20, 18);
        root->setSpacing(12);

        titleLbl_ = new QLabel(this);
        root->addWidget(titleLbl_);

        if (createMode_) {
            nameEdit_ = new QLineEdit(initialName, this);
            nameEdit_->setFixedHeight(36);
            root->addWidget(nameEdit_);
        }

        iconHintLbl_ = new QLabel(this);
        root->addWidget(iconHintLbl_);

        auto* gridWrap = new QWidget(this);
        gridWrap->setObjectName(QStringLiteral("iconGridWrap"));
        auto* grid = new QGridLayout(gridWrap);
        grid->setContentsMargins(10, 10, 10, 10);
        grid->setSpacing(8);

        const auto& pack = albumIconPack();
        for (int i = 0; i < pack.size(); ++i) {
            const AlbumIconSpec& spec = pack[i];
            auto* btn = new QPushButton(gridWrap);
            btn->setObjectName(QStringLiteral("albumIconTile"));
            btn->setCheckable(true);
            btn->setFixedSize(56, 48);
            btn->setIcon(makeAlbumPresetIcon(spec.key, 24));
            btn->setIconSize(QSize(24, 24));
            btn->setToolTip(trUi(spec.labelRu, spec.labelEn));
            btn->setProperty("iconKey", spec.key);
            btn->setChecked(selectedKey_ == spec.key);
            grid->addWidget(btn, i / 4, i % 4);
            iconButtons_.append(btn);

            connect(btn, &QPushButton::clicked, this, [this, spec]() {
                selectKey(spec.key);
            });
        }
        root->addWidget(gridWrap);

        auto* btnRow = new QHBoxLayout();
        btnRow->setSpacing(8);
        clearIconBtn_ = new QPushButton(this);
        clearIconBtn_->setFixedHeight(34);
        okBtn_ = new QPushButton(this);
        okBtn_->setObjectName(QStringLiteral("primaryBtn"));
        okBtn_->setFixedHeight(34);
        cancelBtn_ = new QPushButton(this);
        cancelBtn_->setFixedHeight(34);

        btnRow->addWidget(clearIconBtn_);
        btnRow->addStretch();
        btnRow->addWidget(cancelBtn_);
        btnRow->addWidget(okBtn_);
        root->addLayout(btnRow);

        btnFilter_ = installSmoothButtons(this, AppSettings::instance().effectiveDarkTheme());

        connect(clearIconBtn_, &QPushButton::clicked, this, [this]() {
            selectKey(QString());
            if (!createMode_) accept();
        });
        connect(okBtn_, &QPushButton::clicked, this, [this]() {
            if (createMode_ && nameEdit_ && nameEdit_->text().trimmed().isEmpty()) {
                nameEdit_->setFocus();
                return;
            }
            accept();
        });
        connect(cancelBtn_, &QPushButton::clicked, this, &QDialog::reject);
        if (nameEdit_) {
            connect(nameEdit_, &QLineEdit::returnPressed, okBtn_, &QPushButton::click);
            nameEdit_->setFocus();
        }

        retranslate();
        applyStyle();
    }

    QString albumName() const { return nameEdit_ ? nameEdit_->text().trimmed() : QString(); }
    QString selectedIconKey() const { return selectedKey_; }

private:
    void selectKey(const QString& key) {
        selectedKey_ = key;
        for (QPushButton* b : iconButtons_) {
            b->setChecked(b->property("iconKey").toString() == selectedKey_);
        }
    }

    void retranslate() {
        if (createMode_) {
            setWindowTitle(trUi(QStringLiteral("Новый альбом"), QStringLiteral("New Album")));
            titleLbl_->setText(trUi(QStringLiteral("Название альбома:"), QStringLiteral("Album name:")));
            if (nameEdit_) {
                nameEdit_->setPlaceholderText(trUi(QStringLiteral("Например: Отпуск, Скриншоты, Семья..."),
                                                   QStringLiteral("e.g. Vacation, Screenshots, Family...")));
            }
            iconHintLbl_->setText(trUi(QStringLiteral("Иконка на миниатюру (необязательно):"),
                                       QStringLiteral("Thumbnail icon (optional):")));
            okBtn_->setText(trUi(QStringLiteral("Создать"), QStringLiteral("Create")));
        } else {
            setWindowTitle(trUi(QStringLiteral("Иконка альбома"), QStringLiteral("Album Icon")));
            titleLbl_->setText(trUi(QStringLiteral("Выберите иконку для миниатюры альбома"),
                                    QStringLiteral("Choose an icon for the album thumbnail")));
            iconHintLbl_->setText(trUi(QStringLiteral("Отображается на миниатюре альбома в левой панели:"),
                                       QStringLiteral("Shown on the album thumbnail in the left sidebar:")));
            okBtn_->setText(trUi(QStringLiteral("Применить"), QStringLiteral("Apply")));
        }
        clearIconBtn_->setText(trUi(QStringLiteral("Без иконки"), QStringLiteral("No Icon")));
        cancelBtn_->setText(trUi(QStringLiteral("Отмена"), QStringLiteral("Cancel")));
    }

    void applyStyle() {
        const bool dark = AppSettings::instance().effectiveDarkTheme();
        if (btnFilter_) btnFilter_->setDarkTheme(dark);

        if (dark) {
            setStyleSheet(QStringLiteral(
                "QDialog { background-color: #181920; color: #F1F5F9; }"
                "QWidget#iconGridWrap { background-color: #1E212B; border: 1px solid #2E3342; border-radius: 10px; }"
                "QLabel { color: #E2E8F0; font-size: 12px; font-weight: 600; background: transparent; }"
                "QLineEdit { background-color: #14161D; border: 1px solid #383D4E; border-radius: 8px; padding: 0 12px; color: #F8FAFC; font-size: 13px; }"
                "QLineEdit:focus { border: 1.5px solid #A855F7; }"
                "QPushButton#albumIconTile { background-color: #252936; border: 1px solid #353B4E; border-radius: 10px; }"
                "QPushButton#albumIconTile:hover { background-color: #2E2640; border-color: #A855F7; }"
                "QPushButton#albumIconTile:checked { background-color: #3B1D63; border: 2px solid #A855F7; }"
            ));
        } else {
            setStyleSheet(QStringLiteral(
                "QDialog { background-color: #FFFFFF; color: #0F172A; }"
                "QWidget#iconGridWrap { background-color: #F8FAFC; border: 1px solid #E2E8F0; border-radius: 10px; }"
                "QLabel { color: #1E293B; font-size: 12px; font-weight: 600; background: transparent; }"
                "QLineEdit { background-color: #FFFFFF; border: 1px solid #CBD5E1; border-radius: 8px; padding: 0 12px; color: #0F172A; font-size: 13px; }"
                "QLineEdit:focus { border: 1.5px solid #A855F7; }"
                "QPushButton#albumIconTile { background-color: #FFFFFF; border: 1px solid #E2E8F0; border-radius: 10px; }"
                "QPushButton#albumIconTile:hover { background-color: #FAF5FF; border-color: #A855F7; }"
                "QPushButton#albumIconTile:checked { background-color: #F3E8FF; border: 2px solid #A855F7; }"
            ));
        }
    }

    bool createMode_ = false;
    QString selectedKey_;
    SmoothButtonFilter* btnFilter_ = nullptr;
    QLabel* titleLbl_ = nullptr;
    QLabel* iconHintLbl_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QPushButton* clearIconBtn_ = nullptr;
    QPushButton* okBtn_ = nullptr;
    QPushButton* cancelBtn_ = nullptr;
    QList<QPushButton*> iconButtons_;
};

} // namespace lumina
