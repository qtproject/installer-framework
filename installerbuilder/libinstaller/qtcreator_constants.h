#ifndef QTCREATOR_CONSTANTS_H
#define QTCREATOR_CONSTANTS_H

#if defined(Q_OS_MAC)
    static const char QtCreatorSettingsSuffixPath[] =
        "/Qt Creator.app/Contents/Resources/Nokia/QtCreator.ini";
#else
    static const char QtCreatorSettingsSuffixPath[] =
        "/QtCreator/share/qtcreator/Nokia/QtCreator.ini";
#endif

#if defined(Q_OS_MAC)
    static const char ToolChainSettingsSuffixPath[] =
        "/Qt Creator.app/Contents/Resources/Nokia/toolChains.xml";
#else
    static const char ToolChainSettingsSuffixPath[] =
        "/QtCreator/share/qtcreator/Nokia/toolChains.xml";
#endif

#if defined(Q_OS_MAC)
    static const char QtVersionSettingsSuffixPath[] =
        "/Qt Creator.app/Contents/Resources/Nokia/qtversion.xml";
#else
    static const char QtVersionSettingsSuffixPath[] =
        "/QtCreator/share/qtcreator/Nokia/qtversion.xml";
#endif

// Begin - copied from Creator src\plugins\projectexplorer\toolchainmanager.cpp
static const char TOOLCHAIN_DATA_KEY[] = "ToolChain.";
static const char TOOLCHAIN_COUNT_KEY[] = "ToolChain.Count";
static const char TOOLCHAIN_FILE_VERSION_KEY[] = "Version";
static const char DEFAULT_DEBUGGER_COUNT_KEY[] = "DefaultDebugger.Count";
static const char DEFAULT_DEBUGGER_ABI_KEY[] = "DefaultDebugger.Abi.";
static const char DEFAULT_DEBUGGER_PATH_KEY[] = "DefaultDebugger.Path.";

static const char ID_KEY[] = "ProjectExplorer.ToolChain.Id";
static const char DISPLAY_NAME_KEY[] = "ProjectExplorer.ToolChain.DisplayName";
// End - copied from Creator

// Begin - copied from Creator src\plugins\qt4projectmanager\qtversionmanager.cpp
static const char QtVersionsSectionName[] = "QtVersions";
static const char newQtVersionsKey[] = "NewQtVersions";
// End - copied from Creator

#endif // QTCREATOR_CONSTANTS_H
