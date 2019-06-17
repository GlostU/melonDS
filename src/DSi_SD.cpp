/*
    Copyright 2016-2019 Arisotura

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include <stdio.h>
#include <string.h>
#include "DSi.h"
#include "DSi_SD.h"
#include "Platform.h"


#define SD_DESC  (Num?"SDIO":"SD/MMC")


DSi_SDHost::DSi_SDHost(u32 num)
{
    Num = num;

    Ports[0] = NULL;
    Ports[1] = NULL;
}

DSi_SDHost::~DSi_SDHost()
{
    if (Ports[0]) delete Ports[0];
    if (Ports[1]) delete Ports[1];
}

void DSi_SDHost::Reset()
{
    if (Num == 0)
    {
        PortSelect = 0x0200; // CHECKME
    }
    else
    {
        PortSelect = 0x0100; // CHECKME
    }

    SoftReset = 0x0007; // CHECKME
    SDClock = 0;

    Command = 0;
    Param = 0;
    memset(ResponseBuffer, 0, sizeof(ResponseBuffer));

    IRQStatus = 0;
    IRQMask = 0x8B7F031D;

    if (Ports[0]) delete Ports[0];
    if (Ports[1]) delete Ports[1];
    Ports[0] = NULL;
    Ports[1] = NULL;

    if (Num == 0)
    {
        DSi_MMCStorage* mmc = new DSi_MMCStorage(this, true, "nand.bin");
        mmc->SetCID(DSi::eMMC_CID);

        // TODO: port 0 (SD)
        Ports[1] = mmc;
    }
    else
    {
        // TODO: SDIO (wifi)
    }
}

void DSi_SDHost::DoSavestate(Savestate* file)
{
    // TODO!
}


void DSi_SDHost::SetIRQ(u32 irq)
{
    u32 oldflags = IRQStatus & ~IRQMask;

    IRQStatus |= (1<<irq);
    u32 newflags = IRQStatus & ~IRQMask;

    if ((oldflags == 0) && (newflags != 0))
        NDS::SetIRQ2(Num ? NDS::IRQ2_DSi_SDIO : NDS::IRQ2_DSi_SDMMC);
}

void DSi_SDHost::SendResponse(u32 val, bool last)
{
    *(u32*)&ResponseBuffer[6] = *(u32*)&ResponseBuffer[4];
    *(u32*)&ResponseBuffer[4] = *(u32*)&ResponseBuffer[2];
    *(u32*)&ResponseBuffer[2] = *(u32*)&ResponseBuffer[0];
    *(u32*)&ResponseBuffer[0] = val;

    if (last) SetIRQ(0);
}


u16 DSi_SDHost::Read(u32 addr)
{
    //printf("SDMMC READ %08X %08X\n", addr, NDS::GetPC(1));

    switch (addr & 0x1FF)
    {
    case 0x000: return Command;
    case 0x002: return PortSelect & 0x030F;
    case 0x004: return Param & 0xFFFF;
    case 0x006: return Param >> 16;

    case 0x00C: return ResponseBuffer[0];
    case 0x00E: return ResponseBuffer[1];
    case 0x010: return ResponseBuffer[2];
    case 0x012: return ResponseBuffer[3];
    case 0x014: return ResponseBuffer[4];
    case 0x016: return ResponseBuffer[5];
    case 0x018: return ResponseBuffer[6];
    case 0x01A: return ResponseBuffer[7];

    case 0x01C: return IRQStatus & 0x031D;
    case 0x01E: return (IRQStatus >> 16) & 0x8B7F;
    case 0x020: return IRQMask & 0x031D;
    case 0x022: return (IRQMask >> 16) & 0x8B7F;

    case 0x024: return SDClock;

    case 0x0E0: return SoftReset;
    }

    printf("unknown %s read %08X\n", SD_DESC, addr);
    return 0;
}

void DSi_SDHost::Write(u32 addr, u16 val)
{
    //printf("SDMMC WRITE %08X %04X %08X\n", addr, val, NDS::GetPC(1));

    switch (addr & 0x1FF)
    {
    case 0x000:
        {
            Command = val;
            u8 cmd = Command & 0x3F;

            DSi_SDDevice* dev = Ports[PortSelect & 0x1];
            if (dev)
            {
                // CHECKME
                // "Setting Command Type to "ACMD" is automatically sending an APP_CMD prefix prior to the command number"
                // except DSi boot2 manually sends an APP_CMD prefix AND sets the next command to be ACMD
                switch ((Command >> 6) & 0x3)
                {
                case 0: dev->SendCMD(cmd, Param); break;
                case 1: /*dev->SendCMD(55, 0);*/ dev->SendCMD(cmd, Param); break;
                default:
                    printf("%s: unknown command type %d, %02X %08X\n", SD_DESC, (Command>>6)&0x3, cmd, Param);
                    break;
                }
            }
        }
        return;

    case 0x002: PortSelect = val; return;
    case 0x004: Param = (Param & 0xFFFF0000) | val; return;
    case 0x006: Param = (Param & 0x0000FFFF) | (val << 16); return;

    case 0x01C: IRQStatus &= ~(u32)val; return;
    case 0x01E: IRQStatus &= ~((u32)val << 16); return;
    case 0x020: IRQMask = (IRQMask & 0x8B7F0000) | (val & 0x031D); return;
    case 0x022: IRQMask = (IRQMask & 0x0000031D) | ((val & 0x8B7F) << 16); return;

    case 0x024: SDClock = val & 0x03FF; return;

    case 0x0E0:
        if ((SoftReset & 0x0001) && !(val & 0x0001))
        {
            printf("%s: RESET\n", SD_DESC);
            // TODO: STOP_INTERNAL_ACTION
            // TODO: SD_RESPONSE
            IRQStatus = 0;
            // TODO: ERROR_DETAIL_STATUS
            // TODO: CARD_CLK_CTL
            // TODO: CARD_OPTION
            // TODO: CARD_IRQ_STAT
            // TODO: FIFO16 shit
        }
        SoftReset = 0x0006 | (val & 0x0001);
        return;
    }

    printf("unknown %s write %08X %04X\n", SD_DESC, addr, val);
}


