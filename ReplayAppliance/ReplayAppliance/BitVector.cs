namespace funscrew;

// ====================================================================================================================
public static class BitVector
{
  public const int BITVECTOR_NIBBLE_SIZE = 8;

  // ------------------------------------------------------------------------------------------------------------------
  public unsafe static void SetBit(byte* vector, ref int offset)
  {
    int byteIndex = offset >> 3;     // offset / 8
    int bitIndex = offset & 7;      // offset % 8

    vector[byteIndex] = (byte)(vector[byteIndex] | (1 << bitIndex));
    offset += 1;
  }

  // ------------------------------------------------------------------------------------------------------------------
  public unsafe static void ClearBit(byte* vector, ref int offset)
  {
    int byteIndex = offset >> 3;
    int bitIndex = offset & 7;

    vector[byteIndex] = (byte)(vector[byteIndex] & ~(1 << bitIndex));
    offset += 1;
  }

  // ------------------------------------------------------------------------------------------------------------------
  public unsafe static void WriteNibblet(byte* vector, int nibble, ref int offset)
  {
    // Replace with your ASSERT(...) if you have a custom macro.
    Utils.ASSERT(nibble < (1 << BITVECTOR_NIBBLE_SIZE));

    for (int i = 0; i < BITVECTOR_NIBBLE_SIZE; i++)
    {
      if ((nibble & (1 << i)) != 0)
      {
        SetBit(vector, ref offset);
      }
      else
      {
        ClearBit(vector, ref offset);
      }
    }
  }

  // ------------------------------------------------------------------------------------------------------------------
  public unsafe static int ReadBit(byte* vector, ref int offset)
  {
    int byteIndex = offset >> 3;
    int bitIndex = offset & 7;

    int res = ((vector[byteIndex] & (1 << bitIndex)) != 0) ? 1 : 0;
    offset += 1;
    return res;
  }

  // ------------------------------------------------------------------------------------------------------------------
  public unsafe static int ReadNibblet(byte* vector, ref int offset)
  {
    int nibblet = 0;

    for (int i = 0; i < BITVECTOR_NIBBLE_SIZE; i++)
    {
      nibblet |= (ReadBit(vector, ref offset) << i);
    }

    return nibblet;
  }

}
