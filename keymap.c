/* Copyright 2015-2021 Jack Humbert
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include QMK_KEYBOARD_H
#include "muse.h"
#include "raw_hid.h"

enum preonic_layers {
  _QWERTY,
  _LOWER,
  _RAISE,
  _ADJUST,
  _GKEY
};

enum preonic_keycodes {
  QWERTY = SAFE_RANGE,
  LOWER,
  RAISE,
  BACKLIT,
  GKMT
};

#include "keymap_generated.h"

bool gkey_mode = false;

/*** Send messages ***/
typedef enum {
    GKEY_MSG_TYPE_KEYSTATE = 0,
} gkey_msg_send_header_type_t;

typedef struct {
    uint8_t type;
} gkey_msg_header_t;

typedef struct {
    gkey_msg_header_t header;

    uint8_t col;
    uint8_t row;

    uint8_t bits;
} gkey_msg_keystate_t;

typedef enum {
    GKEY_BITS_PRESSED = 0,
} gkey_msg_keystate_bits_t;


/*** Recv messages ***/
typedef enum {
    GKEY_MSG_TYPE_GKMS,  // GKey Mode Set
} gkey_msg_recv_header_type_t;

typedef struct {
    gkey_msg_header_t header;

    uint8_t state;
} gkey_msg_gkms_t;

typedef enum {
    GKEY_GKMS_OFF,
    GKEY_GKMS_ON,
    GKEY_GKMS_TOGGLE,
} gkey_msg_gkms_state_t;

/*** Packet ***/
typedef union {
    gkey_msg_header_t header;

    /* Send */
    gkey_msg_keystate_t keystate;

    /* Recv */
    gkey_msg_gkms_t gkms;
} gkey_msg_t;

typedef struct {
    gkey_msg_t msg;

    uint8_t padding[RAW_EPSIZE - sizeof(gkey_msg_t)];
} gkey_packet_t;


/*** Handle ***/
void raw_hid_receive(uint8_t *data, uint8_t length) {
    if(length < 1) return;

    gkey_packet_t* pkt = (gkey_packet_t*)data;

    switch(pkt->msg.header.type) {
        case GKEY_MSG_TYPE_GKMS:
            gkey_mode = (pkt->msg.gkms.state == GKEY_GKMS_TOGGLE) ? !gkey_mode : (pkt->msg.gkms.state == GKEY_GKMS_ON);
            break;
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (gkey_mode) {
        gkey_packet_t pkt;
        pkt.msg.header.type = GKEY_MSG_TYPE_KEYSTATE;
        pkt.msg.keystate.col = record->event.key.col;
        pkt.msg.keystate.row = record->event.key.row;

        pkt.msg.keystate.bits = (record->event.pressed << GKEY_BITS_PRESSED)
            // | ...
            ;

        raw_hid_send((uint8_t *)&pkt, sizeof(pkt));
        return false;
    }

    switch (keycode) {
        case QWERTY:
            if (record->event.pressed) {
                set_single_persistent_default_layer(_QWERTY);
            }
            return false;
            break;
        case LOWER:
            if (record->event.pressed) {
                layer_on(_LOWER);
                update_tri_layer(_LOWER, _RAISE, _ADJUST);
            } else {
                layer_off(_LOWER);
                update_tri_layer(_LOWER, _RAISE, _ADJUST);
            }
            return false;
            break;
        case RAISE:
            if (record->event.pressed) {
                layer_on(_RAISE);
                update_tri_layer(_LOWER, _RAISE, _ADJUST);
            } else {
                layer_off(_RAISE);
                update_tri_layer(_LOWER, _RAISE, _ADJUST);
            }
            return false;
            break;
        case BACKLIT:
            if (record->event.pressed) {
                register_code(KC_RSFT);
#ifdef BACKLIGHT_ENABLE
                backlight_step();
#endif
#ifdef RGBLIGHT_ENABLE
                rgblight_step();
#endif
#ifdef __AVR__
                writePinLow(E6);
#endif
            } else {
                unregister_code(KC_RSFT);
#ifdef __AVR__
                writePinHigh(E6);
#endif
            }
            return false;
            break;

        case GKMT:
            if (record->event.pressed) gkey_mode = !gkey_mode;

            return false;
            break;
     }
    return true;
};

bool muse_mode = false;
uint8_t last_muse_note = 0;
uint16_t muse_counter = 0;
uint8_t muse_offset = 70;
uint16_t muse_tempo = 50;

bool encoder_update_user(uint8_t index, bool clockwise) {
  if (muse_mode) {
    if (IS_LAYER_ON(_RAISE)) {
      if (clockwise) {
        muse_offset++;
      } else {
        muse_offset--;
      }
    } else {
      if (clockwise) {
        muse_tempo+=1;
      } else {
        muse_tempo-=1;
      }
    }
  } else {
    if (clockwise) {
      register_code(KC_PGDN);
      unregister_code(KC_PGDN);
    } else {
      register_code(KC_PGUP);
      unregister_code(KC_PGUP);
    }
  }
    return true;
}

bool dip_switch_update_user(uint8_t index, bool active) {
    switch (index) {
        case 0:
            if (active) {
                layer_on(_ADJUST);
            } else {
                layer_off(_ADJUST);
            }
            break;
        case 1:
            if (active) {
                muse_mode = true;
            } else {
                muse_mode = false;
            }
    }
    return true;
}


void matrix_scan_user(void) {
#ifdef AUDIO_ENABLE
    if (muse_mode) {
        if (muse_counter == 0) {
            uint8_t muse_note = muse_offset + SCALE[muse_clock_pulse()];
            if (muse_note != last_muse_note) {
                stop_note(compute_freq_for_midi_note(last_muse_note));
                play_note(compute_freq_for_midi_note(muse_note), 0xF);
                last_muse_note = muse_note;
            }
        }
        muse_counter = (muse_counter + 1) % muse_tempo;
    } else {
        if (muse_counter) {
            stop_all_notes();
            muse_counter = 0;
        }
    }
#endif
}

bool music_mask_user(uint16_t keycode) {
  switch (keycode) {
    case RAISE:
    case LOWER:
      return false;
    default:
      return true;
  }
}
