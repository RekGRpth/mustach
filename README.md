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


