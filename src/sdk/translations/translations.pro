TEMPLATE = aux

include(../../../installerfw.pri)

!exists($$LUPDATE): return()

IB_TRANSLATIONS = $$files($$PWD/*_??.ts)
IB_TRANSLATIONS -= $$PWD/ifw_en.ts

wd = $$toNativeSeparators($$IFW_SOURCE_TREE)
sources = src
lupdate_opts = -locations relative -no-ui-lines -no-sort -no-obsolete

IB_ALL_TRANSLATIONS = $$IB_TRANSLATIONS $$PWD/ifw_untranslated.ts
for(file, IB_ALL_TRANSLATIONS) {
    lang = $$replace(file, .*_([^/]*)\\.ts, \\1)
    v = ts-$${lang}.commands
    $$v = cd $$wd && $$LUPDATE $$lupdate_opts $$sources -ts $$file
    QMAKE_EXTRA_TARGETS += ts-$$lang
}
ts-all.commands = cd $$wd && $$LUPDATE $$lupdate_opts $$sources -ts $$IB_ALL_TRANSLATIONS
QMAKE_EXTRA_TARGETS += ts-all

lconvert_options = -sort-contexts -locations none -i
isEqual(QMAKE_DIR_SEP, /) {
    commit-ts.commands = \
        cd $$wd; \
        git add -N src/sdk/translations/*_??.ts && \
        for f in `git diff-files --name-only src/sdk/translations/*_??.ts`; do \
            $$LCONVERT $$lconvert_options \$\$f -o \$\$f; \
        done; \
        git add src/sdk/translations/*_??.ts && git commit
} else {
    commit-ts.commands = \
        cd $$wd && \
        git add -N src/sdk/translations/*_??.ts && \
        for /f usebackq %%f in (`git diff-files --name-only src/sdk/translations/*_??.ts`) do \
            $$LCONVERT $$lconvert_options %%f -o %%f $$escape_expand(\\n\\t) \
        cd $$wd && git add src/sdk/translations/*_??.ts && git commit
}
QMAKE_EXTRA_TARGETS += commit-ts
