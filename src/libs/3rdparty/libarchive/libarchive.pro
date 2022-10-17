TEMPLATE = lib
TARGET = libarchive
INCLUDEPATH += . ..

CONFIG += staticlib

include(../../../../installerfw.pri)

DESTDIR = $$IFW_LIB_PATH

DEFINES += HAVE_CONFIG_H

HEADERS += $$PWD/archive.h \
    $$PWD/archive_entry.h \
    $$PWD/archive_acl_private.h \
    $$PWD/archive_cmdline_private.h \
    $$PWD/archive_crc32.h \
    $$PWD/archive_cryptor_private.h \
    $$PWD/archive_digest_private.h \
    $$PWD/archive_endian.h \
    $$PWD/archive_entry_locale.h \
    $$PWD/archive_entry_private.h \
    $$PWD/archive_getdate.h \
    $$PWD/archive_hmac_private.h \
    $$PWD/archive_openssl_evp_private.h \
    $$PWD/archive_openssl_hmac_private.h \
    $$PWD/archive_options_private.h \
    $$PWD/archive_pack_dev.h \
    $$PWD/archive_pathmatch.h \
    $$PWD/archive_platform.h \
    $$PWD/archive_platform_acl.h \
    $$PWD/archive_platform_xattr.h \
    $$PWD/archive_ppmd_private.h \
    $$PWD/archive_ppmd8_private.h \
    $$PWD/archive_ppmd7_private.h \
    $$PWD/archive_private.h \
    $$PWD/archive_random_private.h \
    $$PWD/archive_rb.h \
    $$PWD/archive_read_disk_private.h \
    $$PWD/archive_read_private.h \
    $$PWD/archive_string.h \
    $$PWD/archive_string_composition.h \
    $$PWD/archive_write_disk_private.h \
    $$PWD/archive_write_private.h \
    $$PWD/archive_write_set_format_private.h \
    $$PWD/archive_xxhash.h \
    $$PWD/filter_fork.h

