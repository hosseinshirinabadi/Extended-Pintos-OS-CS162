# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_USER_FAULTS => 1, [<<'EOF']);
(CacheTest1) begin
(CacheTest1) create "xyz"
(CacheTest1) open "xyz"
(CacheTest1) close after writing
(CacheTest1) cache reset
(CacheTest1) open "xyz"
(CacheTest1) read for the first time
(CacheTest1) close "xyz" after first read
(CacheTest1) open "xyz"
(CacheTest1) read for the second time
(CacheTest1) close "xyz" after second read
(CacheTest1) Second read has higher hit rate than the first time
(CacheTest1) end
CacheTest1: exit(0)
EOF
pass;