project(
    'mustach',
    'c',
    version: '1.2.8',
    license: '0BSD',
)

json_dict = {
    'cjson': {
        'dep': 'libcjson',
        'xxx': 'cjson',
        'tool_flag': '-DTOOL=MUSTACH_TOOL_CJSON',
        'test_flag': '-DTEST=TEST_CJSON',
    },
    'jsonc': {
        'dep': 'json-c',
        'xxx': 'json-c',
        'tool_flag': '-DTOOL=MUSTACH_TOOL_JSON_C',
        'test_flag': '-DTEST=TEST_JSON_C',
    },
    'jansson': {
        'dep': 'jansson',
        'xxx': 'jansson',
        'tool_flag': '-DTOOL=MUSTACH_TOOL_JANSSON',
        'test_flag': '-DTEST=TEST_JANSSON',
    },
}

core_src = [
    'mustach.c',
    'mustach-wrap.c',
]

install_headers(['mustach.h', 'mustach-wrap.h'], subdir: 'mustach')

all_src = core_src
all_dep = []
all_key = [] # use name

foreach opt, value : json_dict
    if get_option(opt).disabled()
        continue
    endif
    src = 'mustach-' + value['xxx'] + '.c'
    lib = 'mustach-' + value['xxx']
    json_dep = dependency(value['dep'], required: get_option(opt).enabled())
    if json_dep.found()
        all_src += src
        all_dep += json_dep
        all_key += opt
        if get_option('libs') in ['all', 'split']
            library(lib, [core_src, src], dependencies: json_dep, install: true)
        endif
        install_headers('mustach-' + value['xxx'] + '.h', subdir: 'mustach')
    endif
endforeach

if get_option('libs') in ['all', 'single']
    library('mustach', all_src, dependencies: all_dep, install: true)
endif
if get_option('libs') in ['all', 'split']
    library('mustach-core', core_src, install: true)
endif

tool_opt = get_option('tool')

if tool_opt != 'none'
    if all_key.length() == 0
        error('No library found for tool')
    endif
    value = tool_opt == 'auto' ? json_dict[all_key[0]] : json_dict[tool_opt]
    dep = (
        tool_opt == 'auto' ? all_dep[0] : dependency(json_dict[tool_opt]['dep'])
    )
    tool_e = executable(
        'mustach',
        [core_src, 'mustach-tool.c', 'mustach-' + value['xxx'] + '.c'],
        dependencies: dep,
        c_args: value['tool_flag'],
        install: true,
    )
    make = find_program('make')
    foreach n : ['1', '2', '3', '4', '5', '6', '7', '8']
        test_n = 'test' + n
        test(
            'tool ' + test_n,
            make,
            args: ['test'],
            depends: tool_e,
            workdir: meson.current_source_dir() / test_n,
            env: ['NOVALGRIND=1', 'mustach=' + tool_e.full_path()],
        )
    endforeach
endif

foreach opt : all_key
    value = json_dict[opt]
    e = executable(
        value['xxx'] + '-test-specs',
        [
            core_src,
            'test-specs/test-specs.c',
            'mustach-' + value['xxx'] + '.c',
        ],
        dependencies: dependency(value['dep']),
        c_args: value['test_flag'],
    )
    test('specs ' + value['xxx'], e)
endforeach