DSi_MMCStorage::DSi_MMCStorage(DSi_SDHost* host, bool internal, const char* path) : DSi_SDDevice(host)
{
    Internal = internal;
    strncpy(FilePath, path, 1023); FilePath[1023] = '\0';

    File = Platform::OpenLocalFile(path, "r+b");

    CSR = 0x00; // checkme

    // TODO: busy bit
    // TODO: SDHC/SDXC bit
    OCR = 0x80FF8000;

    // TODO: customize based on card size etc
    u8 csd_template[16] = {0x40, 0x40, 0x96, 0xE9, 0x7F, 0xDB, 0xF6, 0xDF, 0x01, 0x59, 0x0F, 0x2A, 0x01, 0x26, 0x90, 0x00};
    memcpy(CSD, csd_template, 16);
}

DSi_MMCStorage::~DSi_MMCStorage()
{
    if (File) fclose(File);
}

void DSi_MMCStorage::SendCMD(u8 cmd, u32 param)
{
    if (CSR & (1<<5))
    {
        CSR &= ~(1<<5);
        return SendACMD(cmd, param);
    }

    switch (cmd)
    {
    case 0: // reset/etc
        Host->SendResponse(CSR, true);
        return;

    case 2:
    case 10: // get CID
        Host->SendResponse(*(u32*)&CID[0], false);
        Host->SendResponse(*(u32*)&CID[1], false);
        Host->SendResponse(*(u32*)&CID[2], false);
        Host->SendResponse(*(u32*)&CID[3], true);
        //if (cmd == 2) SetState(0x02);
        return;

    case 3: // get/set RCA
        if (Internal)
        {
            RCA = param >> 16;
            Host->SendResponse(CSR|0x10000, true); // huh??
        }
        else
        {
            // TODO
            printf("CMD3 on SD card: TODO\n");
        }
        return;

    case 7: // select card (by RCA)
        Host->SendResponse(CSR, true);
        return;

    case 8: // set voltage
        Host->SendResponse(param, true);
        return;

    case 9: // get CSD
        Host->SendResponse(*(u32*)&CSD[0], false);
        Host->SendResponse(*(u32*)&CSD[1], false);
        Host->SendResponse(*(u32*)&CSD[2], false);
        Host->SendResponse(*(u32*)&CSD[3], true);
        return;

    case 55: // ??
        printf("CMD55 %08X\n", param);
        CSR |= (1<<5);
        Host->SendResponse(CSR, true);
        return;
    }

    printf("MMC: unknown CMD %d %08X\n", cmd, param);
}

void DSi_MMCStorage::SendACMD(u8 cmd, u32 param)
{
    switch (cmd)
    {
    case 41: // set operating conditions
        OCR &= 0xBF000000;
        OCR |= (param & 0x40FFFFFF);
        Host->SendResponse(OCR, true);
        SetState(0x01);
        return;
    }

    printf("MMC: unknown ACMD %d %08X\n", cmd, param);
}