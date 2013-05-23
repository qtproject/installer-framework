!isEmpty(NO_APP_BUNDLE_PRI_INCLUDED) {
    error("no_app_bundle.pri already included")
}
NO_APP_BUNDLE_PRI_INCLUDED = 1

equals(TEMPLATE, app):CONFIG -= app_bundle

isEqual(QT_MAJOR_VERSION, 4):static:contains(QT, gui) {
    isEmpty(DESTDIR) {
        MY_DEST_DIR=$$OUT_PWD
    } else {
        MY_DEST_DIR=$$DESTDIR
    }

    !exists($$(MY_DEST_DIR)/qt_menu.nib) {
        # try to get the qt_menu.nib path from the environment variable
        isEmpty(QT_MENU_NIB_DIR): QT_MENU_NIB_DIR = $$(QT_MENU_NIB_DIR)

        # everything which has not the IFW_APP_PATH as target can try to copy it from there
        exists($$IFW_APP_PATH/qt_menu.nib):QT_MENU_NIB_DIR=$$IFW_APP_PATH/qt_menu.nib

        isEmpty(QT_MENU_NIB_DIR) {
            warning(Please call qmake with QT_MENU_NIB_DIR=<YOUR_QT_SRC_DIR>/src/gui/mac/qt_menu.nib)
        } else {
            system($$QMAKE_COPY -r $$quote($$QT_MENU_NIB_DIR) $$quote($$MY_DEST_DIR))
        }
    }
}
