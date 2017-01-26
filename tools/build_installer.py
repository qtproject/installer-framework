#!/usr/bin/env python
#############################################################################
##
## Copyright (C) 2017 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the Qt Installer Framework.
##
## $QT_BEGIN_LICENSE:GPL-EXCEPT$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3 as published by the Free Software
## Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################

import argparse
import os
import shutil
import string
import subprocess
import sys

args = {}

src_dir = ''
build_dir = ''
package_dir = ''
archive_file = ''
target_path = ''

def parse_arguments():
    global args

    parser = argparse.ArgumentParser(description='Build installer for installer framework.')
    parser.add_argument('--clean', dest='clean', action='store_true', help='delete all previous build artifacts')
    parser.add_argument('--static-qmake', dest='qmake', required=True, help='path to qmake that will be used to build the tools')
    parser.add_argument('--doc-qmake', dest='doc_qmake', required=True, help='path to qmake that will be used to generate the documentation')
    parser.add_argument('--make', dest='make', required=True, help='make command')
    parser.add_argument('--targetdir', dest='target_dir', required=True, help='directory the generated installer will be placed in')
    if sys.platform == 'darwin':
        parser.add_argument('--qt_menu_nib', dest='menu_nib', required=True, help='location of qt_menu.nib (usually src/gui/mac/qt_menu.nib)')

    args = parser.parse_args()

def run(args):
    print 'calling ' + string.join(args)
    subprocess.check_call(args)

def init():
    global src_dir
    global build_dir
    global package_dir
    global target_path

    src_dir = os.path.dirname(os.path.abspath(os.path.dirname(sys.argv[0])))
    root_dir = os.path.dirname(src_dir)
    basename = os.path.basename(src_dir)
    build_dir = os.path.join(root_dir, basename + '_build')
    package_dir = os.path.join(root_dir, basename + '_pkg')
    target_path = os.path.join(args.target_dir, 'Qt Installer Framework')

    print 'source dir: ' + src_dir
    print 'build dir: ' + build_dir
    print 'package dir: ' + package_dir
    print 'target path: ' + target_path

    if args.clean and os.path.exists(build_dir):
        print 'delete existing build dir ...'
        shutil.rmtree(build_dir)
    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    if os.path.exists(args.target_dir):
        print 'delete existing target dir ...'
        shutil.rmtree(args.target_dir)
    os.makedirs(args.target_dir)

    if os.path.exists(package_dir):
        print 'delete existing package dir ...'
        shutil.rmtree(package_dir)
    os.makedirs(package_dir)

def build_docs():
    print 'building documentation ...'
    os.chdir(build_dir)
    run((args.doc_qmake, src_dir))
    run((args.make, 'docs'))
    print 'success!'

def build():
    print 'building sources ...'
    os.chdir(build_dir)
    run((args.qmake, src_dir))
    run((args.make))

def package():
    global package_dir
    print 'package ...'
    os.chdir(package_dir)
    shutil.copytree(os.path.join(build_dir, 'bin'), os.path.join(package_dir, 'bin'), ignore = shutil.ignore_patterns("*.exe.manifest","*.exp","*.lib"))
    if sys.platform == 'linux2':
        run(('strip',os.path.join(package_dir, 'bin/archivegen')))
        run(('strip',os.path.join(package_dir, 'bin/binarycreator')))
        run(('strip',os.path.join(package_dir, 'bin/devtool')))
        run(('strip',os.path.join(package_dir, 'bin/installerbase')))
        run(('strip',os.path.join(package_dir, 'bin/repogen')))
    shutil.copytree(os.path.join(build_dir, 'doc'), os.path.join(package_dir, 'doc'))
    shutil.copytree(os.path.join(src_dir, 'examples'), os.path.join(package_dir, 'examples'))
    shutil.copy(os.path.join(src_dir, 'README'), package_dir)
    # create 7z
    archive_file = os.path.join(src_dir, 'dist', 'packages', 'org.qtproject.ifw.binaries', 'data', 'data.7z')
    if not os.path.exists(os.path.dirname(archive_file)):
        os.makedirs(os.path.dirname(archive_file))
    run((os.path.join(package_dir, 'bin', 'archivegen'), archive_file, '*'))
    # run installer
    binary_creator = os.path.join(build_dir, 'bin', 'binarycreator')
    config_file = os.path.join(src_dir, 'dist', 'config', 'config.xml')
    package_dir = os.path.join(src_dir, 'dist', 'packages')
    installer_path = os.path.join(src_dir, 'dist', 'packages')
    run((binary_creator, '--offline-only', '-c', config_file, '-p', package_dir, target_path))
    if sys.platform == 'darwin':
        shutil.copytree(args.menu_nib, target_path + '.app/Contents/Resources/qt_menu.nib')


parse_arguments()
init()
build_docs()
build()
package()

print 'DONE, installer is at ' + target_path
