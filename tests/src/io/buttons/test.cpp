#include "unity/src/unity.h"
#include "unity/Helpers.h"
#include "io/buttons/Buttons.h"
#include "io/leds/LEDs.h"
#include "io/common/CInfo.h"
#include "midi/src/MIDI.h"
#include "core/src/general/Timing.h"
#include "database/Database.h"
#include "stubs/database/DB_ReadWrite.h"

#ifdef BUTTONS_SUPPORTED

namespace
{
    bool buttonState[MAX_NUMBER_OF_BUTTONS] = {};

    class DBhandlers : public Database::Handlers
    {
        public:
        DBhandlers() {}

        void presetChange(uint8_t preset) override
        {
            if (presetChangeHandler != nullptr)
                presetChangeHandler(preset);
        }

        void factoryResetStart() override
        {
            if (factoryResetStartHandler != nullptr)
                factoryResetStartHandler();
        }

        void factoryResetDone() override
        {
            if (factoryResetDoneHandler != nullptr)
                factoryResetDoneHandler();
        }

        void initialized() override
        {
            if (initHandler != nullptr)
                initHandler();
        }

        //actions which these handlers should take depend on objects making
        //up the entire system to be initialized
        //therefore in interface we are calling these function pointers which
        // are set in application once we have all objects ready
        void (*presetChangeHandler)(uint8_t preset) = nullptr;
        void (*factoryResetStartHandler)()          = nullptr;
        void (*factoryResetDoneHandler)()           = nullptr;
        void (*initHandler)()                       = nullptr;
    } dbHandlers;

    class HWAMIDI : public MIDI::HWA
    {
        public:
        HWAMIDI() = default;

        bool init() override
        {
            return true;
        }

        bool dinRead(uint8_t& data) override
        {
            return false;
        }

        bool dinWrite(uint8_t data) override
        {
            return false;
        }

        bool usbRead(MIDI::USBMIDIpacket_t& USBMIDIpacket) override
        {
            return false;
        }

        bool usbWrite(MIDI::USBMIDIpacket_t& USBMIDIpacket) override
        {
            midiPacket.push_back(USBMIDIpacket);
            return true;
        }

        std::vector<MIDI::USBMIDIpacket_t> midiPacket;
    } hwaMIDI;

    class HWALEDs : public IO::LEDs::HWA
    {
        public:
        HWALEDs() {}

        void setState(size_t index, bool state) override
        {
        }

        size_t rgbSingleComponentIndex(size_t rgbIndex, IO::LEDs::rgbIndex_t rgbComponent) override
        {
            return 0;
        }

        size_t rgbIndex(size_t singleLEDindex) override
        {
            return 0;
        }

        void setFadeSpeed(size_t transitionSpeed) override
        {
        }
    } hwaLEDs;

    class HWAButtons : public IO::Buttons::HWA
    {
        public:
        HWAButtons() {}

        bool state(size_t index) override
        {
            return buttonState[index];
        }
    } hwaButtons;

    class ButtonsFilter : public IO::Buttons::Filter
    {
        public:
        bool isFiltered(size_t index, bool value, bool& filteredValue) override
        {
            return true;
        }

        void reset(size_t index) override
        {
        }
    } buttonsFilter;

    DBstorageMock dbStorageMock;
    Database      database = Database(dbHandlers, dbStorageMock, true);
    MIDI          midi(hwaMIDI);
    ComponentInfo cInfo;

    IO::LEDs leds(hwaLEDs, database);

    class HWAU8X8 : public IO::U8X8::HWAI2C
    {
        public:
        HWAU8X8() {}

        bool init() override
        {
            return true;
        }

        bool deInit() override
        {
            return true;
        }

        bool write(uint8_t address, uint8_t* data, size_t size) override
        {
            return true;
        }
    } hwaU8X8;

    IO::U8X8    u8x8(hwaU8X8);
    IO::Display display(u8x8, database);
    IO::Buttons buttons = IO::Buttons(hwaButtons, buttonsFilter, database, midi, leds, display, cInfo);

    void stateChangeRegister(bool state)
    {
        hwaMIDI.midiPacket.clear();

        for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
            buttons.processButton(i, state);
    }
}    // namespace

