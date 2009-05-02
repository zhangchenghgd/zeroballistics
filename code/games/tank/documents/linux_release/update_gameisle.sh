#/bin/bash

rsync -av --delete-excluded --exclude 'sounds' --exclude 'textures' --exclude 'particle_effects' --exclude 'gui' --exclude 'shaders' --exclude 'effects' --exclude '*.svn*' --exclude '*.bbm' --exclude '*.dds' --exclude '*.png' --exclude 'fonts' ~/Desktop/data ~/gameisle_tankdir/


strip ~/gameisle_tankdir/{server_ded,autopatcher_server,master_server,ranking_server}

rsync -vazL ~/gameisle_tankdir/ quanticode@gameisle:~/tank

rsync --delete-excluded -vazL ~/gameisle_patchdir_win/ quanticode@gameisle:~/autopatcher_in_win
rsync --delete-excluded -vazL ~/gameisle_patchdir_lin/ quanticode@gameisle:~/autopatcher_in_lin

