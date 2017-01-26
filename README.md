Introduction to mustach
=======================

mustach is a C implementation of [mustache](http://mustache.github.io "main site for mustache").

The main site for mustach is on [gitlab](https://gitlab.com/jobol/mustach).

The best way to use mustach is to copy the files **mustach.h** and **mustach.c**
in your project and use it.

The current source files are:

- **mustach.c** core implementation of mustache in C
- **mustach.h** header file for core definitions
- **mustach-json-c.c** tiny json wrapper of mustach using [json-c](https://github.com/json-c/json-c)
- **mustach-json-c.h** header file for using the tiny json wrapper
- **mustach-tool.c** simple tool for applying template files to a json file

The file **mustach-json-c.c** is the main example of use of **mustach** core
and it is also a practical implementation that can be used.

The tool **mustach** is build using Makefile. Its usage is:

    mustach json template [template]...

and it prints the result of applying the templates files to the json file.

Extensions
==========

By default, the current implementation provide the following extensions.

Explicit substitution
---------------------

This is a core extension implemented in file **mustach.c**.

In somecases the name of the key used for substition begins with a
character reserved for mustach: one of '#', '^', '/', '&', '{', '>' and '='.
This extension introduces the special character ':' to explicitely
tell mustach to just substitute the value. So ':' becomes a new special
character!

Test of the value
-----------------

This is a tool extension implmented in file **mustach-json-c.c**.

This extension allows to test the value of the selected key.
It is allowed to write key=value (matching test) or key=!value
(not matching test) in any query.

Removing extensions
-------------------

When compiling mustach.c or mustach-json-c.c,
extensions can be removed by defining macros
using option -D.

The possible macros are:

- NO_COLON_EXTENSION_FOR_MUSTACH

  This macro remove the ability to use colon (:)
  as explicit command for variable substituion.
  This extension allows to have name starting
  with one of the mustach character :#^/&{=<

- NO_EQUAL_VALUE_EXTENSION_FOR_MUSTACH

  This macro allows the tool to check the whether
  the actual value is equal to an expected value.
  This is useful in {{#key=val}} or {{^key=val}}
  with the corresponding {{/key=val}}.
  It can also be used in {{key=val}} but this
  doesn't seem to be useful.

- NO_EXTENSION_FOR_MUSTACH

  This macro disables any current or futur
  extension.

