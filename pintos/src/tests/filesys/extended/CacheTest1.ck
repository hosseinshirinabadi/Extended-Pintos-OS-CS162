# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_USER_FAULTS => 1, [<<'EOF']);
(CacheTest1) begin
(CacheTest1) create "a"
(CacheTest1) open "a"
(CacheTest1) close "after writing"
(CacheTest1) cache reset
(CacheTest1) open "a"
(CacheTest1) read first time
(CacheTest1) close "a"
(CacheTest1) open "a"
(CacheTest1) read "a"
(CacheTest1) close "a"
(CacheTest1) Second reading has higher hit rate than the first time
(CacheTest1) end
CacheTest1: exit(0)
EOF
pass;