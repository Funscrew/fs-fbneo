/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "types.h"
#include "bitvector.h"

void
BitVector_SetBit(uint8 *vector, int *offset)
{
   vector[(*offset) / 8] |= (1 << ((*offset) % 8));
   *offset += 1;
}

void
BitVector_ClearBit(uint8 *vector, int *offset)
{
   vector[(*offset) / 8] &= ~(1 << ((*offset) % 8));
   *offset += 1;
}

void
BitVector_WriteNibblet(uint8 *vector, int nibble, int *offset)
{
   ASSERT(nibble < (1 << BITVECTOR_NIBBLE_SIZE));
   for (int i = 0; i < BITVECTOR_NIBBLE_SIZE; i++) {
      if (nibble & (1 << i)) {
         BitVector_SetBit(vector, offset);
      } else {
         BitVector_ClearBit(vector, offset);
      }
   }
}

int
BitVector_ReadBit(uint8 *vector, int *offset)
{
   int res = !!(vector[(*offset) / 8] & (1 << ((*offset) % 8)));
   *offset += 1;
   return res;

   // Better / more readable version?
   //int byteIndex = *offset >> 3;
   //int bitIndex = *offset & 7;
   //int res  = ((vector[byteIndex] & (1 << bitIndex)) != 0) ? 1 : 0;
   //offset += 1;
   //return res;
}

int
BitVector_ReadNibblet(uint8 *vector, int *offset)
{
   int nibblet = 0;
   for (int i = 0; i < BITVECTOR_NIBBLE_SIZE; i++) {
      nibblet |= (BitVector_ReadBit(vector, offset) << i);
   }
   return nibblet;
}

