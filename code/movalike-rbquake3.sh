#!/bin/sh

rm -rf engine/
rm -rf games/
rm -rf shared/
rm -rf libs/

mkdir -p libs
mkdir -p engine
mkdir -p engine/null
mkdir -p games/q3a

mv jpeg-6 libs/jpeg
mv AL libs/openal

mv gameshared shared

mv qcommon engine/
mv client engine/
mv renderer engine/
mv raytracing engine/
#mv sound/* engine/client/

mv server engine/

mv cgame games/q3a/
mv game games/q3a/
mv ui games/q3a/