TEST_SETUP()
{
    //init checks - no point in running further tests if these conditions fail
    TEST_ASSERT(database.init() == true);
    //always start from known state
    database.factoryReset();

    midi.init();
    midi.setChannelSendZeroStart(true);
    midi.enableUSBMIDI();
}

TEST_CASE(Note)
{
    using namespace IO;

    auto test = [&](uint8_t channel, uint8_t velocityValue) {
        //set known state
        for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
        {
            //configure all buttons as momentary
            TEST_ASSERT(database.update(Database::Section::button_t::type, i, static_cast<int32_t>(Buttons::type_t::momentary)) == true);

            //make all buttons send note messages
            TEST_ASSERT(database.update(Database::Section::button_t::midiMessage, i, static_cast<int32_t>(Buttons::messageType_t::note)) == true);

            //set passed velocity value
            TEST_ASSERT(database.update(Database::Section::button_t::velocity, i, velocityValue) == true);

            database.update(Database::Section::button_t::midiChannel, i, channel);
            buttons.reset(i);
        }

        auto verifyValue = [&](bool state) {
            // verify all received messages
            for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
            {
                uint8_t midiMessage = hwaMIDI.midiPacket.at(i).Event << 4;
                TEST_ASSERT(midiMessage == (state ? static_cast<uint8_t>(MIDI::messageType_t::noteOn)
                                                  : midi.getNoteOffMode() == MIDI::noteOffType_t::standardNoteOff ? static_cast<uint8_t>(MIDI::messageType_t::noteOff)
                                                                                                                  : static_cast<uint8_t>(MIDI::messageType_t::noteOn)));

                TEST_ASSERT(hwaMIDI.midiPacket.at(i).Data3 == (state ? velocityValue : 0));
                TEST_ASSERT(channel == midi.getChannelFromStatusByte(hwaMIDI.midiPacket.at(i).Data1));

                //also verify MIDI ID
                //it should be equal to button ID by default
                TEST_ASSERT(i == hwaMIDI.midiPacket.at(i).Data2);
            }
        };

        //simulate button press
        stateChangeRegister(true);
        TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);
        verifyValue(true);

        //simulate button release
        stateChangeRegister(false);
        TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);
        verifyValue(false);

        // try with the latching mode
        for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
            TEST_ASSERT(database.update(Database::Section::button_t::type, i, static_cast<int32_t>(Buttons::type_t::latching)) == true);

        stateChangeRegister(true);
        TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);
        verifyValue(true);

        //nothing should happen on release
        stateChangeRegister(false);
        TEST_ASSERT(hwaMIDI.midiPacket.size() == 0);

        //press again, new messages should arrive
        stateChangeRegister(true);
        verifyValue(false);
    };

    //test for all velocity values
    //test only for channel 0 for simplicity
    for (int j = 1; j < 128; j++)
    {
        midi.setNoteOffMode(MIDI::noteOffType_t::standardNoteOff);
        test(0, j);

        //repeat the test with note off being sent as note on with velocity 0
        midi.setNoteOffMode(MIDI::noteOffType_t::noteOnZeroVel);
        test(0, j);
    }
}

