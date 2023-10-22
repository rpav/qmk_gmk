#!/usr/bin/env bash
mv -f ~/Downloads/preonic_rev3_drop_layout_preonic_1x2uc_mine.json keyboards/preonic/keymaps/rpav/keymap.json

qmk json2c /home/rpav/preonic_rpav_4.json -o keyboards/preonic/keymaps/rpav/keymap_generated.h &&
    qmk compile -kb preonic/rev3_drop -km rpav &&
    sudo qmk flash -kb preonic/rev3_drop -km rpav
