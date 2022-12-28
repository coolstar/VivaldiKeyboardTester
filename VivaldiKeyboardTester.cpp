// VivaldiKeyboardTester.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <cstdint>

#define UINT8 uint8_t
#define UINT16 uint16_t
#define UINT32 uint32_t
#define USHORT uint16_t
#define ULONG uint32_t
#define PULONG ULONG *

#include <cstdbool>
#define BOOLEAN bool
#define TRUE true
#define FALSE false

#undef bool
#undef true
#undef false

#include <cstring>

#define RtlZeroMemory(Destination,Length) memset((Destination),0,(Length))
#define RtlCopyMemory memcpy

#undef memcpy
#undef memset

#include <iostream>
#include <cassert>
#include "keyboard.h"

const UINT8 fnKeys_set1[] = {
    0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x57, 0x58
};

void ReceiveKeys_Guarded(PKEYBOARD_INPUT_DATA startPtr, PKEYBOARD_INPUT_DATA endPtr, PULONG InputDataConsumed);

#define INTFLAG_NEW 0x1
#define INTFLAG_REMOVED 0x2

typedef struct KeyStruct {
    USHORT MakeCode;
    USHORT Flags;
    USHORT InternalFlags;
} KeyStruct, * PKeyStruct;

typedef struct RemappedKeyStruct {
    struct KeyStruct origKey;
    struct KeyStruct remappedKey;
} RemappedKeyStruct, * PRemappedKeyStruct;

#define MAX_CURRENT_KEYS 20

class VivaldiTester {
    UINT8 legacyTopRowKeys[10];
    UINT8 legacyVivaldi[10];

    UINT8 functionRowCount;
    KeyStruct functionRowKeys[16];

    BOOLEAN LeftCtrlPressed;
    BOOLEAN LeftAltPressed;
    BOOLEAN LeftShiftPressed;
    BOOLEAN SearchPressed;

    KeyStruct currentKeys[MAX_CURRENT_KEYS];
    KeyStruct lastKeyPressed;
    int numKeysPressed = 0;

    RemappedKeyStruct remappedKeys[MAX_CURRENT_KEYS];
    int numRemaps;

    void updateKey(KeyStruct key);

    BOOLEAN addRemap(RemappedKeyStruct remap);
    void garbageCollect();

    BOOLEAN checkKey(KEYBOARD_INPUT_DATA key, KeyStruct report[MAX_CURRENT_KEYS]);
    void RemapPassthrough(KEYBOARD_INPUT_DATA report[MAX_CURRENT_KEYS]);
    void RemapLegacy(KEYBOARD_INPUT_DATA report[MAX_CURRENT_KEYS]);

public:
    VivaldiTester();
    void ServiceCallback(PKEYBOARD_INPUT_DATA InputDataStart, PKEYBOARD_INPUT_DATA InputDataEnd, PULONG InputDataConsumed);
};

#define filterExt this
#define DbgPrint printf

VivaldiTester::VivaldiTester() {
    const UINT8 legacyVivaldi[] = {
        VIVALDI_BACK, VIVALDI_FWD, VIVALDI_REFRESH, VIVALDI_FULLSCREEN, VIVALDI_OVERVIEW, VIVALDI_BRIGHTNESSDN, VIVALDI_BRIGHTNESSUP, VIVALDI_MUTE, VIVALDI_VOLDN, VIVALDI_VOLUP
    };

    const UINT8 legacyVivaldiPixelbook[] = {
        VIVALDI_BACK, VIVALDI_REFRESH, VIVALDI_FULLSCREEN, VIVALDI_OVERVIEW, VIVALDI_BRIGHTNESSDN, VIVALDI_BRIGHTNESSUP, VIVALDI_PLAYPAUSE, VIVALDI_MUTE, VIVALDI_VOLDN, VIVALDI_VOLUP
    };

    filterExt->numKeysPressed = 0;
    RtlZeroMemory(&filterExt->currentKeys, sizeof(filterExt->currentKeys));
    RtlZeroMemory(&filterExt->lastKeyPressed, sizeof(filterExt->lastKeyPressed));

    RtlZeroMemory(&filterExt->remappedKeys, sizeof(filterExt->remappedKeys));
    filterExt->numRemaps = 0;

    filterExt->functionRowCount = 0;
    RtlZeroMemory(&filterExt->functionRowKeys, sizeof(filterExt->functionRowKeys));

    RtlCopyMemory(&filterExt->legacyTopRowKeys, &fnKeys_set1, sizeof(filterExt->legacyTopRowKeys));
    RtlCopyMemory(&filterExt->legacyVivaldi, &legacyVivaldi, sizeof(filterExt->legacyVivaldi));

    filterExt->functionRowCount = sizeof(filterExt->legacyVivaldi);
    for (int i = 0; i < sizeof(filterExt->legacyVivaldi); i++) {
        filterExt->functionRowKeys[i].MakeCode = filterExt->legacyVivaldi[i];
        filterExt->functionRowKeys[i].Flags |= KEY_E0;
    }

    DbgPrint("Initialized\n");
}