TEST_CASE(ProgramChange)
{
    using namespace IO;

    auto testProgramChange = [&](uint8_t channel) {
        //set known state
        for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
        {
            //configure all buttons as momentary
            TEST_ASSERT(database.update(Database::Section::button_t::type, i, static_cast<int32_t>(Buttons::type_t::momentary)) == true);

            //make all buttons send program change messages
            TEST_ASSERT(database.update(Database::Section::button_t::midiMessage, i, static_cast<int32_t>(Buttons::messageType_t::programChange)) == true);

            //configure the specifed midi channel
            TEST_ASSERT(database.update(Database::Section::button_t::midiChannel, i, channel) == true);

            buttons.reset(i);
        }

        auto verifyMessage = [&]() {
            // verify all received messages are program change
            for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
            {
                uint8_t midiMessage = hwaMIDI.midiPacket.at(i).Event << 4;
                TEST_ASSERT(midiMessage == static_cast<uint8_t>(MIDI::messageType_t::programChange));

                //byte 3 on program change should be 0
                TEST_ASSERT(0 == hwaMIDI.midiPacket.at(i).Data3);

                //verify channel
                TEST_ASSERT(channel == midi.getChannelFromStatusByte(hwaMIDI.midiPacket.at(i).Data1));

                //also verify MIDI ID/program
                //it should be equal to button ID by default
                TEST_ASSERT(i == hwaMIDI.midiPacket.at(i).Data2);
            }
        };

        //simulate button press
        stateChangeRegister(true);
        TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);
        verifyMessage();

        //program change shouldn't be sent on release
        stateChangeRegister(false);
        TEST_ASSERT(hwaMIDI.midiPacket.size() == 0);

        //repeat the entire test again, but with buttons configured as latching types
        //behaviour should be the same
        for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
            TEST_ASSERT(database.update(Database::Section::button_t::type, i, static_cast<int32_t>(Buttons::type_t::latching)) == true);

        stateChangeRegister(true);
        TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);
        verifyMessage();

        stateChangeRegister(false);
        TEST_ASSERT(hwaMIDI.midiPacket.size() == 0);
    };

    //test for all channels
    for (int i = 0; i < 16; i++)
        testProgramChange(i);

    //test programChangeInc/programChangeDec
    //revert to default database state first
    database.factoryReset();
    stateChangeRegister(false);

    auto configurePCbutton = [&](uint8_t buttonID, uint8_t channel, bool increase) {
        TEST_ASSERT(database.update(Database::Section::button_t::type, buttonID, static_cast<int32_t>(Buttons::type_t::momentary)) == true);
        TEST_ASSERT(database.update(Database::Section::button_t::midiMessage, buttonID, increase ? static_cast<int32_t>(Buttons::messageType_t::programChangeInc) : static_cast<int32_t>(Buttons::messageType_t::programChangeDec)) == true);
        TEST_ASSERT(database.update(Database::Section::button_t::midiChannel, buttonID, channel) == true);

        for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
            buttons.reset(i);
    };

    auto verifyProgramChange = [&](uint8_t buttonID, uint8_t channel, uint8_t program) {
        uint8_t midiMessage = hwaMIDI.midiPacket[buttonID].Event << 4;
        TEST_ASSERT(midiMessage == static_cast<uint8_t>(MIDI::messageType_t::programChange));

        //byte 3 on program change should be 0
        TEST_ASSERT(0 == hwaMIDI.midiPacket[buttonID].Data3);

        //verify channel
        TEST_ASSERT(channel == midi.getChannelFromStatusByte(hwaMIDI.midiPacket[buttonID].Data1));

        TEST_ASSERT(program == hwaMIDI.midiPacket[buttonID].Data2);
    };

    configurePCbutton(0, 0, true);

    //verify that the received program change was 1 for button 0
    stateChangeRegister(true);
    verifyProgramChange(0, 0, 1);
    stateChangeRegister(false);

    //after this, verify that the received program change was 2 for button 0
    stateChangeRegister(true);
    verifyProgramChange(0, 0, 2);
    stateChangeRegister(false);

    //after this, verify that the received program change was 3 for button 0
    stateChangeRegister(true);
    verifyProgramChange(0, 0, 3);
    stateChangeRegister(false);

    //now, revert all buttons back to default
    database.factoryReset();

    //configure some other button to programChangeInc
    configurePCbutton(4, 0, true);

    //verify that the program change is continuing to increase
    stateChangeRegister(true);
    verifyProgramChange(4, 0, 4);
    stateChangeRegister(false);

    stateChangeRegister(true);
    verifyProgramChange(4, 0, 5);
    stateChangeRegister(false);

    //now configure two buttons to send program change/inc
    configurePCbutton(0, 0, true);

    stateChangeRegister(true);
    //program change should be increased by 1, first by button 0
    verifyProgramChange(0, 0, 6);
    //then by button 4
    verifyProgramChange(4, 0, 7);
    stateChangeRegister(false);

    //configure another button to programChangeInc, but on other channel
    configurePCbutton(1, 4, true);

    stateChangeRegister(true);
    //program change should be increased by 1, first by button 0
    verifyProgramChange(0, 0, 8);
    //then by button 4
    verifyProgramChange(4, 0, 9);
    //program change should be sent on channel 4 by button 1
    verifyProgramChange(1, 4, 1);
    stateChangeRegister(false);

    //revert to default again
    database.factoryReset();

    //now configure button 0 for programChangeDec
    configurePCbutton(0, 0, false);

    stateChangeRegister(true);
    //program change should decrease by 1
    verifyProgramChange(0, 0, 8);
    stateChangeRegister(false);

    stateChangeRegister(true);
    //program change should decrease by 1 again
    verifyProgramChange(0, 0, 7);
    stateChangeRegister(false);

    //configure another button for programChangeDec
    configurePCbutton(1, 0, false);

    stateChangeRegister(true);
    //button 0 should decrease the value by 1
    verifyProgramChange(0, 0, 6);
    //button 1 should decrease it again
    verifyProgramChange(1, 0, 5);
    stateChangeRegister(false);

    //configure another button for programChangeDec
    configurePCbutton(2, 0, false);

    stateChangeRegister(true);
    //button 0 should decrease the value by 1
    verifyProgramChange(0, 0, 4);
    //button 1 should decrease it again
    verifyProgramChange(1, 0, 3);
    //button 2 should decrease it again
    verifyProgramChange(2, 0, 2);
    stateChangeRegister(false);

    //reset all received messages first
    hwaMIDI.midiPacket.clear();

    //only two program change messages should be sent
    //program change value is 0 after the second button decreases it
    //once the value is 0 no further messages should be sent in dec mode

    stateChangeRegister(true);
    //button 0 should decrease the value by 1
    verifyProgramChange(0, 0, 1);
    //button 1 should decrease it again
    verifyProgramChange(1, 0, 0);

    //verify that only two program change messages have been received
    uint8_t pcCounter = 0;

    for (int i = 0; i < hwaMIDI.midiPacket.size(); i++)
    {
        //no program change messages should be sent
        uint8_t midiMessage = hwaMIDI.midiPacket.at(i).Event << 4;

        if (midiMessage == static_cast<uint8_t>(MIDI::messageType_t::programChange))
            pcCounter++;
    }

    TEST_ASSERT(pcCounter == 2);

    stateChangeRegister(false);

    //revert all buttons to default
    database.factoryReset();

    configurePCbutton(0, 0, true);

    stateChangeRegister(true);
    //button 0 should increase the last value by 1
    verifyProgramChange(0, 0, 1);
    stateChangeRegister(false);
}

