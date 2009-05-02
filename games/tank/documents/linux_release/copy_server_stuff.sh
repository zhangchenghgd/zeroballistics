#/bin/bash

rsync -av --delete-excluded --exclude 'sounds' --exclude 'textures' --exclude 'particle_effects' --exclude 'gui' --exclude 'shaders' --exclude 'effects' --exclude '*.svn*' --exclude '*.bbm' --exclude '*.dds' --exclude '*.png' --exclude 'fonts' ~/Desktop/data $1
