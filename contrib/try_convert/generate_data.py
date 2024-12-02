import random

random.seed(42)

numbers = {
    'int2' : ((-32768, 32767), False),
    'int4' : ((-2147483648, 2147483647), False),
    'int8' : ((-9223372036854775808, 9223372036854775807), False),
    'float4' : ((10**6, 10**6), True),
    'float8' : ((10**15, 10**15), True),
    'numeric' : ((10**20, 10**20), True),
    # 'float8',
    # 'int4',
    # 'float4',
    # 'int2',
    # 'numeric',
}


def save_datafile(t, data):
    filename = f'data/tt_{t}.data'
    file = open(filename, 'w')
    file.write('\n'.join([str(d) for d in data]))

    return filename


for number_type in numbers:

    type_range = numbers[number_type][0]
    is_float = numbers[number_type][1]

    mn = type_range[0]
    mx = type_range[1]

    nln = 0
    ln = len(str(type_range[1]))-1

    table_name = f'tt_{number_type}'

    values = []

    for c in range(nln, ln):
        rep = 20 // ln + 1
        for _ in range(rep):
            n = random.random()
            if c >= 0:
                n *= (10 ** (c+1))
            else:
                n /= (10 ** (-c))
            if not is_float:
                n = int(n)
            
            values += [n]

    filename = save_datafile(number_type, values)