TEST_CASE(ControlChange)
{
    using namespace IO;

    auto controlChangeTest = [&](uint8_t controlValue) {
        //set known state
        for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
        {
            //configure all buttons as momentary
            TEST_ASSERT(database.update(Database::Section::button_t::type, i, static_cast<int32_t>(Buttons::type_t::momentary)) == true);

            //make all buttons send control change messages
            TEST_ASSERT(database.update(Database::Section::button_t::midiMessage, i, static_cast<int32_t>(Buttons::messageType_t::controlChange)) == true);

            //set passed control value
            TEST_ASSERT(database.update(Database::Section::button_t::velocity, i, controlValue) == true);

            buttons.reset(i);
        }

        auto verifyMessage = [&](uint8_t midiValue) {
            // verify all received messages are control change
            for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
            {
                uint8_t midiMessage = hwaMIDI.midiPacket.at(i).Event << 4;
                TEST_ASSERT(midiMessage == static_cast<uint8_t>(MIDI::messageType_t::controlChange));

                TEST_ASSERT(midiValue == hwaMIDI.midiPacket.at(i).Data3);

                //verify channel
                TEST_ASSERT(0 == midi.getChannelFromStatusByte(hwaMIDI.midiPacket.at(i).Data1));

                //also verify MIDI ID
                //it should be equal to button ID by default
                TEST_ASSERT(i == hwaMIDI.midiPacket.at(i).Data2);
            }
        };

        //simulate button press
        stateChangeRegister(true);
        TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);

        verifyMessage(controlValue);

        //no messages should be sent on release
        stateChangeRegister(false);
        TEST_ASSERT(hwaMIDI.midiPacket.size() == 0);

        //change to latching type
        //behaviour should be the same

        for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
            TEST_ASSERT(database.update(Database::Section::button_t::type, i, static_cast<int32_t>(Buttons::type_t::latching)) == true);

        stateChangeRegister(true);
        TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);

        verifyMessage(controlValue);

        stateChangeRegister(false);
        TEST_ASSERT(hwaMIDI.midiPacket.size() == 0);

        // change type to control change with 0 on reset, momentary mode
        // this means CC value 0 should be sent on release

        for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
            TEST_ASSERT(database.update(Database::Section::button_t::type, i, static_cast<int32_t>(Buttons::type_t::momentary)) == true);

        for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
            TEST_ASSERT(database.update(Database::Section::button_t::midiMessage, i, static_cast<int32_t>(Buttons::messageType_t::controlChangeReset)) == true);

        stateChangeRegister(true);
        TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);

        verifyMessage(controlValue);

        stateChangeRegister(false);
        TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);

        verifyMessage(0);

        //same test again, but in latching mode
        //now, on press, messages should be sent
        //on release, nothing should happen
        //on second press reset should be sent (CC with value 0)

        for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
            TEST_ASSERT(database.update(Database::Section::button_t::type, i, static_cast<int32_t>(Buttons::type_t::latching)) == true);

        stateChangeRegister(true);
        TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);

        verifyMessage(controlValue);

        stateChangeRegister(false);
        TEST_ASSERT(hwaMIDI.midiPacket.size() == 0);

        stateChangeRegister(true);
        TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);

        verifyMessage(0);
    };

    //verify with all control values
    //value 0 is normally blocked to configure for users (via sysex)
    for (int i = 1; i < 128; i++)
        controlChangeTest(i);
}

