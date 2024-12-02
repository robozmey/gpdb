import re

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

for (id, name) in re.findall(type_pattern, content):
    if name != '' and name[0] != '_':
        id = int(id)
        type_id_name[id] = name
        type_name_id[name] = id

print('Types found:', len(type_id_name))
# print('\n'.join([str(i) + ' ' + type_id_name[i] for i in sorted(type_id_name)]))


### GET CONVERTS

f = open(pg_cast_path)
content = f.read()

cast_pattern = r'DATA\(insert \([\s]*(\d+)[\s]+(\d+)[\s]+(\d+)[\s]+(.)[\s]+(.)';

casts = []

for (source, target, _, _, meth) in re.findall(cast_pattern, content):
    casts += [(int(source), int(target), meth)]

print('Casts found:', len(casts))
# print('\n'.join([str(c[0]) + ' ' + c[1] + ' ' + c[2] for c in casts]))

supported_types = [
    'int8',             # NUMBERS
    'int4',
    'int2',
    'float8',
    'float4',
    'numeric',
    # 'bit',
    # 'bool',
    # 'varbit',
    # 'date',             # TIME
    # 'time',
    # 'timez',
    # 'timestamp',
    # 'timestampz',
    # 'box',              # GEOMENTY
    # 'circle',
    # 'line',
    # 'lseg',
    # 'path',
    # 'point',
    # 'polygon',
    # 'cidr',             # IP
    # 'inet',
    # 'json',             # JSON
    # 'jsonb',
    # 'bytea',            # STRINGS
    # 'char',
    # 'varchar',
    # 'text',
    # 'interval',
    # 'macaddr',
    # 'macaddr8',
    # 'money',
    # # 'pg_lsn',
    # # 'tsquery',
    # # 'tsvector',
    # # 'txid_snapshot',
    # 'uuid',
    # 'xml'
]

print(supported_types)


### HEADER & FOOTER

test_header = open('test_header.sql').read()
test_footer = open('test_footer.sql').read()


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


test_load_data = '-- CREATE DATA\n'

for number_type in numbers:

    type_range = numbers[number_type][0]
    is_float = numbers[number_type][1]

    mn = type_range[0]
    mx = type_range[1]

    nln = 0
    ln = len(str(type_range[1]))-1

    table_name = f'tt_{number_type}'

    test_load_data += f'CREATE TABLE {table_name} (v {number_type}) DISTRIBUTED BY (v);\n'

    filename = f'data/{table_name}.data'

    test_load_data += f'COPY {table_name} from \'@abs_srcdir@/{filename}\';\n'      

##### COPY real_city FROM '@abs_srcdir@/data/real_city.data';

## GET DATA

data_sources = {
    'json' : '@abs_srcdir@/data/jsonb.data'
}

def get_data(type_name):
    return f'tt_{type_name}'



## TEST

def create_test(source_name, target_name, test_data):

    query = \
        f'select * from (' \
            f'select ' \
                f'try_convert(v, NULL::{target_name}) as v1, ' \
                f'try_convert_by_sql(v, NULL::{target_name}) as v2' \
            f' from {test_data}' \
    f') as t(v1, v2) where not (v1 = v2);'
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
    test_text_data = f'(select v::text from {test_type_data}) as t(v)'

    to_text_in, to_text_out = create_test(type_name, 'text', test_type_data)
    from_text_in, from_text_out = create_test('text', type_name, test_text_data)

    text_tests_in += [to_text_in, from_text_in]
    text_tests_out += [to_text_out, from_text_out]

print(text_tests_in[0])
print(text_tests_in[1])


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

print(function_tests_in[0])


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
