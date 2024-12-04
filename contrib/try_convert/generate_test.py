import re

supported_types = [
    'int8',             # NUMBERS
    'int4',
    'int2',
    'float8',
    'float4',
    'numeric',
    'bool',
    'bit',              # BITSTRING
    'varbit',
    'date',             # TIME
    'time',
    'timetz',
    'timestamp',
    'timestamptz',
    'interval',
    # 'box',              # GEOMENTY
    # 'circle',
    # 'line',
    # 'lseg',
    # 'path',
    # 'point',
    # 'polygon',
    'cidr',             # IP
    'inet',
    'macaddr',
    'json',             # JSON
    'jsonb',
    'xml',
    # 'bytea',            
    'char',             # STRINGS
    # 'bpchar',
    'varchar',
    'text',
    'money',
    # # 'pg_lsn',
    # # 'tsquery',
    # # 'tsvector',
    # # 'txid_snapshot',
    'uuid',

    'citext',
]

string_types = [
    'text',
    'citext',
    'char',
    # 'bpchar',
    'varchar',
]

typmod_types = [
    'bit',
    'varbit',
    'char',
    'varchar',
    # 'bpchar',
]

typmod_lens = [
    None, 1, 5, 10, 20
]

def get_typemod_type(t, l):
    if l is None:
        return t
    else:
        return f'{t}({l})'

def get_typemod_table(t, l):
    if l is None:
        return f'tt_{t}'
    else:
        return f'tt_{t}_{l}'

uncomparable_types = [
    'json',
    'xml',
]

extensions = [
    'citext'
]

extension_types = [
    'citext'
]

extension_casts = [
    ('citext', 'text'),
    ('citext', 'varchar'),
    ('citext', 'bpchar'),
    ('text', 'citext'),
    ('varchar', 'citext'),
    ('bpchar', 'citext'),
    ('boolean', 'citext'),
    ('inet', 'citext'),
]

print('Supported types:', ' '.join(supported_types))


pg_type_path = '/home/robozmey/gpdb_src/src/include/catalog/pg_type.h'
pg_cast_path = '/home/robozmey/gpdb_src/src/include/catalog/pg_cast.h'


def remove_empty_lines(t):
    return "\n".join([s for s in t.split("\n") if s])


### GET TYPE IDs

# TODO Is_have_IO

f = open(pg_type_path)
content = f.read()

type_pattern = r'DATA\(insert OID = (.*) \([\s]*(.*?)[\s]';

type_name_id = {}
type_id_name = {}

supported_types_count = 0

for (id, name) in re.findall(type_pattern, content):
    if name != '' and name[0] != '_':
        id = int(id)
        type_id_name[id] = name
        type_name_id[name] = id

        if name in supported_types:
            supported_types_count += 1

supported_extension_types_count = 0
for name in extension_types:
    if name in supported_types:
        supported_extension_types_count += 1

print(f'Types found: {len(type_id_name)}, supported: {supported_types_count}')
print(f'Extensions types found: {len(extension_types)}, supported: {supported_extension_types_count}')


### GET CONVERTS

f = open(pg_cast_path)
content = f.read()

cast_pattern = r'DATA\(insert \([\s]*(\d+)[\s]+(\d+)[\s]+(\d+)[\s]+(.)[\s]+(.)';

casts = []
supported_cast_count = 0

for (source, target, _, _, meth) in re.findall(cast_pattern, content):
    casts += [(int(source), int(target), meth)]
    if type_id_name[int(source)] in supported_types and type_id_name[int(target)] in supported_types:
            supported_cast_count += 1

supported_extension_casts_count = 0
for s, t in extension_casts:
     if s in supported_types and t in supported_types:
        supported_extension_casts_count += 1

print(f'Casts found: {len(casts)}, supported: {supported_cast_count}')
print(f'Extensions casts found: {len(extension_casts)}, supported: {supported_extension_casts_count}')


### HEADER & FOOTER

