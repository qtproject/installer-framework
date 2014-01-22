include(../../qttest.pri)

QT -= gui
QT += network
isEqual(QT_MAJOR_VERSION, 5) {
  QT += concurrent
}
SOURCES += tst_task.cpp