SOURCES += $$PWD/archive_acl.c \
    $$PWD/archive_check_magic.c \
    $$PWD/archive_cmdline.c \
    $$PWD/archive_cryptor.c \
    $$PWD/archive_digest.c \
    $$PWD/archive_entry.c \
    $$PWD/archive_entry_copy_stat.c \
    $$PWD/archive_entry_link_resolver.c \
    $$PWD/archive_entry_sparse.c \
    $$PWD/archive_entry_stat.c \
    $$PWD/archive_entry_strmode.c \
    $$PWD/archive_entry_xattr.c \
    $$PWD/archive_getdate.c \
    $$PWD/archive_hmac.c \
    $$PWD/archive_match.c \
    $$PWD/archive_options.c \
    $$PWD/archive_pack_dev.c \
    $$PWD/archive_pathmatch.c \
    $$PWD/archive_ppmd8.c \
    $$PWD/archive_ppmd7.c \
    $$PWD/archive_random.c \
    $$PWD/archive_rb.c \
    $$PWD/archive_read.c \
    $$PWD/archive_read_add_passphrase.c \
    $$PWD/archive_read_append_filter.c \
    $$PWD/archive_read_data_into_fd.c \
    $$PWD/archive_read_disk_entry_from_file.c \
    $$PWD/archive_read_disk_posix.c \
    $$PWD/archive_read_disk_set_standard_lookup.c \
    $$PWD/archive_read_extract.c \
    $$PWD/archive_read_extract2.c \
    $$PWD/archive_read_open_fd.c \
    $$PWD/archive_read_open_file.c \
    $$PWD/archive_read_open_filename.c \
    $$PWD/archive_read_open_memory.c \
    $$PWD/archive_read_set_format.c \
    $$PWD/archive_read_set_options.c \
    $$PWD/archive_read_support_filter_all.c \
    $$PWD/archive_read_support_filter_bzip2.c \
    $$PWD/archive_read_support_filter_compress.c \
    $$PWD/archive_read_support_filter_gzip.c \
    $$PWD/archive_read_support_filter_grzip.c \
    $$PWD/archive_read_support_filter_lrzip.c \
    $$PWD/archive_read_support_filter_lz4.c \
    $$PWD/archive_read_support_filter_lzop.c \
    $$PWD/archive_read_support_filter_none.c \
    $$PWD/archive_read_support_filter_program.c \
    $$PWD/archive_read_support_filter_rpm.c \
    $$PWD/archive_read_support_filter_uu.c \
    $$PWD/archive_read_support_filter_xz.c \
    $$PWD/archive_read_support_filter_zstd.c \
    $$PWD/archive_read_support_format_7zip.c \
    $$PWD/archive_read_support_format_all.c \
    $$PWD/archive_read_support_format_ar.c \
    $$PWD/archive_read_support_format_by_code.c \
    $$PWD/archive_read_support_format_cab.c \
    $$PWD/archive_read_support_format_cpio.c \
    $$PWD/archive_read_support_format_empty.c \
    $$PWD/archive_read_support_format_iso9660.c \
    $$PWD/archive_read_support_format_lha.c \
    $$PWD/archive_read_support_format_mtree.c \
    $$PWD/archive_read_support_format_rar.c \
    $$PWD/archive_read_support_format_rar5.c \
    $$PWD/archive_read_support_format_raw.c \
    $$PWD/archive_read_support_format_tar.c \
    $$PWD/archive_read_support_format_warc.c \
    $$PWD/archive_read_support_format_xar.c \
    $$PWD/archive_read_support_format_zip.c \
    $$PWD/archive_string.c \
    $$PWD/archive_string_sprintf.c \
    $$PWD/archive_util.c \
    $$PWD/archive_version_details.c \
    $$PWD/archive_virtual.c \
    $$PWD/archive_write.c \
    $$PWD/archive_write_disk_posix.c \
    $$PWD/archive_write_disk_set_standard_lookup.c \
    $$PWD/archive_write_open_fd.c \
    $$PWD/archive_write_open_file.c \
    $$PWD/archive_write_open_filename.c \
    $$PWD/archive_write_open_memory.c \
    $$PWD/archive_write_add_filter.c \
    $$PWD/archive_write_add_filter_b64encode.c \
    $$PWD/archive_write_add_filter_by_name.c \
    $$PWD/archive_write_add_filter_bzip2.c \
    $$PWD/archive_write_add_filter_compress.c \
    $$PWD/archive_write_add_filter_grzip.c \
    $$PWD/archive_write_add_filter_gzip.c \
    $$PWD/archive_write_add_filter_lrzip.c \
    $$PWD/archive_write_add_filter_lz4.c \
    $$PWD/archive_write_add_filter_lzop.c \
    $$PWD/archive_write_add_filter_none.c \
    $$PWD/archive_write_add_filter_program.c \
    $$PWD/archive_write_add_filter_uuencode.c \
    $$PWD/archive_write_add_filter_xz.c \
    $$PWD/archive_write_add_filter_zstd.c \
    $$PWD/archive_write_set_format.c \
    $$PWD/archive_write_set_format_7zip.c \
    $$PWD/archive_write_set_format_ar.c \
    $$PWD/archive_write_set_format_by_name.c \
    $$PWD/archive_write_set_format_cpio.c \
    $$PWD/archive_write_set_format_cpio_binary.c \
    $$PWD/archive_write_set_format_cpio_newc.c \
    $$PWD/archive_write_set_format_cpio_odc.c \
    $$PWD/archive_write_set_format_filter_by_ext.c \
    $$PWD/archive_write_set_format_gnutar.c \
    $$PWD/archive_write_set_format_iso9660.c \
    $$PWD/archive_write_set_format_mtree.c \
    $$PWD/archive_write_set_format_pax.c \
    $$PWD/archive_write_set_format_raw.c \
    $$PWD/archive_write_set_format_shar.c \
    $$PWD/archive_write_set_format_ustar.c \
    $$PWD/archive_write_set_format_v7tar.c \
    $$PWD/archive_write_set_format_warc.c \
    $$PWD/archive_write_set_format_xar.c \
    $$PWD/archive_write_set_format_zip.c \
    $$PWD/archive_write_set_options.c \
    $$PWD/archive_write_set_passphrase.c \
    $$PWD/filter_fork_posix.c \
    $$PWD/xxhash.c

if (isEmpty(IFW_ZLIB_LIBRARY):contains(QT_MODULES, zlib)) {
    INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtZlib
}

linux {
    INCLUDEPATH += ./config/linux
    HEADERS += $$PWD/config/linux/config.h
    SOURCES += $$PWD/archive_disk_acl_linux.c
}

macx {
    INCLUDEPATH += ./config/mac
    HEADERS += $$PWD/config/mac/config.h
    SOURCES += $$PWD/archive_disk_acl_darwin.c
}

win32:!cygwin-g++ {
    INCLUDEPATH += ./config/win
    HEADERS += $$PWD/config/win/config.h \
        $$PWD/archive_windows.h
    SOURCES += $$PWD/archive_entry_copy_bhfi.c \
        $$PWD/archive_read_disk_windows.c \
        $$PWD/archive_windows.c \
        $$PWD/archive_write_disk_windows.c \
        $$PWD/filter_fork_windows.c
}

target.path = $$[QT_INSTALL_LIBS]
INSTALLS += target