test_header = \
    f'-- SCRIPT-GENERATED TEST for TRY_CONVERT\n' \
    f'-- Tests {supported_types_count} types of {len(type_id_name)} from pg_types.h\n' \
    f'-- Tests {supported_cast_count} cast of {len(casts)} from pg_cast.h\n' \
    f'-- Tests {supported_extension_types_count} types of {len(extension_types)} from extensions\n' \
        f'-- Tests {supported_extension_casts_count} casts of {len(extension_casts)} from extensions\n' \
    f'\n' \
    f'create schema tryconvert;\n' \
    f'set search_path = tryconvert;\n' \
    f'\n' \
    f'-- start_ignore\n' \
    f'CREATE EXTENSION IF NOT EXISTS try_convert;\n' \
    f'-- end_ignore\n' \
    f'\n' \

for extension in extensions:
    test_header += \
        f'-- start_ignore\n' \
        f'CREATE EXTENSION IF NOT EXISTS {extension};\n' \
        f'-- end_ignore\n' \

test_footer = 'reset search_path;'


### TRY_CONVERT_BY_SQL

test_funcs = ''

func_text = \
    f'CREATE FUNCTION try_convert_by_sql_text(_in text, INOUT _out ANYELEMENT, source_type text)\n' \
    f'  LANGUAGE plpgsql AS\n' \
    f'$func$\n' \
    f'    BEGIN\n' \
    f'        EXECUTE format(\'SELECT %L::%s::%s\', $1, source_type, pg_typeof(_out))\n' \
    f'        INTO  _out;\n' \
    f'        EXCEPTION WHEN others THEN\n' \
    f'        -- do nothing: _out already carries default\n' \
    f'    END\n' \
    f'$func$;\n'

test_funcs += func_text

func_text = \
    f'CREATE FUNCTION try_convert_by_sql_text_with_len_out(_in text, INOUT _out ANYELEMENT, source_type text, len_out int)\n' \
    f'  LANGUAGE plpgsql AS\n' \
    f'$func$\n' \
    f'    BEGIN\n' \
    f'        EXECUTE format(\'SELECT %L::%s::%s(%d)\', $1, source_type, pg_typeof(_out), len_out)\n' \
    f'        INTO  _out;\n' \
    f'        EXCEPTION WHEN others THEN\n' \
    f'        -- do nothing: _out already carries default\n' \
    f'    END\n' \
    f'$func$;\n'

test_funcs += func_text

for type_name in supported_types:

    func_text = \
        f'CREATE FUNCTION try_convert_by_sql_with_len_out(_in {type_name}, INOUT _out ANYELEMENT, len_out int)\n' \
        f'  LANGUAGE plpgsql AS\n' \
        f'$func$\n' \
        f'    BEGIN\n' \
        f'        EXECUTE format(\'SELECT %L::{type_name}::%s(%s)\', $1, pg_typeof(_out), len_out::text)\n' \
        f'        INTO  _out;\n' \
        f'        EXCEPTION WHEN others THEN\n' \
        f'        -- do nothing: _out already carries default\n' \
        f'    END\n' \
        f'$func$;\n'

    test_funcs += func_text

    func_text = \
        f'CREATE FUNCTION try_convert_by_sql(_in {type_name}, INOUT _out ANYELEMENT)\n' \
        f'  LANGUAGE plpgsql AS\n' \
        f'$func$\n' \
        f'    BEGIN\n' \
        f'        EXECUTE format(\'SELECT %L::{type_name}::%s\', $1, pg_typeof(_out))\n' \
        f'        INTO  _out;\n' \
        f'        EXCEPTION WHEN others THEN\n' \
        f'        -- do nothing: _out already carries default\n' \
        f'    END\n' \
        f'$func$;\n'

    test_funcs += func_text

### CREATE DATA

import random

random.seed(42)

