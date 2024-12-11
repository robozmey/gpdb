-- LOAD DATA
CREATE TABLE tt_int2 (v int2) DISTRIBUTED BY (v);
COPY tt_int2 from '@abs_srcdir@/data/tt_int2.data';
CREATE TABLE tt_int4 (v int4) DISTRIBUTED BY (v);
COPY tt_int4 from '@abs_srcdir@/data/tt_int4.data';
CREATE TABLE tt_int8 (v int8) DISTRIBUTED BY (v);
COPY tt_int8 from '@abs_srcdir@/data/tt_int8.data';
CREATE TABLE tt_float4 (v float4) DISTRIBUTED BY (v);
COPY tt_float4 from '@abs_srcdir@/data/tt_float4.data';
CREATE TABLE tt_float8 (v float8) DISTRIBUTED BY (v);
COPY tt_float8 from '@abs_srcdir@/data/tt_float8.data';
CREATE TABLE tt_numeric (v numeric) DISTRIBUTED BY (v);
COPY tt_numeric from '@abs_srcdir@/data/tt_numeric.data';
