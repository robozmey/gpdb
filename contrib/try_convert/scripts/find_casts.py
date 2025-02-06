
import re
import os, glob


top_srcdir = '../..'

pg_type_path = f'{top_srcdir}/src/include/catalog/pg_type.h'
pg_cast_path = f'{top_srcdir}/src/include/catalog/pg_cast.h'
pg_proc_path = f'{top_srcdir}/src/backend/catalog/pg_proc_combined.h'


def get_pg_proc():
    f = open(pg_proc_path)
    content = f.read()
    
    # DATA(insert OID =  46 (  textin			   PGNSP PGUID 12 1 0 0 0 f f f f t f i 1 0 25 "2275" _null_ _null_ _null_ _null_ textin _null_ _null_ _null_ ));
    func_pattern = r'DATA\(insert OID =\s+(\w*)\s+\(.*?(\w*) _null_ _null_ _null_ n?\s?a?\s?\)\);';

    func_id_name = {}

    func_id_name['0'] = 'via I/O'

    for (id, name) in re.findall(func_pattern, content):
        func_id_name[id] = name

    return func_id_name


def get_pg_type():
    f = open(pg_type_path)
    content = f.read()

    type_pattern = r'DATA\(insert OID = (.*) \([\s]+(.*?)[\s]+(.*?\s+){12}(.*?)\s+(.*?)\s+';

    type_name_id = {}
    type_id_name = {}
    type_io_funcs = {}

    for t in re.findall(type_pattern, content):
        id = t[0]
        name = t[1]
        infunc = t[3]
        outfunc = t[4]

        type_io_funcs[name] = [infunc, outfunc]
        # print(len(t), t[3])
        if name != '' and name[0] != '_':
            id = int(id)
            type_id_name[id] = name
            type_name_id[name] = id

    return type_name_id, type_id_name, type_io_funcs


def get_pg_cast(type_id_name, func_id_name):
    f = open(pg_cast_path)
    content = f.read()

    cast_pattern = r'DATA\(insert \([\s]*(\d+)[\s]+(\d+)[\s]+(\d+)[\s]+(.)[\s]+(.)';

    casts = []

    for (source, target, funcid, _, meth) in re.findall(cast_pattern, content):
        if int(source) not in type_id_name or int(target) not in type_id_name:
            continue

        way = '______unknown'

        if meth == 'f':
            if funcid not in func_id_name:
                way = '______unknown_funcid'
            else:
                way = func_id_name[funcid]
                if way == '':
                    way = '______unknown_funcid'
                
        elif meth == 'i':
            way = 'WITH INOUT'
        elif meth == 'b':
            way = 'WITHOUT FUNCTION'




        casts += [(type_id_name[int(source)], type_id_name[int(target)], way)]
        # print(type_id_name[int(source)], ' -> ', type_id_name[int(target)], ' via ', meth, f'({funcid} - {func_id_name[funcid]}) ', f'{source}-{target}')

    return casts


### GET FROM TEXT (EXTENSIONS)

from general import supported_extensions

def get_extensions():

    create_casts = []
    create_functions = []

    for extension in supported_extensions:
        for root, subdirs, files in os.walk(f'../{extension}'):
                for filename in files:
                    file_path = os.path.join(root, filename)
                    if filename[-4:] == '.sql':
                        # print(file_path)
                        with open(file_path, 'r') as f:
                            content = f.read()

                            create_casts += find_create_casts_in_text(content)

                            create_functions += list(find_create_function_in_text(content).items())

    return create_casts, dict(create_functions)

def find_create_casts_in_text(text):
    create_cast_pattern = 'CREATE CAST\s*\((\w+) AS (\w+)\)\s*([\w\s\(\)]+);'
    create_casts = []

    for target, source, f in re.findall(create_cast_pattern, text):
        # print(target, source, f)
        if re.match('WITH INOUT', f) is not None:
            create_casts += [(target, source, 'WITH INOUT')]
        if re.match('WITHOUT FUNCTION', f) is not None:
            create_casts += [(target, source, 'WITHOUT FUNCTION')]

        m = re.match('WITH FUNCTION ([\w\(\)]+)', f)
        if m is not None:
            # print(m[1])
            create_casts += [(target, source, m[1])]
    
    return create_casts

p_space = '\s+'

def find_create_function_in_text(text):
    create_function_pattern = 'CREATE FUNCTION' + p_space + '(\w+)\([\w\s,]+\)' + p_space + 'RETURNS' + p_space + '\w+' + p_space + 'AS' + p_space + '([\',\w\s]+)' + p_space + 'LANGUAGE[\w\s,]+;'
    create_functions = {}

    for sql_name, c_obj in re.findall(create_function_pattern, text):
        # print(target, source, f)
        
        m = re.fullmatch("\'(\w+)\'", c_obj)
        if m is not None:
            c_name = m[1]
            create_functions[sql_name] = c_name
            continue

        m = re.fullmatch("\'MODULE_PATHNAME\',\s+\'(\w+)\'", c_obj)
        if m is not None:
            c_name = m[1]
            create_functions[sql_name] = c_name
        
    
    return create_functions



EXAMPLE_CRETATE_CAST_EXTENSION = '''
# CITEXT

CREATE CAST (citext AS text)    WITHOUT FUNCTION AS IMPLICIT;
CREATE CAST (citext AS varchar) WITHOUT FUNCTION AS IMPLICIT;
CREATE CAST (citext AS bpchar)  WITHOUT FUNCTION AS ASSIGNMENT;
CREATE CAST (text AS citext)    WITHOUT FUNCTION AS ASSIGNMENT;
CREATE CAST (varchar AS citext) WITHOUT FUNCTION AS ASSIGNMENT;
CREATE CAST (bpchar AS citext)  WITH FUNCTION citext(bpchar)  AS ASSIGNMENT;
CREATE CAST (boolean AS citext) WITH FUNCTION citext(boolean) AS ASSIGNMENT;
CREATE CAST (inet AS citext)    WITH FUNCTION citext(inet)    AS ASSIGNMENT;

CREATE FUNCTION citext(bpchar)
RETURNS citext
AS 'rtrim1'
LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION citext(boolean)
RETURNS citext
AS 'booltext'
LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION citext(inet)
RETURNS citext
AS 'network_show'
LANGUAGE internal IMMUTABLE STRICT;



# HSTORE

CREATE CAST (text[] AS hstore)
  WITH FUNCTION hstore(text[]);

CREATE CAST (hstore AS json)
  WITH FUNCTION hstore_to_json(hstore);

CREATE CAST (hstore AS jsonb)
  WITH FUNCTION hstore_to_jsonb(hstore);

CREATE FUNCTION hstore(text[])
RETURNS hstore
AS 'MODULE_PATHNAME', 'hstore_from_array'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION hstore_to_json(hstore)
RETURNS json
AS 'MODULE_PATHNAME', 'hstore_to_json'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION hstore_to_jsonb(hstore)
RETURNS jsonb
AS 'MODULE_PATHNAME', 'hstore_to_jsonb'
LANGUAGE C IMMUTABLE STRICT;
'''