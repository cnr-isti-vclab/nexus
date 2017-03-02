#ifndef QCONTROLLER_H
#define QCONTROLLER_H

#include <QObject>
#include <QTimer>
#include "controller.h"

class QGLWidget;
class QController: public QObject, public Controller {
    Q_OBJECT
public:
    void start(QGLWidget *shared = NULL);

signals:
    ///emitted at most every (1/fps)s if the cache is changed
    void updated();

protected:
    QTimer updater;
};


#endif // QCONTROLLER_H
