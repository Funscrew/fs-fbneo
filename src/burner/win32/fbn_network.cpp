#include "burner.h"
#include <stdexcept>

const int MAXPLAYER = 4;

// The counts of each type of input.
static int nPlayerInputs[MAXPLAYER];
static int nConstInputs;
static int nDIPInputs;

// The offsets into the arrays as accessed by: BurnDrvGetInputInfo(i) where the types of inputs are located.
static int nPlayerOffset[MAXPLAYER];
static int nConstOffsets;
static int nDIPOffset;

//const int INPUTSIZE = 8 * (4 + 8);				// We are assuming a max of eight bytes of input for up to 12 players.
//// REFACTOR: Use uint8_t....
//static unsigned char nControls[INPUTSIZE];

// ---------------------------------------------------------------------------------------------------------------------
// Inputs are assumed to be in the following order:
// All player 1 controls
// All player 2 controls (if any)
// All player 3 controls (if any)
// All player 4 controls (if any)
// All common controls
// All DIP switches
int NetworkInitInput()
{
  if (nGameInpCount == 0) {
    return 1;
  }

  struct BurnInputInfo bii;
  memset(&bii, 0, sizeof(bii));

  unsigned int i = 0;

  // All of this code is computing the indexes for where the p1 / p2, etc. controls begin.
  // They are scanning for Pn descriptions + counting the indexes.
  // This can be severly cleaned up with a better, more concise input system.
  nPlayerOffset[0] = 0;
  do {
    BurnDrvGetInputInfo(&bii, i);
    i++;
  } while (!_strnicmp(bii.szName, "P1", 2) && i <= nGameInpCount);
  i--;
  nPlayerInputs[0] = i - nPlayerOffset[0];

  for (int j = 1; j < MAXPLAYER; j++) {
    char szString[3] = "P?";
    szString[1] = j + '1';
    nPlayerOffset[j] = i;
    while (!_strnicmp(bii.szName, szString, 2) && i < nGameInpCount) {
      i++;
      BurnDrvGetInputInfo(&bii, i);
    }
    nPlayerInputs[j] = i - nPlayerOffset[j];
  }

  nConstOffsets = i;
  while ((bii.nType & BIT_GROUP_CONSTANT) == 0 && i < nGameInpCount) {
    i++;
    BurnDrvGetInputInfo(&bii, i);
  };
  nConstInputs = i - nConstOffsets;

  nDIPOffset = i;
  nDIPInputs = nGameInpCount - nDIPOffset;

  // NOTE: All of the above code is making big assumptions about where the data actually is.
  // Granted the conventions of the driver defs in FBNEO support this, but it is still a bad
  // practice.  New input system will resolve this for us.


  debugPrintf(_T("  * Network inputs configured as follows --\n"));
  for (int j = 0; j < MAXPLAYER; j++) {
    debugPrintf(_T("    p%d offset %d, inputs %d.\n"), j + 1, nPlayerOffset[j], nPlayerInputs[j]);
  }
  debugPrintf(_T("    common offset %d, inputs %d.\n"), nConstOffsets, nConstInputs);
  debugPrintf(_T("    dip offset %d, inputs %d.\n"), nDIPOffset, nDIPInputs);

  return 0;
}


