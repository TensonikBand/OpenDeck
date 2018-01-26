/*
    OpenDeck MIDI platform firmware
    Copyright (C) 2015-2017 Igor Petrovic

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Board.h"

//flash code length -- this is loaded in flash by the linker and other scripts
const uint32_t ProgramLength __attribute__ ((section (".length"))) = 0;

///
/// \brief Location at which size of application is written in flash.
///
#define APP_LENGTH_LOCATION         (uint32_t)0x000000AC

///
/// \brief Location at which compiled binary CRC is written in EEPROM.
///
#define SW_CRC_LOCATION_EEPROM     (EEPROM_SIZE - 2)

Board::Board()
{
    //default constructor
}

void Board::reboot(rebootType_t type)
{
    switch(type)
    {
        case rebootApp:
        eeprom_write_byte((uint8_t*)REBOOT_VALUE_EEPROM_LOCATION, APP_REBOOT_VALUE);
        break;

        case rebootBtldr:
        eeprom_write_byte((uint8_t*)REBOOT_VALUE_EEPROM_LOCATION, BTLDR_REBOOT_VALUE);
        break;
    }

    mcuReset();
}

///
/// \brief Checks if firmware has been updated.
/// Firmware file has written CRC in last two flash addresses. Application stores last read CRC in EEPROM. If EEPROM and flash CRC differ, firmware has been updated.
///
bool Board::checkNewRevision()
{
    //current app crc is written to last flash location
    //previous crc is stored into eeprom
    //if two differ, app has changed

    uint32_t flash_size = pgm_read_dword_far(APP_LENGTH_LOCATION);
    uint16_t crc_eeprom = eeprom_read_word((uint16_t*)SW_CRC_LOCATION_EEPROM);
    uint16_t crc_flash = pgm_read_word_far(flash_size);

    if (crc_eeprom != crc_flash)
    {
        eeprom_update_word((uint16_t*)SW_CRC_LOCATION_EEPROM, crc_flash);
        return true;
    }

    return false;
}

Board board;
