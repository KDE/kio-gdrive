#!/bin/sh
$XGETTEXT `find . -not -path \*/tests/\* -name \*.cpp -o -name \*.qml -o -name \*.cc -o -name \*.h` -o $podir/kio5_gdrive.pot
