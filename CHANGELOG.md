1.1.1 (2021-08-19)
------------------
Fixes:
 - Avoid conflicting with getopt
 - Remove unexpected build artifact
 - Handle correctly a size of 0

1.1.0 (2021-05-01)
------------------
New:
 - API refactored to take lengths to ease working with partial or 
   non-NULL-terminated strings. (ABI break)

Fixes:
 - Use correct int type for jansson (json_int_t instead of int64_t).
 - JSON output of different backends is now the same.

1.0 (2021-04-28, retacted)
--------------------------
Legal:
 - License changed to ISC.

Fixes:
 - Possible data leak in memfile_open() by clearing buffers.
 - Fix build on Solaris-likes by including alloca.h.
 - Fix Windows build by including malloc.h, using size_t instead of
   ssize_t, and using the standard tenary operator syntax.
 - Fix JSON in test3 by using double quote characters.
 - Fix installation in alternative directories such as
   /opt/pkg/lib on macOS by setting install_name.
 - Normalise return values in compare() implementations.

New:
 - Support for cJSON and jansson libraries.
 - Version info now embedded at build time and shown with mustach(1) 
   usage.
 - Versioned so-names (e.g. libxlsx.so.1.0).
 - BINDIR, LIBDIR and INCLUDEDIR variables in Makefile.
 - New mustach-wrap.{c,h} to ease implementation new libraries,
   extracted and refactored from the existing implementations.
 - Makefile now supports 3 modes: single libmustach (default), split
   libmustache-core etc, and both.
 - Any or all backends (json-c, jansson, etc) can be enabled at compile
   time. By default, all available libraries are used.
 - mustach(1) can use any JSON backend instead of only json-c.
 - MUSTACH_COMPATIBLE_0_99 can be defined for backwards source
   compatibility.
 - 'No extensions' can now be set Mustach_With_NoExtensions instead of
   passing 0.
 - pkgconfig (.pc) file for library.
 - Manual page for mustach(1).

Changed:
 - Many renames.
 - Maximum tag length increased from 1024 to 4096.
 - Other headers include json-c.h instead of using forward declarations.
 - mustach(1) reads from /dev/stdin instead of fd 0.
 - Several structures are now taken as const.
 - New/changed Makefile targets.
