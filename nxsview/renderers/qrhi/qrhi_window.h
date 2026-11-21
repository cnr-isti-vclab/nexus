#ifndef NX_QRHI_WINDOW_H
#define NX_QRHI_WINDOW_H

#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QPushButton;
class QSpinBox;
class RhiNxsview;

class QrhiWindow : public QWidget {
public:
	explicit QrhiWindow(QWidget *parent = nullptr);

	RhiNxsview *area;
	QCheckBox *triangles;
	QCheckBox *normals;
	QCheckBox *colors;
	QCheckBox *patches;
	QPushButton *light;
	QPushButton *snapshot;
	QCheckBox *info;
	QSpinBox *fov;
	QPushButton *top;
	QPushButton *bottom;
	QPushButton *meshColor;
	QCheckBox *extract;
	QDoubleSpinBox *mtri;
	QDoubleSpinBox *error;
	QDoubleSpinBox *gpu;
	QDoubleSpinBox *ram;
};

#endif
