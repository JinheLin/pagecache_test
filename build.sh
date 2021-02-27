#!/bin/bash

g++ -std=c++11 -o pagecache_write pagecache_write.cc

g++ -std=c++11 -o readahead_random readahead_random.cc

g++ -std=c++11 -o readahead_sequential readahead_sequential.cc