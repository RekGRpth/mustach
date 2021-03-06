mustach(1)

# NAME

mustach - Mustache templating command line engine

# SYNOPSIS

*mustach* JSON TEMPLATE...

# DESCRIPTION

Instanciate the TEMPLATE files accordingly to the JSON file.

If one of the given files is *-*, the standard input is used.

# EXAMPLE

A typical Mustache template file: *temp.must*

```
Hello {{name}}
You have just won {{value}} dollars!
{{#in_ca}}
Well, {{taxed_value}} dollars, after taxes.
{{/in_ca}}
```

Given a JSON file: *inst.json*

```
{
  "name": "Chris",
  "value": 10000,
  "taxed_value": 10000 - (10000 * 0.4),
  "in_ca": true
}
```

Calling the command *mustach inst.json temp.must*
will produce the following output:

```
Hello Chris
You have just won 10000 dollars!
Well, 6000.0 dollars, after taxes.
```

# LINK

Site of *mustach*, the *C* implementation: https://gitlab.com/jobol/mustach

*Mustache format*: http://mustache.github.io/mustache.5.html

Main site for *Mustache*: http://mustache.github.io/

JSON: https://www.json.org/

