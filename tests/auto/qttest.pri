include(../../installerfw.pri)
#include(qttestrpath.pri)

isEmpty(TEMPLATE):TEMPLATE=app
QT += testlib
CONFIG += qt warn_on console depend_includepath testcase

DEFINES -= QT_NO_CAST_FROM_ASCII
# prefix test binary with tst_
!contains(TARGET, ^tst_.*):TARGET = $$join(TARGET,,"tst_")

#win32 {
#    lib = ../../
#    lib ~= s,/,\\,g
#    # the below gets added to later by testcase.prf
#    check.commands = cd . & set PATH=$$lib;%PATH%& cmd /c
#}
macx:include(../../no_app_bundle.pri)
