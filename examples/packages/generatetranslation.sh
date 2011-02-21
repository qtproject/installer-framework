#!/bin/sh

if [ $# -lt 1 ]; then
    echo "Usage: $0 scriptfile [uifiles]*"  >/dev/stderr
    exit 1
fi

FILE="$TMPDIR/$(basename $0).$RANDOM.$$.tmp"
FILE2="$TMPDIR/$(basename $0).$RANDOM.$$.tmp"
FILE3="$TMPDIR/$(basename $0).$RANDOM.$$.tmp"

CONTEXT=`basename $1 | sed -e 's,\([^.]*\)\..*,\1,'`

# first work on the script file
sed -ne 's/qsTr *( *"\(\([^"\\]*\(\\.\)*\)*\)"/\
TRANSLATE\1TRANSLATE\
/pg' $1 | sed -ne 's,^TRANSLATE\(.*\)TRANSLATE$,\1,p' | 
sed -e 's/\\"/";/g' | 
sed -e 's/</\&lt;/g' > $FILE

# remove duplicates
sort -u $FILE > $FILE3
mv $FILE3 $FILE

echo "<?xml version=\"1.0\" encoding=\"utf8\"?>" > $FILE2
echo "<!DOCTYPE TS><TS version=\"1.1\">" >> $FILE2
echo "<context>" >> $FILE2
echo "    <name>$CONTEXT</name>" >> $FILE2

sed -e 's/\(.*\)/    <message>\
        <source>\1<\/source>\
        <translation type="unfinished"><\/translation>\
    <\/message>/' < $FILE >> $FILE2

echo "</context>" >> $FILE2


while [ $# -gt 1 ]; do
shift

CONTEXT=`sed -ne 's,.*<class>\([^<]*\)</class>.*,\1,p' $1`

echo "<context>" >> $FILE2
echo "    <name>$CONTEXT</name>" >> $FILE2

# then work on the ui files
sed -ne 's/<string>\([^<\\]*\)<\/string>/\
TRANSLATE\1TRANSLATE\
/pg' $1 | sed -ne 's,^TRANSLATE\(.*\)TRANSLATE$,\1,p' | 
sed -e 's/\\"/";/g' | 
sed -e 's/</\&lt;/g' > $FILE

# remove duplicates
sort -u $FILE > $FILE3
mv $FILE3 $FILE



sed -e 's/\(.*\)/    <message>\
        <source>\1<\/source>\
        <translation type="unfinished"><\/translation>\
    <\/message>/' < $FILE >> $FILE2


echo "</context>" >> $FILE2

done

echo "</TS>" >> $FILE2

cat $FILE2

rm $FILE
rm $FILE2

exit 0
