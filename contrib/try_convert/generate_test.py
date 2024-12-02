import re

supported_types = [
    'int8',             # NUMBERS
    'int4',
    'int2',
    'float8',
    'float4',
    'numeric',
    # 'bit',
    'bool',
    # 'varbit',
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
    # 'bytea',            # STRINGS
    # 'char',
    # 'varchar',
    'text',
    'money',
    # # 'pg_lsn',
    # # 'tsquery',
    # # 'tsvector',
    # # 'txid_snapshot',
    # 'uuid',
]

uncomparable_types = [
    'json',
    'xml',
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

print(f'Types found: {len(type_id_name)}, supported: {supported_types_count}')


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

print(f'Casts found: {len(casts)}, supported: {supported_cast_count}')


### HEADER & FOOTER

test_header = \
    f'-- SCRIPT-GENERATED TEST for TRY_CONVERT\n' \
    f'-- Tests {supported_types_count} types of {len(type_id_name)} from pg_types.h\n' \
    f'-- Tests {supported_cast_count} cast of {len(casts)} from pg_cast.h\n' \
    f'\n' \
    f'create schema tryconvert;\n' \
    f'set search_path = tryconvert;\n' \
    f'\n' \
    f'-- start_ignore\n' \
    f'CREATE EXTENSION IF NOT EXISTS try_convert;\n' \
    f'-- end_ignore\n' \
    f'\n' \

test_footer = 'reset search_path;'


### TRY_CONVERT_BY_SQL

test_funcs = ''

for type_name in supported_types:

    func_text = \
        f'CREATE OR REPLACE FUNCTION try_convert_by_sql(_in {type_name}, INOUT _out ANYELEMENT)\n' \
        f'  LANGUAGE plpgsql AS\n' \
        f'$func$\n' \
        f'    BEGIN\n' \
        f'        EXECUTE format(\'SELECT %L::%s\', $1, source_type)\n' \
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
    return  f'DELETE FROM tt_temp;' \
            f'COPY tt_temp from \'@abs_srcdir@/{filename}\';\n' \
            f'INSERT INTO {table_name}(id, v) SELECT row_number() OVER(), v::{type_name} from tt_temp;'  

for type_name in supported_types:

    table_name = f'tt_{type_name}'

    test_load_data += f'CREATE TABLE {table_name} (id serial, v {type_name}) DISTRIBUTED BY (id);\n'

    filename = f'data/{table_name}.data'

    test_load_data += copy_data(table_name, filename, type_name) + '\n'


## GET DATA

def get_data(type_name):
    return f'tt_{type_name}'


## TEST

def create_test(source_name, target_name, test_data):

    test_filter = 'not (v1 = v2)' if target_name not in uncomparable_types else 'not (v1::text = v2::text)'

    query = \
        f'select * from (' \
            f'select ' \
                f'try_convert(v, NULL::{target_name}) as v1, ' \
                f'try_convert_by_sql(v, NULL::{target_name}) as v2' \
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

for type_name in supported_types:
    test_type_data = get_data(type_name)

    load_text_data_text = f'DELETE FROM tt_temp; COPY tt_temp from \'@abs_srcdir@/data/tt_{type_name}.data\';'
    test_text_data = 'tt_temp'

    test_corrupted_text_data = f'(select (\'!@#%^&*\' || v || \'!@#%^&*\') from {test_type_data}) as t(v)'

    to_text_in, to_text_out = create_test(type_name, 'text', test_type_data)
    from_text_in, from_text_out = create_test('text', type_name, test_text_data)
    from_corrupted_text_in, from_corrupted_text_out = create_test('text', type_name, test_corrupted_text_data)

    text_tests_in += [to_text_in, load_text_data_text, from_text_in, from_corrupted_text_in]
    text_tests_out += [to_text_out, load_text_data_text, from_text_out, from_corrupted_text_out]

# print(text_tests_in[0])
# print(text_tests_in[1])


### CAST from pg_cast

function_tests_in = []
function_tests_out = []

for (source_id, target_id, method) in casts:

    source_name = type_id_name[source_id]
    target_name = type_id_name[target_id]
    test_data = get_data(source_name)

    if (source_name not in supported_types or target_name not in supported_types):
        continue

    test_in, test_out = create_test(source_name, target_name, test_data)

    function_tests_in += [test_in]
    function_tests_out += [test_out]

# print(function_tests_in[0])

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