#undef filterExt
#define devExt this

void VivaldiTester::updateKey(KeyStruct data) {
    for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
        if (devExt->currentKeys[i].InternalFlags & INTFLAG_REMOVED) {
            RtlZeroMemory(&devExt->currentKeys[i], sizeof(devExt->currentKeys[0])); //Remove any keys marked to be removed
        }
    }

    KeyStruct origData = data;
    //Apply any remaps if they were done
    for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
        if (devExt->remappedKeys[i].origKey.MakeCode == data.MakeCode &&
            devExt->remappedKeys[i].origKey.Flags == (data.Flags & (KEY_E0 | KEY_E1))) {
            data.MakeCode = devExt->remappedKeys[i].remappedKey.MakeCode;
            data.Flags = devExt->remappedKeys[i].remappedKey.Flags | (data.Flags & ~(KEY_E0 | KEY_E1));
            break;
        }
    }

    garbageCollect();

    data.Flags = data.Flags & (KEY_E0 | KEY_E1 | KEY_BREAK);
    if (data.Flags & KEY_BREAK) { //remove
        data.Flags = data.Flags & (KEY_E0 | KEY_E1);
        origData.Flags = origData.Flags & (KEY_E0 | KEY_E1);
        if (devExt->lastKeyPressed.MakeCode == data.MakeCode &&
            devExt->lastKeyPressed.Flags == data.Flags) {
            RtlZeroMemory(&devExt->lastKeyPressed, sizeof(devExt->lastKeyPressed));
        }

        for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
            if (devExt->currentKeys[i].MakeCode == data.MakeCode &&
                devExt->currentKeys[i].Flags == data.Flags) {
                for (int j = 0; j < MAX_CURRENT_KEYS; j++) { //Remove any remaps if the original key is to be removed
                    if (devExt->remappedKeys[j].origKey.MakeCode == origData.MakeCode &&
                        devExt->remappedKeys[j].origKey.Flags == origData.Flags) {
                        RtlZeroMemory(&devExt->remappedKeys[j], sizeof(devExt->remappedKeys[0]));
                    }
                }

                devExt->currentKeys[i].InternalFlags |= INTFLAG_REMOVED;
            }
        }
    }
    else {
        for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
            if (devExt->currentKeys[i].Flags == 0x00 && devExt->currentKeys[i].MakeCode == 0x00) {
                devExt->currentKeys[i] = data;
                devExt->currentKeys[i].InternalFlags |= INTFLAG_NEW;
                devExt->numKeysPressed++;
                devExt->lastKeyPressed = data;
                break;
            }
            else if (devExt->currentKeys[i].Flags == data.Flags && devExt->currentKeys[i].MakeCode == data.MakeCode) {
                break;
            }
        }
    }
}

