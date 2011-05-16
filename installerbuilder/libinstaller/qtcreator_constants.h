#ifndef QTCREATOR_CONSTANTS_H
#define QTCREATOR_CONSTANTS_H

#if defined ( Q_OS_MAC )
    static const char *const QtCreatorSettingsSuffixPath =
        "/Qt Creator.app/Contents/Resources/Nokia/QtCreator.ini";
#else
    static const char *const QtCreatorSettingsSuffixPath =
        "/QtCreator/share/qtcreator/Nokia/QtCreator.ini";
#endif

#if defined ( Q_OS_MAC )
    static const char *const ToolChainSettingsSuffixPath =
        "/Qt Creator.app/Contents/Resources/Nokia/toolChains.xml";
#else
    static const char *const ToolChainSettingsSuffixPath =
        "/QtCreator/share/qtcreator/Nokia/toolChains.xml";
#endif

//Begin - copied from Creator src\plugins\projectexplorer\toolchainmanager.cpp
static const char *const TOOLCHAIN_DATA_KEY = "ToolChain.";
static const char *const TOOLCHAIN_COUNT_KEY = "ToolChain.Count";
static const char *const TOOLCHAIN_FILE_VERSION_KEY = "Version";
static const char *const DEFAULT_DEBUGGER_COUNT_KEY = "DefaultDebugger.Count";
static const char *const DEFAULT_DEBUGGER_ABI_KEY = "DefaultDebugger.Abi.";
static const char *const DEFAULT_DEBUGGER_PATH_KEY = "DefaultDebugger.Path.";

static const char *const ID_KEY = "ProjectExplorer.ToolChain.Id";
static const char *const DISPLAY_NAME_KEY = "ProjectExplorer.ToolChain.DisplayName";
//End - copied from Creator

//Begin - copied from Creator src\plugins\qt4projectmanager\qtversionmanager.cpp
static const char *const QtVersionsSectionName = "QtVersions";
static const char *const newQtVersionsKey = "NewQtVersions";
//End - copied from Creator


#endif //QTCREATOR_CONSTANTS_H
