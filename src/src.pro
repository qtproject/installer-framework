TEMPLATE = subdirs
SUBDIRS += libs sdk
sdk.depends = libs

TRANSLATIONS += sdk/translations/de.ts \
    sdk/translations/en.ts \
    sdk/translations/es.ts \
    sdk/translations/fr.ts \
    sdk/translations/ja.ts \
    sdk/translations/pl.ts \
    sdk/translations/ru.ts \
    sdk/translations/zh_cn.ts \
    sdk/translations/it.ts