BOOLEAN VivaldiTester::addRemap(RemappedKeyStruct remap) {
    for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
        if (devExt->remappedKeys[i].origKey.MakeCode == remap.origKey.MakeCode &&
            devExt->remappedKeys[i].origKey.Flags == remap.remappedKey.Flags) {
            if (memcmp(&devExt->remappedKeys[i], &remap, sizeof(remap)) == 0) {
                return TRUE; //already exists
            }
            else {
                return FALSE; //existing remap exists but not the same
            }
        }
    }

    garbageCollect();

    const RemappedKeyStruct emptyStruct = { 0 };
    for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
        if (memcmp(&devExt->remappedKeys[i], &emptyStruct, sizeof(emptyStruct)) == 0) {
            devExt->remappedKeys[i] = remap;


            //Now apply remap
            for (int j = 0; j < MAX_CURRENT_KEYS; j++) {
                if (devExt->currentKeys[j].MakeCode == remap.origKey.MakeCode &&
                    devExt->currentKeys[j].Flags == remap.origKey.Flags) {
                    devExt->currentKeys[j].MakeCode = remap.remappedKey.MakeCode;
                    devExt->currentKeys[j].Flags = remap.remappedKey.Flags;
                    break;
                }
            }

            return TRUE;
        }
    }
    return FALSE; //no slot found
}

void VivaldiTester::garbageCollect() {
    //Clear out any empty remap slots
    for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
        RemappedKeyStruct keyRemaps[MAX_CURRENT_KEYS] = { 0 };
        const RemappedKeyStruct emptyStruct = { 0 };
        int j = 0;
        for (int k = 0; k < MAX_CURRENT_KEYS; k++) {
            if (memcmp(&devExt->remappedKeys[k], &emptyStruct, sizeof(emptyStruct)) != 0) {
                keyRemaps[j] = devExt->remappedKeys[k];
                j++;
            }
        }
        devExt->numRemaps = j;
        RtlCopyMemory(&devExt->remappedKeys, keyRemaps, sizeof(keyRemaps));
    }

    //Clear out any empty key slots
    for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
        KeyStruct keyCodes[MAX_CURRENT_KEYS] = { 0 };
        int j = 0;
        for (int k = 0; k < MAX_CURRENT_KEYS; k++) {
            if (devExt->currentKeys[k].Flags != 0 ||
                devExt->currentKeys[k].MakeCode != 0) {
                keyCodes[j] = devExt->currentKeys[k];
                j++;
            }
        }
        devExt->numKeysPressed = j;
        RtlCopyMemory(&devExt->currentKeys, keyCodes, sizeof(keyCodes));
    }
}

BOOLEAN VivaldiTester::checkKey(KEYBOARD_INPUT_DATA key, KeyStruct report[MAX_CURRENT_KEYS]) {
    for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
        if (report[i].MakeCode == key.MakeCode &&
            report[i].Flags == (key.Flags & (KEY_E0 | KEY_E1))) {
            return TRUE;
        }
    }
    return FALSE;
}

void VivaldiTester::RemapPassthrough(KEYBOARD_INPUT_DATA data[MAX_CURRENT_KEYS]) {
    for (int i = 0; i < devExt->numKeysPressed; i++) {
        for (int j = 0; j < devExt->functionRowCount; j++) { //Set back to F1 -> F12 for passthrough
            if (data[i].MakeCode == devExt->functionRowKeys[j].MakeCode &&
                data[i].Flags == devExt->functionRowKeys->Flags) {
                RemappedKeyStruct remappedStruct = { 0 }; //register remap (Vivaldi => F1 -> F12)
                remappedStruct.origKey.MakeCode = data[i].MakeCode;
                remappedStruct.origKey.Flags = data[i].Flags;
                remappedStruct.remappedKey.MakeCode = fnKeys_set1[j];
                remappedStruct.remappedKey.Flags = KEY_E0;

                if (addRemap(remappedStruct)) {
                    data[i].Flags &= ~(KEY_E0 | KEY_E1);
                    data[i].MakeCode = fnKeys_set1[j];
                }
            }
        }

        if (devExt->LeftCtrlPressed && devExt->LeftAltPressed &&
            data[i].MakeCode == K_BACKSP && data[i].Flags == 0) {
            RemappedKeyStruct remappedStruct = { 0 }; //register remap (Ctrl + Alt + Backspace => Ctrl + Alt + Delete)
            remappedStruct.origKey.MakeCode = data[i].MakeCode;
            remappedStruct.origKey.Flags = data[i].Flags;
            remappedStruct.remappedKey.MakeCode = K_DELETE;
            remappedStruct.remappedKey.Flags = KEY_E0;

            if (addRemap(remappedStruct)) {
                data[i].MakeCode = K_DELETE;
                data[i].Flags |= KEY_E0;
            }
        }
    }
}

