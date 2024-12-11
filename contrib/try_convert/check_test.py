import re

regression_path = './regression.diffs'

f = open(regression_path)
lines = f.read().split('\n')

supported_types = [
    'int2',
    'int4',
    'int8',
    'float4',
    'float8',
    'numeric',

    'date',
    'time',
    'timestamp',
    'timetz',
    'timestamptz'
]

for i in range(1, len(lines)):
    line = lines[i]
    if len(line) > 0 and len(lines[i-1]) > 0 and line[0] == '-' and lines[i-1][0] != '-':
        words = re.split('::|\*|;|\n| |\(|\)|,|\.|\".*\"|\'.*\'|<.*>', lines[i-1])
        ans = []
        is_prining = False
        for word in words:
            if word not in ['select', 'from', 
                            'try_convert', 'try_convert_by_sql', 'try_convert_by_sql_text', 'try_convert_by_sql_with_len_out', 
                            'NULL', 'v', 'v1', 'v2', 'where', 'is', 'not', 'distinct', 'as', 't', '']:
                ans += [word]
                for w in word.split('_'):
                    if w in supported_types:
                        is_prining = True
        if is_prining:
            print(' '.join(ans))
        