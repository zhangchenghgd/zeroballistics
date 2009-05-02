
find zb_beta_v10/ -type f -exec chmod -v 644 {} \;
find zb_beta_v10/ -name "*so" -exec strip {} \;

strip zb_beta_v10/tank.x86

chmod +x zb_beta_v10/tank.x86 zb_beta_v10/start_client.sh

