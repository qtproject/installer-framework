#include <QCoreApplication>
#include <QProcess>
#include <QDebug>
#include <QDirIterator>
#include <QDateTime>
#include <iostream>

//rebase to adress 0x20000000 results in crashing tools like perl on win7/64bit
//which I used for testing
bool rebaseDlls(const QString &maddeBinLocation,
                const QString &rebaseBinaryLocation,
                const QString &adressValue = "0x50000000",
                const QString &dllFilter = "msys*.dll")
{
    QStringList dllStringList = QDir(maddeBinLocation).entryList(QStringList(dllFilter), QDir::Files, QDir::Size | QDir::Reversed);
    QString dlls;
    QString dllsEnd; //of an unknown issue msys-1.0.dll should be the last on my system
    foreach (QString dll, dllStringList) {
        dll.prepend(maddeBinLocation + "/");
        if (dll.contains("msys-1")) {
            dllsEnd.append(dll + " ");
        } else {
            dlls.append(dll + " ");
        }
    }
    dlls = dlls.append(dllsEnd);

    QProcess process;
    process.setEnvironment(QStringList("EMPTY_ENVIRONMENT=true"));
    QProcess::ProcessError initError(process.error());
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setNativeArguments(QString("-b %1 -o 0x10000 -v %2").arg(adressValue, dlls));
    process.start(rebaseBinaryLocation);
    process.waitForFinished();
    if (process.exitCode() != 0 ||
        initError != process.error() ||
        process.exitStatus() == QProcess::CrashExit)
    {
        qWarning() << rebaseBinaryLocation + " " + process.nativeArguments();
        qWarning() << QString("\t Adress rebasing failed! Maybe a process(bash.exe, ssh, ...) is using some of the dlls(%1).").arg(dllStringList.join(", "));
        qWarning() << "\t Output: \n" << process.readAll();
        if (initError != process.error()) {
            qDebug() << QString("\tError(%1): ").arg(process.error()) << process.errorString();
        }

        if (process.exitStatus() == QProcess::CrashExit) {
            qDebug() << "\tcrashed!!!";
        }
        return false;
    }
    qWarning() << rebaseBinaryLocation + " " + process.nativeArguments();
    //qWarning() << "\t Output: \n" << process.readAll();
    return true;
}

bool checkTools(const QString &maddeBinLocation, const QStringList &binaryCheckList)
{
    QDirIterator it( maddeBinLocation, binaryCheckList, QDir::Files );
    while (it.hasNext()) {
        QString processPath(it.next());
        if (!QFileInfo(processPath).exists()) {
            qDebug() << processPath << " is missing - so we don't need to check it";
            continue;
        }
        bool testedToolState = true;
        QProcess process;
        process.setEnvironment(QStringList("EMPTY_ENVIRONMENT=true"));
        QProcess::ProcessError initError(process.error());
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(processPath, QStringList("--version"));
        process.waitForFinished(1000);
        QString processOutput = process.readAll();

        //check for the unpossible load dll error
        if (processOutput.contains("VirtualAlloc pointer is null") ||
            processOutput.contains("Couldn't reserve space for") ||
            process.exitStatus() == QProcess::CrashExit)
        {
            qWarning() << QString("found dll loading problem with(ExitCode: %1): ").arg(QString::number(process.exitCode())) << processPath;
            qWarning() << processOutput;
            testedToolState = false;
            if (initError != process.error()) {
                qWarning() << QString("\tError(%1): ").arg(process.error()) << process.errorString();
            }

            if (process.exitStatus() == QProcess::CrashExit) {
                qWarning() << "\tcrashed!!!";
            }
        }
        if ( process.state() == QProcess::Running )
            process.terminate();
        //again wait some time
        if ( !process.waitForFinished( 1000 ) ) {
            process.kill();
            process.waitForFinished( 1000 ); //some more waittime
        }
        if (testedToolState == false) {
            return false;
        }
    }
    return true;
}

