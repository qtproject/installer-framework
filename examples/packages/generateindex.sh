#!/bin/bash

echo "<Components>"
for i in `find . -maxdepth 1 -type d -not -name \\\.*`; do
    echo "Processing $i..." >&2
    if [ -f "$i/meta/package.xml" ]; then
        echo "<Component>$i</Component>"
    else
	echo "Ignoring $i: Could not find meta/package.xml" >&2
    fi
done
echo "</Components>"