void VivaldiTester::RemapLegacy(KEYBOARD_INPUT_DATA data[MAX_CURRENT_KEYS]) {
    for (int i = 0; i < devExt->numKeysPressed; i++) {
        if (!devExt->LeftCtrlPressed) {
            for (int j = 0; j < devExt->functionRowCount; j++) { //Set back to F1 -> F12 for passthrough
                if (data[i].MakeCode == devExt->functionRowKeys[j].MakeCode &&
                    data[i].Flags == devExt->functionRowKeys->Flags) {
                    RemappedKeyStruct remappedStruct = { 0 }; //register remap
                    remappedStruct.origKey.MakeCode = data[i].MakeCode;
                    remappedStruct.origKey.Flags = data[i].Flags;
                    remappedStruct.remappedKey.MakeCode = fnKeys_set1[j];

                    if (addRemap(remappedStruct)) {
                        data[i].Flags &= ~(KEY_E0 | KEY_E1);
                        data[i].MakeCode = fnKeys_set1[j];
                    }
                }
            }
        }

        if (devExt->LeftCtrlPressed && devExt->LeftAltPressed &&
            data[i].MakeCode == K_BACKSP && data[i].Flags == 0) {
            RemappedKeyStruct remappedStruct = { 0 }; //register remap (Ctrl + Alt + Backspace => Ctrl + Alt + Delete)
            remappedStruct.origKey.MakeCode = data[i].MakeCode;
            remappedStruct.origKey.Flags = data[i].Flags;
            remappedStruct.remappedKey.MakeCode = K_DELETE;
            remappedStruct.remappedKey.Flags = KEY_E0;

            if (addRemap(remappedStruct)) {
                data[i].MakeCode = K_DELETE;
                data[i].Flags |= KEY_E0;
            }
        }
    }
}

