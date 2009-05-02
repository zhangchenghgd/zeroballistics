
find zb_beta_v11/ -type f -exec chmod -v 644 {} \;
find zb_beta_v11/ -name "*so" -exec strip {} \;

strip zb_beta_v11/tank.x86

chmod +x zb_beta_v11/tank.x86 zb_beta_v11/start_client.sh

