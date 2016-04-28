#!/bin/sh

for x in *.cc ; do clang-format -style=Mozilla $x > tidycode ; mv tidycode $x ; done
for x in *.h ; do clang-format -style=Mozilla $x > tidycode ; mv tidycode $x ; done