// ---------------------------------------------------------------------------------------------------------------------
// TODO: Allow a future version to use a direct memory address to unpack from.
// From the nControls data, this will read back inputs + update the values in the emulator directly.
// inputSize: size in memory of all inputs for all players.
void UnpackGameInputs(int playerInputSize) {

  // int playerInputSize = inputSize / nMaxPlayers;

  struct BurnInputInfo bii;
  memset(&bii, 0, sizeof(bii));

  int i, j = 0;

  // Decode Player 1 input block.
  // This takes the synced inputs and decodes them back into the system input
  // memory.
  for (i = 0, j = 0; i < nPlayerInputs[0]; i++, j++) {
    BurnDrvGetInputInfo(&bii, i + nPlayerOffset[0]);
    if (bii.nType == BIT_DIGITAL) {
      if (nControls[j >> 3] & (1 << (j & 7))) {
        *bii.pVal = 0x01;
      }
      else {
        *bii.pVal = 0x00;
      }
    }
  }
  for (i = 0; i < nConstInputs; i++, j++) {
    BurnDrvGetInputInfo(&bii, i + nConstOffsets);
    if (nControls[j >> 3] & (1 << (j & 7))) {
      *bii.pVal = 0x01;
    }
    else {
      *bii.pVal = 0x00;
    }
  }

  // Convert j to byte count
  j = (j + 7) >> 3;

  // Analog inputs
  for (i = 0; i < nPlayerInputs[0]; i++) {
    BurnDrvGetInputInfo(&bii, i + nDIPOffset);
    if (bii.nType & BIT_GROUP_ANALOG) {
      *bii.pShortVal = (nControls[j] << 8) | nControls[j + 1];
      j += 2;
    }
  }

  // DIP switches --> each one gets a byte!
  for (i = 0; i < nDIPInputs; i++, j++) {
    BurnDrvGetInputInfo(&bii, i + nDIPOffset);
    *bii.pVal = nControls[j];
  }

  // Decode other player's input blocks
  for (int l = 1; l < nMaxPlayers; l++) {
    // Only decode if there is a player for this game.
    if (nPlayerInputs[l]) {
      for (i = 0, j = playerInputSize * (l << 3); i < nPlayerInputs[l]; i++, j++) {
        BurnDrvGetInputInfo(&bii, i + nPlayerOffset[l]);
        if (bii.nType == BIT_DIGITAL) {
          if (nControls[j >> 3] & (1 << (j & 7))) {
            *bii.pVal = 0x01;
          }
          else {
            *bii.pVal = 0x00;
          }
        }
      }

      for (i = 0; i < nConstInputs; i++, j++) {
#if 0
        // Allow other players to use common inputs
        BurnDrvGetInputInfo(&bii, i + nConstOffsets);
        if (nControls[j >> 3] & (1 << (j & 7))) {
          *bii.pVal |= 0x01;
        }
#endif
      }

      // Convert j to byte count
      j = (j + 7) >> 3;

      // Analog inputs/constants
      for (i = 0; i < nPlayerInputs[l]; i++) {
        BurnDrvGetInputInfo(&bii, i + nPlayerOffset[l]);
        if (bii.nType != BIT_DIGITAL) {
          if (bii.nType & BIT_GROUP_ANALOG) {
            *bii.pShortVal = (nControls[j] << 8) | nControls[j + 1];
            j += 2;
          }
        }
      }

      // TEST if this is needed for both players?
#if 1
      // For a DIP switch to be set to 1, ALL players must set it
      for (i = 0; i < nDIPInputs; i++, j++) {
        BurnDrvGetInputInfo(&bii, i + nDIPOffset);
        *bii.pVal &= nControls[j];
      }
#endif
    }
  }

}

// ---------------------------------------------------------------------------------------------------------------------
// TODO: In a future iteration let's make it so we can choose where we put the inputs in memory....
int PackGameInputs()
{
  int res = 0;
  int memOffset = 0;

  struct BurnInputInfo bii;
  memset(&bii, 0, sizeof(bii));

  // Initialize controls to 0
  memset(nControls, 0, INPUTSIZE);

  // Get all inputs for all players.
  for (size_t pi = 0; pi < nMaxPlayers; pi++)
  {
    int inputCount = nPlayerInputs[pi];
    int inputOffset = nPlayerOffset[pi];

    int i = 0;
    int j = memOffset;

    // Pack all DIP switches + common controls + Player 1 controls.
    for (i = 0, j = 0; i < inputCount; i++, j++) {
      BurnDrvGetInputInfo(&bii, i + inputOffset);
      if (*bii.pVal && bii.nType == BIT_DIGITAL) {
        UINT wIndex = (j >> 3) + memOffset;
        nControls[wIndex] |= (1 << (j & 7));
      }
    }

    // HACK: This is a ROM specific mod.  This should be handled as a kind of 'plugin' vs. hard coding.
    for (i = 0; i < nConstInputs; i++, j++) {
      BurnDrvGetInputInfo(&bii, i + nConstOffsets);
      bool allow_reset = !strcmp(BurnDrvGetTextA(DRV_NAME), "sf2hf") && kNetVersion >= NET_VERSION_RESET_SF2HF;
      bool can_tilt = !kNetGame || strcmp(bii.szName, "Tilt");
      bool can_reset = !kNetGame || VidOverlayCanReset() || (strcmp(bii.szName, "Reset") && strcmp(bii.szName, "Diagnostic") && strcmp(bii.szName, "Service") && strcmp(bii.szName, "Test")) || allow_reset;
      if (*bii.pVal && can_tilt && can_reset) {
        UINT wIndex = (j >> 3) + memOffset;
        nControls[wIndex] |= (1 << (j & 7));
      }
    }

    // Convert j to byte count (>> 3 == / 8)
    UINT wIndex = ((j + 7) >> 3) + memOffset;

    // Analog controls/constants.
    // These get 1-2 byte each, which makes sense because they need to encode more data.
    for (i = 0; i < nPlayerInputs[0]; i++) {
      BurnDrvGetInputInfo(&bii, i + inputOffset);
      if (*bii.pVal && bii.nType != BIT_DIGITAL) {
        if (bii.nType & BIT_GROUP_ANALOG) {
          nControls[wIndex++] = *bii.pShortVal >> 8;
          nControls[wIndex++] = *bii.pShortVal & 0xFF;
        }
        else {
          nControls[wIndex++] = *bii.pVal;
        }
      }
    }

    // DIP switches		-- For some reason, DIP switches each get a full byte, but everything else gets a bit....
    // Seems like these could also be backed into bits, but maybe we don't because the offset it unclear,
    // and there are relatively few DIPS for any given game....
    // Maybe it is because the analog inputs showed up first?
    for (i = 0; i < nDIPInputs; i++) {
      BurnDrvGetInputInfo(&bii, i + nDIPOffset);
      nControls[wIndex++] = *bii.pVal;
    }

    // k is the size in bytes of all inputs for one player
    int k = wIndex + 1;

    res = k;
    memOffset = k;
  }


  return res;
}

