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

typedef struct KeySetting {
    USHORT MakeCode;
    USHORT Flags;
} KeySetting, * PKeySetting;

#define MAX_CURRENT_KEYS 20

class VivaldiTester {
    UINT8 legacyTopRowKeys[10];
    UINT8 legacyVivaldi[10];

    UINT8 functionRowCount;
    KeySetting functionRowKeys[16];

    BOOLEAN LeftCtrlPressed;
    BOOLEAN LeftAltPressed;
    BOOLEAN LeftShiftPressed;
    BOOLEAN SearchPressed;

    KeySetting currentKeys[MAX_CURRENT_KEYS];
    int numKeysPressed = 0;
    
    KEYBOARD_INPUT_DATA lastReported[MAX_CURRENT_KEYS];

    void updateKey(KeySetting key);
    BOOLEAN checkKey(KEYBOARD_INPUT_DATA key, KEYBOARD_INPUT_DATA report[MAX_CURRENT_KEYS]);
    void RemapPassthrough(KEYBOARD_INPUT_DATA report[MAX_CURRENT_KEYS]);

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

    RtlZeroMemory(&filterExt->lastReported, sizeof(filterExt->lastReported));

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

void VivaldiTester::updateKey(KeySetting data) {
    data.Flags = data.Flags & (KEY_E0 | KEY_E1 | KEY_BREAK);
    if (data.Flags & KEY_BREAK) { //remove
        data.Flags = data.Flags & (KEY_E0 | KEY_E1);
        for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
            if (devExt->currentKeys[i].MakeCode == data.MakeCode &&
                devExt->currentKeys[i].Flags == data.Flags) {
                devExt->currentKeys[i].MakeCode = 0;
                devExt->currentKeys[i].Flags = 0;
            }
            KeySetting keyCodes[MAX_CURRENT_KEYS] = { 0 };
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
    else {
        for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
            if (devExt->currentKeys[i].Flags == 0x00 && devExt->currentKeys[i].MakeCode == 0x00) {
                devExt->currentKeys[i] = data;
                devExt->numKeysPressed++;
                break;
            }
            else if (devExt->currentKeys[i].Flags == data.Flags && devExt->currentKeys[i].MakeCode == data.MakeCode) {
                return;
            }
        }
    }
}

BOOLEAN VivaldiTester::checkKey(KEYBOARD_INPUT_DATA key, KEYBOARD_INPUT_DATA report[MAX_CURRENT_KEYS]) {
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
                data[i].Flags &= ~(KEY_E0 | KEY_E1);
                data[i].MakeCode = fnKeys_set1[i];
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
            KeySetting key;
            key.MakeCode = InputDataStart[i].MakeCode;
            key.Flags = InputDataStart[i].Flags;
            updateKey(key);
        }
        *InputDataConsumed = i;
    }

    KEYBOARD_INPUT_DATA newReport[MAX_CURRENT_KEYS] = { 0 };
    for (int i = 0; i < devExt->numKeysPressed; i++) { //Prepare new report for remapper to sort through
        newReport[i].MakeCode = devExt->currentKeys[i].MakeCode;
        newReport[i].Flags = devExt->currentKeys[i].Flags;
    }
    //RemapPassthrough(newReport);

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
    for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
        if (devExt->lastReported[i].MakeCode == 0 && devExt->lastReported[i].Flags == 0)
            break;
        if (!checkKey(devExt->lastReported[i], newReport)) {
            newReport[reportSize].MakeCode = devExt->lastReported[i].MakeCode;
            newReport[reportSize].Flags = devExt->lastReported[i].Flags | KEY_BREAK;

            reportSize++;
            if (reportSize == (MAX_CURRENT_KEYS - 1))
                break;
        }
    }

    RtlZeroMemory(devExt->lastReported, sizeof(devExt->lastReported));
    RtlCopyMemory(devExt->lastReported, newReport, sizeof(newReport[0]) * newReportKeysPresent);

    //Now prepare the report
    for (int i = 0; i < reportSize; i++) {
        newReport[i].UnitId = InputDataStart[0].UnitId;
    }

    ULONG DataConsumed;
    DbgPrint("Legacy Keys\n");
    if (InputDataEnd > InputDataStart) {
        ReceiveKeys_Guarded(InputDataStart, InputDataEnd, &DataConsumed);
    }

    DbgPrint("HID translated Keys\n");
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

    for (ULONG i = 0; i < (endPtr - 1) - startPtr; i++) {
        assert(startPtr[i].MakeCode != startPtr[i + 1].MakeCode || 
            (startPtr[i].Flags & (KEY_E0 | KEY_E1)) != (startPtr[i + 1].Flags & (KEY_E0 | KEY_E1)));
    }

    printf("\t==Frame Start==\n");
    for (ULONG i = 0; i < endPtr - startPtr; i++) {
        printf("\t\tKey %d: Code 0x%x, Flags 0x%x\n", i, startPtr[i].MakeCode, startPtr[i].Flags);
        consumedCount++;
    }
    printf("\t==Frame End==\n");

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

    KEYBOARD_INPUT_DATA testData[10];
    RtlZeroMemory(testData, sizeof(testData));

    testData[0].MakeCode = K_LCTRL;
    SubmitKeys_Guarded(&test, testData, 1);

    SubmitKeys_Guarded(&test, testData, 1);

    testData[1].MakeCode = VIVALDI_MUTE;
    testData[1].Flags = KEY_E0;
    SubmitKeys_Guarded(&test, testData, 2);

    testData[1].Flags |= KEY_BREAK;
    SubmitKeys_Guarded(&test, testData, 2);

    testData[1].MakeCode = 0;
    testData[1].Flags = 0;
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].Flags |= KEY_BREAK;
    SubmitKeys_Guarded(&test, testData, 1);

    RtlZeroMemory(testData, sizeof(testData));

    testData[0].MakeCode = VIVALDI_MUTE;
    testData[0].Flags = KEY_E0;
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].Flags |= KEY_BREAK;
    SubmitKeys_Guarded(&test, testData, 1);
}