void VivaldiTester::ServiceCallback(PKEYBOARD_INPUT_DATA InputDataStart, PKEYBOARD_INPUT_DATA InputDataEnd, PULONG InputDataConsumed) {
    PKEYBOARD_INPUT_DATA pData;
    for (pData = InputDataStart; pData != InputDataEnd; pData++) { //First loop -> Refresh Modifier Keys and Change Legacy Keys to vivaldi bindings
        if ((pData->Flags & (KEY_E0 | KEY_E1)) == 0) {
            switch (pData->MakeCode)
            {
            case K_LCTRL: //L CTRL
                if ((pData->Flags & KEY_BREAK) == 0) {
                    devExt->LeftCtrlPressed = TRUE;
                }
                else {
                    devExt->LeftCtrlPressed = FALSE;
                }
                break;
            case K_LALT: //L Alt
                if ((pData->Flags & KEY_BREAK) == 0) {
                    devExt->LeftAltPressed = TRUE;
                }
                else {
                    devExt->LeftAltPressed = FALSE;
                }
                break;
            case K_LSHFT: //L Shift
                if ((pData->Flags & KEY_BREAK) == 0) {
                    devExt->LeftShiftPressed = TRUE;
                }
                else {
                    devExt->LeftShiftPressed = FALSE;
                }
                break;
            default:
                for (int i = 0; i < sizeof(devExt->legacyTopRowKeys); i++) {
                    if (pData->MakeCode == devExt->legacyTopRowKeys[i]) {
                        pData->MakeCode = devExt->legacyVivaldi[i];
                        pData->Flags |= KEY_E0; //All legacy vivaldi upgrades use E0 modifier
                    }
                }

                break;
            }
        }
        if ((pData->Flags & (KEY_E0 | KEY_E1)) == KEY_E0) {
            if (pData->MakeCode == 0x5B) { //Search Key
                if ((pData->Flags & KEY_BREAK) == 0) {
                    devExt->SearchPressed = TRUE;
                }
                else {
                    devExt->SearchPressed = FALSE;
                }
            }
        }
    }

    {
        //Now make the data HID-like for easier handling
        ULONG i = 0;
        for (i = 0; i < (InputDataEnd - InputDataStart); i++) {
            KeyStruct key = { 0 };
            key.MakeCode = InputDataStart[i].MakeCode;
            key.Flags = InputDataStart[i].Flags;
            updateKey(key);
        }
        *InputDataConsumed = i;
    }

    KEYBOARD_INPUT_DATA newReport[MAX_CURRENT_KEYS] = { 0 };
    //Add new keys
    for (int i = 0, j = 0; i < devExt->numKeysPressed; i++) { //Prepare new report for remapper to sort through
        if (devExt->currentKeys[i].InternalFlags & INTFLAG_NEW) {
            newReport[j].MakeCode = devExt->currentKeys[i].MakeCode;
            newReport[j].Flags = devExt->currentKeys[i].Flags;
            devExt->currentKeys[i].InternalFlags &= ~INTFLAG_NEW;
            j++;
        }
    }

    //Do whichever remap was chosen
    //RemapPassthrough(newReport);
    RemapLegacy(newReport);

    //Remove any empty keys
    int newReportKeysPresent = 0;
    for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
        if (newReport[i].Flags != 0 ||
            newReport[i].MakeCode != 0) {
            newReport[newReportKeysPresent] = newReport[i];
            newReportKeysPresent++;
        }
    }

    for (int i = newReportKeysPresent; i < MAX_CURRENT_KEYS; i++) {
        RtlZeroMemory(&newReport[i], sizeof(newReport[i]));
    }

    //Now add all the removed keys
    int reportSize = newReportKeysPresent;
    for (int i = 0; i < devExt->numKeysPressed; i++) { //Prepare new report for remapper to sort through
        if (devExt->currentKeys[i].InternalFlags & INTFLAG_REMOVED) {
            newReport[reportSize].MakeCode = devExt->currentKeys[i].MakeCode;
            newReport[reportSize].Flags = devExt->currentKeys[i].Flags | KEY_BREAK;
            reportSize++;
        }
    }

    //If empty report keys, add the last key (if present)
    if (reportSize == 0 && (devExt->lastKeyPressed.MakeCode != 0 || devExt->lastKeyPressed.Flags != 0)) {
        newReport[reportSize].MakeCode = devExt->lastKeyPressed.MakeCode;
        newReport[reportSize].Flags = devExt->lastKeyPressed.Flags;
        reportSize++;
    }

    //Now prepare the report
    for (int i = 0; i < reportSize; i++) {
        newReport[i].UnitId = InputDataStart[0].UnitId;
    }

    ULONG DataConsumed;
    DbgPrint("\tLegacy Keys\n");
    if (InputDataEnd > InputDataStart) {
        ReceiveKeys_Guarded(InputDataStart, InputDataEnd, &DataConsumed);
    }

    DbgPrint("\tHID translated Keys\n");
    if (reportSize > 0) {
        ReceiveKeys_Guarded(newReport, newReport + reportSize, &DataConsumed);
    }
}

int CompareKeys(const void *raw1, const void *raw2) {
    PKEYBOARD_INPUT_DATA data1 = (PKEYBOARD_INPUT_DATA)raw1;
    PKEYBOARD_INPUT_DATA data2 = (PKEYBOARD_INPUT_DATA)raw2;
    return ((data1->MakeCode - data2->MakeCode) << 4) +
        ((data2->Flags & (KEY_E0 | KEY_E1) - (data1->Flags & (KEY_E0 | KEY_E1))));
}

