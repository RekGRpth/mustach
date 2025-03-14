1.2.10 (2024-10-03)
-------------------

Minor:
 - fix compiler warnings

1.2.9 (2024-06-04)
------------------

Change:
 - meson.build updated

1.2.8 (2024-04-16)
------------------

New:
 - test scripts now return failing status
 - explicit version of mustache specs for test (#49)
 - handling of CR (iOS) and CRLF (windows) end of line styles

Change:
 - License of use is now Zero-Clause BSD

Fix:
 - improved management and detection of valgrind (#56, #57)
 - Headers without implicit dependency

1.2.7 (2024-03-21)
------------------

New:
 - fallback to default when mustach_wrap_get_partial
   returns MUSTACH_ERROR_PARTIAL_NOT_FOUND
 - remove at compile time the load of files for templates
   if MUSTACH_LOAD_TEMPLATE is defined as 0
 - add compile time flag MUSTACH_SAFE for enforcing
   safety behaviours

Fix:
 - selection of subitem by index (#47)
 - get latest iterated key when getting key name (#52)
 - allow tests without valgrind
 - avoid recursive template expansion (#55)

1.2.6 (2024-01-08)
------------------

Fix:
 - improve naming (#42)
 - magical spaces in recursive partials (#43)
 - installation when tool isn't built
 - correct detection of falsey values (#45)

Minor:
 - update to newer reference tests

1.2.5 (2023-02-18)
------------------

Fix:
 - Don't override CFLAGS in Makefile
 - Use of $(INSTALL) in Makefile for setting options

Minor:
 - Orthograf of 'instantiate'

1.2.4 (2023-01-02)
------------------

Fix:
 - Latent SIGSEGV using cJSON

1.2.3 (2022-12-21)
------------------

New:
 - Flag Mustach_With_ErrorUndefined (and option --strict for the tool)
   for returning a requested tag is not defined
 - Test of specifications in separate directory

Fix:
 - Version printing is now okay
 - Compiling libraries on Darwin (no soname but install_name)
 - Compiling test6 with correct flags
 - Update test from specifications
 - Better use of valgrind reports

1.2.2 (2021-10-28)
------------------

Fix:
 - SONAME of libmustach-json-c.so

1.2.1 (2021-10-19)
------------------

New:
 - Add SONAME in libraries.
 - Flag Mustach_With_PartialDataFirst to switch the
   policy of resolving partials.

Fix:
 - Identification of types in cJSON

1.2.0 (2021-08-24)
------------------

New:
 - Add hook 'mustach_wrap_get_partial' for handling partials.
 - Add test of mustache specifications https://github.com/mustache/spec.

Changes:
 - Mustach_With_SingleDot is always set.
 - Mustach_With_IncPartial is always set.
 - Mustach_With_AllExtensions is changed to use currently known extensions.
 - Output of tests changed.
 - Makefile improved.
 - Partials are first searched as file then in current selection.
 - Improved management of delimiters.

Fixes:
 - Improved output accordingly to https://github.com/mustache/spec:
   - escaping of quote "
   - interpolating null with empty string
   - removal of empty lines with standalone tag
   - don't enter section if null
   - indentation of partials
 - comment improved for get of mustach_wrap_itf.

1.1.1 (2021-08-19)
------------------
Fixes:
 - Avoid conflicting with getopt.
 - Remove unexpected build artifact.
 - Handle correctly a size of 0.

1.1.0 (2021-05-01)
------------------
New:
 - API refactored to take lengths to ease working with partial or
   non-NULL-terminated strings. (ABI break)

Fixes:
 - Use correct int type for jansson (json_int_t instead of int64_t).
 - JSON output of different backends is now the same.

1.0 (2021-04-28, retracted)
---------------------------
Legal:
 - License changed to ISC.

Fixes:
 - Possible data leak in memfile_open() by clearing buffers.
 - Fix build on Solaris-likes by including alloca.h.
 - Fix Windows build by including malloc.h, using size_t instead of
   ssize_t, and using the standard ternary operator syntax.
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