bool removeDirectory( const QString& path )
{
    if ( !QFileInfo(path).exists() )
        return true;
    if ( path.isEmpty() ) // QDir( "" ) points to the working directory! We never want to remove that one.
        return true;

    QDirIterator it(path, QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System);
    while ( it.hasNext() )
    {
        it.next();
        const QFileInfo currentFileInfo = it.fileInfo();

        if ( currentFileInfo.isDir() && !currentFileInfo.isSymLink() )
        {
            removeDirectory( currentFileInfo.filePath() );
        }
        else
        {
            QFile f( currentFileInfo.filePath() );
            if( !f.remove() )
                qWarning() << "Can't remove: " << currentFileInfo.absoluteFilePath();
        }
    }

    if ( !QDir().rmdir(path))
        return QDir().rename(path, path + "_" + QDateTime::currentDateTime().toString());
    return true;
}

bool cleanUpSubDirectories(const QString &maddeLocation, const QStringList &subDirectoryList, const QStringList &extraFiles)
{
    if (maddeLocation.isEmpty()) {
        qWarning() << "Remove nothing could be result in a broken system ;)";
        return false;
    }
    if (!QFileInfo(maddeLocation).exists()) {
        qWarning() << QString("Ups, location '%1' is not existing.").arg(maddeLocation);
        return false;
    }

    foreach (const QString &subDirectory, subDirectoryList) {
        bool removed = removeDirectory(maddeLocation + "/" + subDirectory);
        if (!removed) {
            qWarning() << "Can't remove " << subDirectory <<" for a clean up step";
            return false;
        }
    }
    foreach (const QString &fileName, extraFiles) {
        QFile file(fileName);
        if (file.exists()) {
            bool removed = QFile(fileName).remove();
            if (!removed) {
                qWarning() << "Can't remove " << extraFiles <<" for a clean up step";
            }
        }
    }

    return true;
}

bool runInstall(const QString &postinstallCommand, const QString &checkFileForAnOkInstallation)
{
    QProcess process;
    process.setEnvironment(QStringList("EMPTY_ENVIRONMENT=true"));
    QProcess::ProcessError initError(process.error());
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.setNativeArguments(postinstallCommand);
    process.start(QString(), QStringList());

    process.waitForFinished(-1);
    if (process.exitCode() != 0 ||
        initError != process.error() ||
        process.exitStatus() == QProcess::CrashExit)
    {
        qWarning() << QString("runInstall(ExitCode: %1) went wrong \n'%2'").arg(QString::number(process.exitCode()), postinstallCommand);
        if (initError != process.error()) {
            qDebug() << QString("\tError(%1): ").arg(process.error()) << process.errorString();
        }

        if (process.exitStatus() == QProcess::CrashExit) {
            qDebug() << "\tcrashed!!!";
        }
    }
    if ( process.state() == QProcess::Running )
        process.terminate();
    //again wait some time
    if ( !process.waitForFinished( 1000 ) )
        process.kill();

    return QFileInfo(checkFileForAnOkInstallation).exists();
}


static void printUsage()
{
    const QString appName = QFileInfo( QCoreApplication::applicationFilePath() ).fileName();
    std::cout << "Usage: " << qPrintable(appName) << " -r <rebase_binary> -m <MADDE_directory>" << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << qPrintable(appName) << " -r D:/msysgit_rebase.exe -m D:/Maemo/4.6.2" << std::endl;
}


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if ( app.arguments().count() != 5 ) //5. is app name
    {
        printUsage();
        return 6;
    }

    QString rebaseBinaryLocation;
    QString maddeLocation;
    for ( int i = 1; i < app.arguments().size(); ++i ) {
        if ( app.arguments().at( i ) == QLatin1String( "" ) )
            continue;
        if (app.arguments().at(i) == "-r") {
            i++;
            rebaseBinaryLocation = app.arguments().at(i);
        }
        if (app.arguments().at(i) == "-m") {
            i++;
            maddeLocation = app.arguments().at(i);
        }
    }
    if (!QFileInfo(maddeLocation).isDir()) {
        qDebug() << "MADDE location is not existing.";
        return 4;
    }
    if (!QFileInfo(maddeLocation + "/bin").isDir()) {
        qDebug() << "It seems that the madde location don't have a 'bin'' directory.";
        return 5;
    }
    QString maddeBinLocation = maddeLocation + "/bin";

    //from qs
