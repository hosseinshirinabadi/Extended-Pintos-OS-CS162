# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_USER_FAULTS => 1, [<<'EOF']);
(CacheTest2) begin
(CacheTest2) create "xyz"
(CacheTest2) open "xyz" for writing
(CacheTest2) writing 64kB to "xyz" byte-by-byte
(CacheTest2) close "xyz" after writing
(CacheTest2) open "xyz" for read
(CacheTest2) close "xyz" after first read
(CacheTest2) total number of device writes is on the order of 128
(CacheTest2) end
CacheTest2: exit(0)
EOF
pass;