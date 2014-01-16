include(../../qttest.pri)

QT -= gui
isEqual(QT_MAJOR_VERSION, 5) {
  QT += concurrent network
}
SOURCES += tst_task.cpp