TEST_CASE(NoMessages)
{
    using namespace IO;

    //configure all buttons to messageType_t::none so that midi messages aren't sent

    for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
    {
        //configure all buttons as momentary
        TEST_ASSERT(database.update(Database::Section::button_t::type, i, static_cast<int32_t>(Buttons::type_t::momentary)) == true);

        //don't send any message
        TEST_ASSERT(database.update(Database::Section::button_t::midiMessage, i, static_cast<int32_t>(Buttons::messageType_t::none)) == true);

        buttons.reset(i);
    }

    stateChangeRegister(true);
    TEST_ASSERT(hwaMIDI.midiPacket.size() == 0);

    stateChangeRegister(false);
    TEST_ASSERT(hwaMIDI.midiPacket.size() == 0);

    for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
        TEST_ASSERT(database.update(Database::Section::button_t::type, i, static_cast<int32_t>(Buttons::type_t::latching)) == true);

    stateChangeRegister(true);
    TEST_ASSERT(hwaMIDI.midiPacket.size() == 0);

    stateChangeRegister(false);
    TEST_ASSERT(hwaMIDI.midiPacket.size() == 0);

    stateChangeRegister(true);
    TEST_ASSERT(hwaMIDI.midiPacket.size() == 0);

    stateChangeRegister(false);
    TEST_ASSERT(hwaMIDI.midiPacket.size() == 0);
}

