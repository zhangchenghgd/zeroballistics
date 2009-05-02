


nice -n 19 mencoder mf://$1*.jpg -mf fps=20:type=jpg -ovc lavc -lavcopts vcodec=mpeg4:mbd=2:mv0:trell:v4mv:cbp:last_pred=3:predia=2:dia=2:vmax_b_frames=2:vb_strategy=1:precmp=2:cmp=2:subcmp=2:preme=2:vme=5:naq:qns=2:vbitrate=2000 -oac copy -o $1.avi