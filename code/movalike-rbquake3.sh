#!/bin/sh

rm -rf engine/
rm -rf games/
rm -rf shared/
rm -rf libs/

mkdir -p libs
mkdir -p engine
mkdir -p engine/null
mkdir -p games/q3a
mkdir -p shared

git mv jpeg-6 libs/jpeg
#mv AL libs/openal

#mv gameshared shared
git mv botlib shared/botlib

git mv game/bg_public.h shared/
git mv cgame/cg_public.h shared/
git mv game/g_public.h shared/
git mv game/q_math.c shared/
git mv game/q_shared.c shared/
git mv game/q_shared.h shared/
git mv game/surfaceflags.h shared/
git mv ui/keycodes.h shared/

git mv qcommon engine/
git mv client engine/
git mv renderer engine/
git mv raytracing engine/
#mv sound/* engine/client/

git mv server engine/

git mv cgame games/q3a/
git mv game games/q3a/
git mv ui games/q3a/q3_ui