numbers = {
    'int2' : ((-32768, 32767), False),
    'int4' : ((-2147483648, 2147483647), False),
    'int8' : ((-9223372036854775808, 9223372036854775807), False),
    'float4' : ((10**6, 10**6), True),
    'float8' : ((10**15, 10**15), True),
    'numeric' : ((10**20, 10**20), True),
}

test_load_data = '-- LOAD DATA\n'

test_load_data += f'CREATE TABLE tt_temp (v text) DISTRIBUTED BY (v);\n'

def copy_data(table_name, filename, type_name):
    return  f'DELETE FROM tt_temp;\n' \
            f'COPY tt_temp from \'@abs_srcdir@/{filename}\';\n' \
            f'INSERT INTO {table_name}(id, v) SELECT row_number() OVER(), v::{type_name} from tt_temp;'  

type_tables = {}

def create_table(type_name, varlen=None):
    table_name = get_typemod_table(type_name, varlen)
    field_type = get_typemod_type(type_name, varlen)

    type_tables[type_name] = table_name

    load_data = f'CREATE TABLE {table_name} (id serial, v {field_type}) DISTRIBUTED BY (id);\n'

    filename = f'data/tt_{type_name}.data'

    load_data += copy_data(table_name, filename, field_type) + '\n'

    # load_data += f'SELECT * FROM {table_name};'

    return load_data

def get_string_table(type_name, string_type, type_varlen=None, string_varlen=None):

    if type_varlen is not None and string_varlen is not None:
        return f'tt_{string_type}_{string_varlen}_of_{type_name}_{type_varlen}'
    elif type_varlen is not None:
        return f'tt_{string_type}_of_{type_name}_{type_varlen}'
    elif string_varlen is not None:
        return f'tt_{string_type}_{string_varlen}_of_{type_name}'
    
    return f'tt_{string_type}_of_{type_name}'

for type_name in supported_types:

    for type_varlen in typmod_lens:
        if type_varlen is not None and type_name not in typmod_types:
            continue

        test_load_data += create_table(type_name, type_varlen)
        
        for string_type in string_types:
            for string_varlen in typmod_lens:
                if string_varlen is not None and string_type not in typmod_types:
                    continue

                field_type = get_typemod_type(type_name, type_varlen)
                string_field_type = get_typemod_type(string_type, string_varlen)

                table_name = get_string_table(type_name, string_type, type_varlen, string_varlen)
                
                load_data =  f'CREATE TABLE {table_name} (id serial, v {string_field_type}) DISTRIBUTED BY (id);\n'

                cut = f'::{field_type}' if type_varlen is not None else ''

                load_data += f'INSERT INTO {table_name}(id, v) SELECT row_number() OVER(), v{cut}::{string_field_type} from tt_temp;\n' 

                test_load_data += load_data
            


## GET DATA

def get_data(type_name):
    return type_tables[type_name]

def get_len_from_data(type_name):
    f = open(f'data/tt_{type_name}')
    return(len(f.read().split('\n')))

def get_from_data(type_name, i = None):
    f = open(f'data/tt_{type_name}.data')
    values = f.read().split('\n')
    if i is None:
        return content
    return values[i]

## TEST

def create_test(source_name, target_name, test_data, default='NULL', source_varlen=None, target_varlen=None):

    test_filter = 'v1 is distinct from v2' if target_name not in uncomparable_types else 'v1::text is distinct from  v2::text'

    try_convert_sql = f'try_convert_by_sql(v, {default}::{target_name})'

    if target_varlen is not None:
        try_convert_sql = f'try_convert_by_sql_with_len_out(v, {default}::{target_name}, {target_varlen})'

    if source_varlen is not None or source_name in ['bpchar']:

        source_name_1 = get_typemod_type(source_name, source_varlen)

        try_convert_sql = f'try_convert_by_sql_text(v::text, {default}::{target_name}, \'{source_name_1}\'::text)'

        if target_varlen is not None:
            try_convert_sql = f'try_convert_by_sql_text_with_len_out(v::text, {default}::{target_name}, \'{source_name_1}\'::text, {target_varlen})'
        
    target_name_1 = get_typemod_type(target_name, target_varlen) 

    query = \
        f'select * from (' \
            f'select ' \
                f'try_convert(v, {default}::{target_name_1}) as v1, ' \
                f'{try_convert_sql} as v2' \
            f' from {test_data}' \
    f') as t(v1, v2) where {test_filter};'
    result = \
        ' v1 | v2 \n' \
        '----+----\n' \
        '(0 rows)\n' \

    input_source = query
    output_source = remove_empty_lines(query) + '\n' + result

    return input_source, output_source