//    var envExecutable = installer.value("TargetDir") + "/" + winMaddeSubPath + "/bin/env.exe";
//    var postShell = installer.value("TargetDir") + "/" + winMaddeSubPath + "/postinstall/postinstall.sh";
//    component.addOperation("Execute", "{0,1}", envExecutable, "-i" , "/bin/sh", "--login", postShell, "--nosleepontrap", "showStandardError");

    QString postinstallShell = maddeLocation + "/postinstall/postinstall.sh";
    //"--nosleepontrap" is not used in our Madde, but later maybe it is there again
    QString postinstallCommand = maddeBinLocation + QString("/env.exe -i /bin/sh --login %1").arg(postinstallShell + " --nosleepontrap");
    QString successFileCheck = maddeLocation + "/madbin/mad.cmd";
    QString directoriesForCleanUpArgument = "sysroots, toolchains, targets, runtimes, wbin";
    directoriesForCleanUpArgument.remove(" ");
    QStringList directoriesForCleanUp(directoriesForCleanUpArgument.split(","));

    //this was used for testings
    //cleanUpSubDirectories(maddeLocation, directoriesForCleanUp, QStringList(successFileCheck));

    bool installationWentFine = runInstall(postinstallCommand, successFileCheck);

    if (installationWentFine)
        return 0;


    QString binaryCheckArgument = ("a2p.exe, basename.exe, bash.exe, bzip2.exe, cat.exe, chmod.exe, cmp.exe, comm.exe, cp.exe, cut.exe, " \
                                   "date.exe, diff.exe, diff3.exe, dirname.exe, du.exe, env.exe, expr.exe, find.exe, fold.exe, gawk.exe, " \
                                   "grep.exe, gzip.exe, head.exe, id.exe, install.exe, join.exe, less.exe, ln.exe, ls.exe, lzma.exe, m4.exe, " \
                                   "make.exe, md5sum.exe, mkdir.exe, mv.exe, od.exe, paste.exe, patch.exe, perl.exe, perl5.6.1.exe, ps.exe, " \
                                   "rm.exe, rmdir.exe, sed.exe, sh.exe, sleep.exe, sort.exe, split.exe, ssh-agent.exe, ssh-keygen.exe, " \
                                   "stty.exe, tail.exe, tar.exe, tee.exe, touch.exe, tr.exe, true.exe, uname.exe, uniq.exe, vim.exe, wc.exe, xargs.exe");
    binaryCheckArgument.remove(" ");
    QStringList binaryCheckList(binaryCheckArgument.split(","));

    QStringList possibleRebaseAdresses;
    possibleRebaseAdresses << "0x5200000"; //this uses perl
    possibleRebaseAdresses << "0x30000000";
    possibleRebaseAdresses << "0x35000000";
    possibleRebaseAdresses << "0x40000000";
    possibleRebaseAdresses << "0x60000000";
    possibleRebaseAdresses << "0x60800000";
    possibleRebaseAdresses << "0x68000000"; //this uses git

    foreach (const QString &newAdress, possibleRebaseAdresses) {
        bool rebased = rebaseDlls(maddeBinLocation, rebaseBinaryLocation, newAdress);
        if (!rebased) {
            //rebasing is not working
            return 2;
        }
        bool reseted = cleanUpSubDirectories(maddeLocation, directoriesForCleanUp, QStringList(successFileCheck));
        if (!reseted) {
            //Madde couldn't reseted to the starting position
            return 3;
        }
        if (!checkTools(maddeBinLocation, binaryCheckList)) {
            //we need another adress
            continue;
        }
        bool installationWentFine = runInstall(postinstallCommand, successFileCheck);

        if (installationWentFine) {
            qDebug() << "Rebasing dlls to " << newAdress <<" helped to install MADDE";
            return 0;
        }
    }

    //means rebasing was not helping
    return 1;
}