#if MAX_NUMBER_OF_LEDS > 0
TEST_CASE(LocalLEDcontrol)
{
    using namespace IO;
    using namespace IO;

    for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
    {
        //configure all buttons as momentary
        TEST_ASSERT(database.update(Database::Section::button_t::type, i, static_cast<int32_t>(Buttons::type_t::momentary)) == true);

        //send note messages
        TEST_ASSERT(database.update(Database::Section::button_t::midiMessage, i, static_cast<int32_t>(Buttons::messageType_t::note)) == true);

        TEST_ASSERT(database.update(Database::Section::button_t::velocity, i, 127) == true);

        buttons.reset(i);
    }

    //configure one of the leds in local control mode
    TEST_ASSERT(database.update(Database::Section::leds_t::controlType, 0, static_cast<int32_t>(LEDs::controlType_t::localNoteForStateNoBlink)) == true);
    //set 127 as activation value, 0 as activation ID
    TEST_ASSERT(database.update(Database::Section::leds_t::activationValue, 0, 127) == true);
    TEST_ASSERT(database.update(Database::Section::leds_t::activationID, 0, 0) == true);

    //all leds should be off initially
    for (int i = 0; i < MAX_NUMBER_OF_LEDS; i++)
        TEST_ASSERT(leds.getColor(i) == LEDs::color_t::off);

    //simulate the press of all buttons
    //since led 0 is configured in local control mode, it should be on now
    stateChangeRegister(true);
    TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);

    TEST_ASSERT(leds.getColor(0) != LEDs::color_t::off);

    //all other leds should remain off
    for (int i = 1; i < MAX_NUMBER_OF_LEDS; i++)
        TEST_ASSERT(leds.getColor(i) == LEDs::color_t::off);

    //now release the button and verify that the led is off again
    stateChangeRegister(false);
    TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);

    for (int i = 0; i < MAX_NUMBER_OF_LEDS; i++)
        TEST_ASSERT(leds.getColor(i) == LEDs::color_t::off);

    //test again in latching mode
    for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
        TEST_ASSERT(database.update(Database::Section::button_t::type, i, static_cast<int32_t>(Buttons::type_t::latching)) == true);

    stateChangeRegister(true);
    TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);

    TEST_ASSERT(leds.getColor(0) != LEDs::color_t::off);

    stateChangeRegister(false);
    TEST_ASSERT(hwaMIDI.midiPacket.size() == 0);

    //led should remain on
    TEST_ASSERT(leds.getColor(0) != LEDs::color_t::off);

    stateChangeRegister(true);
    TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);

    TEST_ASSERT(leds.getColor(0) == LEDs::color_t::off);

    //test again in control change mode
    for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
    {
        //configure all buttons as momentary
        TEST_ASSERT(database.update(Database::Section::button_t::type, i, static_cast<int32_t>(Buttons::type_t::momentary)) == true);

        //send cc messages
        TEST_ASSERT(database.update(Database::Section::button_t::midiMessage, i, static_cast<int32_t>(Buttons::messageType_t::controlChange)) == true);

        buttons.reset(i);
    }

    stateChangeRegister(true);
    TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);

    //led should be off since it's configure to react on note messages and not on control change
    TEST_ASSERT(leds.getColor(0) == LEDs::color_t::off);

    TEST_ASSERT(database.update(Database::Section::leds_t::controlType, 0, static_cast<int32_t>(LEDs::controlType_t::localCCforStateNoBlink)) == true);

    //no messages being sent on release in CC mode
    stateChangeRegister(false);
    TEST_ASSERT(hwaMIDI.midiPacket.size() == 0);

    //nothing should happen on release yet
    TEST_ASSERT(leds.getColor(0) == LEDs::color_t::off);

    stateChangeRegister(true);
    TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);

    //led should be on now
    TEST_ASSERT(leds.getColor(0) != LEDs::color_t::off);

    stateChangeRegister(false);
    TEST_ASSERT(hwaMIDI.midiPacket.size() == 0);

    //no messages sent - led must remain on
    TEST_ASSERT(leds.getColor(0) != LEDs::color_t::off);

    //change the control value for button 0 to something else
    TEST_ASSERT(database.update(Database::Section::button_t::velocity, 0, 126) == true);

    stateChangeRegister(true);
    TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);

    //led should be off now - it has received velocity 126 differing from activating one which is 127
    TEST_ASSERT(leds.getColor(0) == LEDs::color_t::off);

    //try similar thing - cc with reset 0
    for (int i = 0; i < MAX_NUMBER_OF_BUTTONS; i++)
    {
        //configure all buttons as momentary
        TEST_ASSERT(database.update(Database::Section::button_t::type, i, static_cast<int32_t>(Buttons::type_t::momentary)) == true);

        //send cc messages with reset
        TEST_ASSERT(database.update(Database::Section::button_t::midiMessage, i, static_cast<int32_t>(Buttons::messageType_t::controlChangeReset)) == true);

        TEST_ASSERT(database.update(Database::Section::button_t::velocity, i, 127) == true);

        buttons.reset(i);
    }

    stateChangeRegister(true);
    TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);

    TEST_ASSERT(leds.getColor(0) != LEDs::color_t::off);

    stateChangeRegister(false);
    TEST_ASSERT(hwaMIDI.midiPacket.size() == MAX_NUMBER_OF_BUTTONS);

    TEST_ASSERT(leds.getColor(0) == LEDs::color_t::off);
}
#endif

#endif