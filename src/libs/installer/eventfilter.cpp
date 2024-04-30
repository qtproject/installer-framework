#include "eventfilter.h"

#include <QEvent>
#include <QDebug>

EventFilter::EventFilter(QObject* parrent): QObject(parrent) {}

bool EventFilter::eventFilter(QObject *obj, QEvent *event) {
    qDebug () << obj->metaObject()->className() << event->type();
    return false;
}