### CAST to & from text

text_tests_in = []
text_tests_out = []

default_value = 'NULL'

for string_type in string_types:
    for string_varlen in typmod_lens:
        if string_varlen is not None and type_name not in typmod_types:
            continue

        for type_name in supported_types:
            for type_varlen in typmod_lens:
                if type_varlen is not None and type_name not in typmod_types:
                    continue

                test_type_table = get_typemod_table(type_name, type_varlen)

                text_type_table = get_string_table(type_name, string_type, type_varlen, string_varlen)

                test_corrupted_text_data = f'(select (\'!@#%^&*\' || v || \'!@#%^&*\') from {text_type_table}) as t(v)'

                to_text_in, to_text_out = create_test(type_name, string_type, test_type_table, default_value, type_varlen, string_varlen)
                from_text_in, from_text_out = create_test(string_type, type_name, text_type_table, default_value, string_varlen, type_varlen)
                from_corrupted_text_in, from_corrupted_text_out = create_test(string_type, type_name, test_corrupted_text_data, default_value, string_varlen, type_varlen)

                text_tests_in += [to_text_in, from_text_in]
                text_tests_out += [to_text_out, from_text_out]

# print(text_tests_in[0])
# print(text_tests_in[1])


### CAST from pg_cast

function_tests_in = []
function_tests_out = []

type_casts = [(type_id_name[source_id], type_id_name[target_id]) for (source_id, target_id, method) in casts]  \
    + extension_casts

for source_name, target_name in type_casts:
    if (source_name not in supported_types or target_name not in supported_types):
        continue

    d = f'\'{get_from_data(target_name, 0)}\''
    for default in ['NULL']:

        for source_varlen in typmod_lens:
            if source_varlen is not None and source_name not in typmod_types:
                continue

            test_table = get_typemod_table(source_name, source_varlen)

            for target_varlen in typmod_lens:
                if target_varlen is not None and target_name not in typmod_types:
                    continue
            
                test_in, test_out = create_test(source_name, target_name, test_table, default, source_varlen, target_varlen)

                function_tests_in += [test_in]
                function_tests_out += [test_out]
        

# print(function_tests_in[0])


### DEFAULTS TEST

# for type_name in supported_types:
    
#     query = f'SELECT try_convert({}::{}, {get_from_data(type_name, 0)}::{type_name});'

### ONE MILLION ERRORS

# TODO

### NESTED CASTS

# TODO

### CONSTRUCT TEST

test_str = '\n'.join([
    test_header, \
    # FUNCTIONS
    test_funcs, \
    # CREATE DATA
    test_load_data, \
    '-- TEXT TESTS', \
    '\n'.join(text_tests_in), \
    '-- FUNCTION TESTS', \
    '\n'.join(function_tests_in), \
    test_footer
    ]) + '\n'

test_f = open('input/try_convert.source', 'w')
test_f.write(test_str)


test_str = '\n'.join([
    remove_empty_lines(test_header), \
    # FUNCTIONS
    remove_empty_lines(test_funcs), \
    # CREATE DATA
    remove_empty_lines(test_load_data), \
    '-- TEXT TESTS', \
    '\n'.join(text_tests_out), \
    '-- FUNCTION TESTS', \
    '\n'.join(function_tests_out), \
    remove_empty_lines(test_footer)
    ]) + '\n'

test_f = open('output/try_convert.source', 'w')
test_f.write(test_str)