// ---------------------------------------------------------------------------------------------------------------------
int NetworkGetInput(int size)
{
  // int i, j;

  // HACK for calls from initialization type code.
  // if (size == -1) { throw std::runtime_error("fail!"); }
  //  size = PackGameInputs();
  //}

  //struct BurnInputInfo bii;
  //memset(&bii, 0, sizeof(bii));

  //// Initialize controls to 0
  //memset(nControls, 0, INPUTSIZE);

  //// Pack all DIP switches + common controls + Player 1 controls.
  //for (i = 0, j = 0; i < nPlayerInputs[0]; i++, j++) {
  //	BurnDrvGetInputInfo(&bii, i + nPlayerOffset[0]);
  //	UINT jIndex = j >> 3;
  //	if (*bii.pVal && bii.nType == BIT_DIGITAL) {
  //		nControls[j >> 3] |= (1 << (j & 7));
  //	}
  //}

 // // HACK: This is a ROM specific mod.  This should be handled as a kind of 'plugin' vs. hard coding.
  //for (i = 0; i < nConstInputs; i++, j++) {
  //	BurnDrvGetInputInfo(&bii, i + nConstOffsets);
  //	bool allow_reset = !strcmp(BurnDrvGetTextA(DRV_NAME), "sf2hf") && kNetVersion >= NET_VERSION_RESET_SF2HF;
  //	bool can_tilt = !kNetGame || strcmp(bii.szName, "Tilt");
  //	bool can_reset = !kNetGame || VidOverlayCanReset() || (strcmp(bii.szName, "Reset") && strcmp(bii.szName, "Diagnostic") && strcmp(bii.szName, "Service") && strcmp(bii.szName, "Test")) || allow_reset;
  //	if (*bii.pVal && can_tilt && can_reset) {
  //		nControls[j >> 3] |= (1 << (j & 7));
  //	}
  //}

  //// Convert j to byte count (>> 3 == / 8)
  //j = (j + 7) >> 3;

  //// Analog controls/constants.
  //// These get 1-2 byte each, which makes sense because they need to encode more data.
  //for (i = 0; i < nPlayerInputs[0]; i++) {
  //	BurnDrvGetInputInfo(&bii, i + nPlayerOffset[0]);
  //	if (*bii.pVal && bii.nType != BIT_DIGITAL) {
  //		if (bii.nType & BIT_GROUP_ANALOG) {
  //			nControls[j++] = *bii.pShortVal >> 8;
  //			nControls[j++] = *bii.pShortVal & 0xFF;
  //		}
  //		else {
  //			nControls[j++] = *bii.pVal;
  //		}
  //	}
  //}

  //// DIP switches		-- For some reason, DIP switches each get a full byte, but everything else gets a bit....
  //// Seems like these could also be backed into bits, but maybe we don't because the offset it unclear,
  //// and there are relatively few DIPS for any given game....
 // // Maybe it is because the analog inputs showed up first?
  //for (i = 0; i < nDIPInputs; i++, j++) {
  //	BurnDrvGetInputInfo(&bii, i + nDIPOffset);
  //	nControls[j] = *bii.pVal;
  //}

  //// k is the size in bytes of all inputs for one player
  //int k = j + 1;


  size = PackGameInputs();

  // Send the control block to the Network DLL & retrieve all controls
  int playerInputSize = size / nMaxPlayers;
  if (kNetGame) {
    if (!QuarkSyncInput(nControls, playerInputSize, MAXPLAYER)) {
      return 1;
    }
  }

  UnpackGameInputs(playerInputSize);

  return 0;
}
