#!/bin/bash

secrets_file="`pwd`/secrets.h"

for var in "${!SONIC_@}"; do
    echo "#define $var \"${!var}"\" >> $secrets_file
done
