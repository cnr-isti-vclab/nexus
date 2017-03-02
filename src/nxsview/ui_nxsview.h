/********************************************************************************
** Form generated from reading UI file 'nxsview.ui'
**
** Created by: Qt User Interface Compiler version 5.5.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_NXSVIEW_H
#define UI_NXSVIEW_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QWidget>
#include "gl_nxsview.h"

QT_BEGIN_NAMESPACE

class Ui_Form
{
public:
    QGridLayout *gridLayout_2;
    GLNxsview *area;
    QGroupBox *groupBox_2;
    QFormLayout *formLayout;
    QCheckBox *triangles;
    QCheckBox *normals;
    QCheckBox *colors;
    QCheckBox *patches;
    QPushButton *light;
    QPushButton *snapshot;
    QLabel *label_5;
    QSpinBox *fov;
    QCheckBox *info;
    QGroupBox *groupBox;
    QGridLayout *gridLayout;
    QPushButton *top;
    QPushButton *bottom;
    QPushButton *color;
    QGroupBox *groupBox_3;
    QGridLayout *gridLayout1;
    QCheckBox *extract;
    QLabel *label_7;
    QDoubleSpinBox *mtri;
    QLabel *label_4;
    QDoubleSpinBox *error;
    QLabel *label_2;
    QDoubleSpinBox *gpu;
    QLabel *label_6;
    QDoubleSpinBox *ram;
    QSpacerItem *spacerItem;

    void setupUi(QWidget *Form)
    {
        if (Form->objectName().isEmpty())
            Form->setObjectName(QStringLiteral("Form"));
        Form->resize(969, 702);
        gridLayout_2 = new QGridLayout(Form);
        gridLayout_2->setObjectName(QStringLiteral("gridLayout_2"));
        area = new GLNxsview(Form);
        area->setObjectName(QStringLiteral("area"));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(area->sizePolicy().hasHeightForWidth());
        area->setSizePolicy(sizePolicy);
        area->setMinimumSize(QSize(800, 600));

        gridLayout_2->addWidget(area, 0, 0, 4, 1);

        groupBox_2 = new QGroupBox(Form);
        groupBox_2->setObjectName(QStringLiteral("groupBox_2"));
        formLayout = new QFormLayout(groupBox_2);
        formLayout->setObjectName(QStringLiteral("formLayout"));
        triangles = new QCheckBox(groupBox_2);
        triangles->setObjectName(QStringLiteral("triangles"));
        triangles->setChecked(true);

        formLayout->setWidget(0, QFormLayout::SpanningRole, triangles);

        normals = new QCheckBox(groupBox_2);
        normals->setObjectName(QStringLiteral("normals"));
        normals->setChecked(true);

        formLayout->setWidget(1, QFormLayout::SpanningRole, normals);

        colors = new QCheckBox(groupBox_2);
        colors->setObjectName(QStringLiteral("colors"));
        colors->setChecked(true);

        formLayout->setWidget(2, QFormLayout::SpanningRole, colors);

        patches = new QCheckBox(groupBox_2);
        patches->setObjectName(QStringLiteral("patches"));
        patches->setEnabled(true);
        patches->setChecked(false);

        formLayout->setWidget(3, QFormLayout::SpanningRole, patches);

        light = new QPushButton(groupBox_2);
        light->setObjectName(QStringLiteral("light"));
        light->setCheckable(true);

        formLayout->setWidget(4, QFormLayout::SpanningRole, light);

        snapshot = new QPushButton(groupBox_2);
        snapshot->setObjectName(QStringLiteral("snapshot"));

        formLayout->setWidget(5, QFormLayout::SpanningRole, snapshot);

        label_5 = new QLabel(groupBox_2);
        label_5->setObjectName(QStringLiteral("label_5"));

        formLayout->setWidget(6, QFormLayout::LabelRole, label_5);

        fov = new QSpinBox(groupBox_2);
        fov->setObjectName(QStringLiteral("fov"));
        fov->setMinimum(5);
        fov->setMaximum(120);
        fov->setSingleStep(10);
        fov->setValue(30);

        formLayout->setWidget(6, QFormLayout::FieldRole, fov);

        info = new QCheckBox(groupBox_2);
        info->setObjectName(QStringLiteral("info"));

        formLayout->setWidget(7, QFormLayout::LabelRole, info);


        gridLayout_2->addWidget(groupBox_2, 0, 1, 1, 2);

        groupBox = new QGroupBox(Form);
        groupBox->setObjectName(QStringLiteral("groupBox"));
        gridLayout = new QGridLayout(groupBox);
        gridLayout->setObjectName(QStringLiteral("gridLayout"));
        top = new QPushButton(groupBox);
        top->setObjectName(QStringLiteral("top"));

        gridLayout->addWidget(top, 0, 0, 1, 1);

        bottom = new QPushButton(groupBox);
        bottom->setObjectName(QStringLiteral("bottom"));

        gridLayout->addWidget(bottom, 1, 0, 1, 1);

        color = new QPushButton(groupBox);
        color->setObjectName(QStringLiteral("color"));

        gridLayout->addWidget(color, 2, 0, 1, 1);


        gridLayout_2->addWidget(groupBox, 1, 1, 1, 2);

        groupBox_3 = new QGroupBox(Form);
        groupBox_3->setObjectName(QStringLiteral("groupBox_3"));
        gridLayout1 = new QGridLayout(groupBox_3);
        gridLayout1->setObjectName(QStringLiteral("gridLayout1"));
        extract = new QCheckBox(groupBox_3);
        extract->setObjectName(QStringLiteral("extract"));
        extract->setChecked(true);

        gridLayout1->addWidget(extract, 0, 0, 1, 2);

        label_7 = new QLabel(groupBox_3);
        label_7->setObjectName(QStringLiteral("label_7"));

        gridLayout1->addWidget(label_7, 1, 0, 1, 1);

        mtri = new QDoubleSpinBox(groupBox_3);
        mtri->setObjectName(QStringLiteral("mtri"));
        mtri->setDecimals(1);
        mtri->setMaximum(100);

        gridLayout1->addWidget(mtri, 1, 1, 1, 1);

        label_4 = new QLabel(groupBox_3);
        label_4->setObjectName(QStringLiteral("label_4"));

        gridLayout1->addWidget(label_4, 2, 0, 1, 1);

        error = new QDoubleSpinBox(groupBox_3);
        error->setObjectName(QStringLiteral("error"));
        error->setDecimals(1);
        error->setMaximum(100);

        gridLayout1->addWidget(error, 2, 1, 1, 1);

        label_2 = new QLabel(groupBox_3);
        label_2->setObjectName(QStringLiteral("label_2"));

        gridLayout1->addWidget(label_2, 3, 0, 1, 1);

        gpu = new QDoubleSpinBox(groupBox_3);
        gpu->setObjectName(QStringLiteral("gpu"));
        gpu->setDecimals(0);
        gpu->setMaximum(2000);

        gridLayout1->addWidget(gpu, 3, 1, 1, 1);

        label_6 = new QLabel(groupBox_3);
        label_6->setObjectName(QStringLiteral("label_6"));

        gridLayout1->addWidget(label_6, 4, 0, 1, 1);

        ram = new QDoubleSpinBox(groupBox_3);
        ram->setObjectName(QStringLiteral("ram"));
        ram->setDecimals(1);
        ram->setMaximum(24000);

        gridLayout1->addWidget(ram, 4, 1, 1, 1);


        gridLayout_2->addWidget(groupBox_3, 2, 1, 1, 2);

        spacerItem = new QSpacerItem(94, 16, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout_2->addItem(spacerItem, 3, 2, 1, 1);


        retranslateUi(Form);

        QMetaObject::connectSlotsByName(Form);
    } // setupUi

    void retranslateUi(QWidget *Form)
    {
        Form->setWindowTitle(QApplication::translate("Form", "NxsView - ISTI-CNR", 0));
        Form->setStyleSheet(QString());
        groupBox_2->setTitle(QApplication::translate("Form", "View:", 0));
        triangles->setText(QApplication::translate("Form", "Triangles", 0));
        normals->setText(QApplication::translate("Form", "Normals", 0));
        colors->setText(QApplication::translate("Form", "Colors", 0));
        patches->setText(QApplication::translate("Form", "Patches", 0));
        light->setText(QApplication::translate("Form", "Light", 0));
        snapshot->setText(QApplication::translate("Form", "Snapshot", 0));
        label_5->setText(QApplication::translate("Form", "Fov", 0));
        info->setText(QApplication::translate("Form", "Info", 0));
        groupBox->setTitle(QApplication::translate("Form", "Colors:", 0));
        top->setText(QApplication::translate("Form", "top", 0));
        bottom->setText(QApplication::translate("Form", "bottom", 0));
        color->setText(QApplication::translate("Form", "mesh", 0));
        groupBox_3->setTitle(QApplication::translate("Form", "Parameters:", 0));
        extract->setText(QApplication::translate("Form", "Extract", 0));
        label_7->setText(QApplication::translate("Form", "MTri", 0));
        label_4->setText(QApplication::translate("Form", "Error", 0));
        label_2->setText(QApplication::translate("Form", "Gpu", 0));
        label_6->setText(QApplication::translate("Form", "Ram", 0));
    } // retranslateUi

};

namespace Ui {
    class Form: public Ui_Form {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_NXSVIEW_H
