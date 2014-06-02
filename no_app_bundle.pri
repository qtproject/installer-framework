!isEmpty(NO_APP_BUNDLE_PRI_INCLUDED) {
    error("no_app_bundle.pri already included")
}
NO_APP_BUNDLE_PRI_INCLUDED = 1

equals(TEMPLATE, app):CONFIG -= app_bundle