void ReceiveKeys_Guarded(PKEYBOARD_INPUT_DATA startPtr, PKEYBOARD_INPUT_DATA endPtr, PULONG InputDataConsumed) {
    qsort(startPtr, endPtr - startPtr, sizeof(*startPtr), CompareKeys);

    ULONG consumedCount = 0;

    printf("\t\t==Frame Start==\n");
    for (ULONG i = 0; i < endPtr - startPtr; i++) {
        printf("\t\t\tKey %d: Code 0x%x, Flags 0x%x\n", i, startPtr[i].MakeCode, startPtr[i].Flags);
        consumedCount++;
    }
    printf("\t\t==Frame End==\n");

    for (ULONG i = 0; i < (endPtr - 1) - startPtr; i++) {
        assert(startPtr[i].MakeCode != startPtr[i + 1].MakeCode ||
            (startPtr[i].Flags & (KEY_E0 | KEY_E1)) != (startPtr[i + 1].Flags & (KEY_E0 | KEY_E1)));
    }

    *InputDataConsumed = consumedCount;
}

void SubmitKeys_Guarded(VivaldiTester *test, PKEYBOARD_INPUT_DATA start, UINT32 count) {
    PKEYBOARD_INPUT_DATA newData = (PKEYBOARD_INPUT_DATA)malloc(sizeof(KEYBOARD_INPUT_DATA) * count);
    assert(newData != 0);

    RtlCopyMemory(newData, start, sizeof(KEYBOARD_INPUT_DATA) * count);

    ULONG consumedCount = 0;
    test->ServiceCallback(newData, newData + count, &consumedCount);

    assert(consumedCount == count); //Weird breakage can happen if this isn't equal
    free(newData);
}

int main()
{
    VivaldiTester test;

    KEYBOARD_INPUT_DATA testData[2];
    RtlZeroMemory(testData, sizeof(testData)); //Reset test data

    testData[0].MakeCode = K_LCTRL;
    printf("Ctrl\n");
    SubmitKeys_Guarded(&test, testData, 1);

    printf("Ctrl Repeat\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = VIVALDI_MUTE;
    testData[0].Flags = KEY_E0;
    printf("Mute\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].Flags |= KEY_BREAK;
    printf("Mute Release\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = K_LCTRL;
    testData[0].Flags = 0;
    printf("Ctrl Repeat\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].Flags |= KEY_BREAK;
    printf("Ctrl Release\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = VIVALDI_MUTE;
    testData[0].Flags = KEY_E0;
    printf("Mute\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].Flags |= KEY_BREAK;
    printf("Mute Release\n");
    SubmitKeys_Guarded(&test, testData, 1);

    RtlZeroMemory(testData, sizeof(testData)); //Reset test data

    testData[0].MakeCode = 0x1E;
    testData[0].Flags = 0;
    printf("A Press\n");
    SubmitKeys_Guarded(&test, testData, 1);

    printf("A Hold\n");
    SubmitKeys_Guarded(&test, testData, 1);

    printf("A Hold\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = 0x1F;
    testData[0].Flags = 0;
    printf("S Press + A Hold\n");
    SubmitKeys_Guarded(&test, testData, 1);

    printf("S + A Hold\n");
    SubmitKeys_Guarded(&test, testData, 1);

    printf("S + A Hold\n");
    SubmitKeys_Guarded(&test, testData, 1);

    printf("S + A Hold\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = 0x1E;
    testData[0].Flags = KEY_BREAK;
    printf("A Release\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = 0x1F;
    testData[0].Flags = 0;
    printf("S Hold\n");
    SubmitKeys_Guarded(&test, testData, 1);

    printf("S Hold\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = 0x1F;
    testData[0].Flags = KEY_BREAK;
    printf("S Release\n");
    SubmitKeys_Guarded(&test, testData, 1);

    RtlZeroMemory(testData, sizeof(testData)); //Reset test data

    testData[0].MakeCode = K_LCTRL;
    printf("Ctrl\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = K_LALT;
    printf("Alt\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = K_BACKSP;
    printf("Backspace\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = K_LCTRL;
    testData[0].Flags = KEY_BREAK;
    printf("Release Ctrl\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = K_LALT;
    testData[0].Flags = KEY_BREAK;
    printf("Release Alt\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = K_BACKSP;
    testData[0].Flags = KEY_BREAK;
    printf("Release Backspace\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = K_BACKSP;
    testData[0].Flags = 0;
    printf("Backspace\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = K_BACKSP;
    testData[0].Flags = KEY_BREAK;
    printf("Release Backspace\n");
    SubmitKeys_Guarded(&test, testData, 1);
}