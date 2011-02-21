function Component()
{
    if( installer.value( "os" ) == "win" )
    {
        component.addDownloadableArchive( "bin.7z" );
        component.addDownloadableArchive( "installer.dll" );
        component.addDownloadableArchive( "KDToolsCore2.dll" );
        component.addDownloadableArchive( "KDUpdater2.dll" );
        component.addDownloadableArchive( "lib.7z" );
        component.addDownloadableArchive( "LICENSE" );
        component.addDownloadableArchive( "Qt Creator.url" );
        component.addDownloadableArchive( "share.7z" );
        component.addDownloadableArchive( "uninst.exe" );
        component.addDownloadableArchive( "Updater.dll" );
        component.addDownloadableArchive( "Updater.pluginspec" );
    }
    else if( installer.value( "os" ) == "mac" )
    {
        component.addDownloadableArchive( "libUpdater.dylib" );
        component.addDownloadableArchive( "Updater.pluginspec" );
        component.addDownloadableArchive( "Qt Creator.app.7z" );
    }
}

Component.prototype.createOperations = function( archive )
{
    if( installer.value( "os" ) == "win" )
    {
        component.createOperationsForArchive( "bin.7z" );
        component.addOperation( "Copy", "installer://com.nokia.qtcreator/installer.dll", "@TargetDir@/bin/installer.dll" );
        component.addOperation( "Copy", "installer://com.nokia.qtcreator/KDToolsCore2.dll", "@TargetDir@/bin/KDToolsCore2.dll" );
        component.addOperation( "Copy", "installer://com.nokia.qtcreator/KDUpdater2.dll", "@TargetDir@/bin/KDUpdater2.dll" );
        component.addOperation( "Copy", "installer://com.nokia.qtcreator/LICENSE", "@TargetDir@/LICENSE" );
        component.addOperation( "Copy", "installer://com.nokia.qtcreator/Qt Creator.url", "@TargetDir@/Qt Creator.url" );
        component.createOperationsForArchive( "lib.7z" );
        component.createOperationsForArchive( "share.7z" );
        component.addOperation( "Copy", "installer://com.nokia.qtcreator/uninst.exe", "@TargetDir@/uninst.exe" );
        component.addOperation( "Copy", "installer://com.nokia.qtcreator/Updater.dll", "@TargetDir@/lib/qtcreator/plugins/Nokia/Updater.dll" );
        component.addOperation( "Copy", "installer://com.nokia.qtcreator/Updater.pluginspec", "@TargetDir@/lib/qtcreator/plugins/Nokia/Updater.pluginspec" );
    }
    else if( installer.value( "os" ) == "mac" )
    {
        component.createOperationsForArchive( "Qt Creator.app.7z" );
        component.addOperation( "Copy", "installer://com.nokia.qtcreator/libUpdater.dylib", "@TargetDir@/Qt Creator.app/Contents/PlugIns/Nokia/libUpdater.dylib" );
        component.addOperation( "Copy", "installer://com.nokia.qtcreator/Updater.pluginspec", "@TargetDir@/Qt Creator.app/Contents/PlugIns/Nokia/Updater.pluginspec" );
    }

    if( installer.isUpdater() )
        component.addOperation( "SelfRestart" );
}
