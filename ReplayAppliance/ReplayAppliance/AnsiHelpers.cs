using System.Text;


namespace funscrew;

// ==============================================================================================================
public static unsafe class AnsiHelpers
{
  // ------------------------------------------------------------------------------------
  public static int PtrToAnsiStringLength(byte* p, int maxLen)
  {
    int len = 0;
    while (len < maxLen && p[len] != 0)
    {
      len++;
    }
    return len;
  }

  // ------------------------------------------------------------------------------------
  public static string PtrToFixedLengthString(byte* p, int len, int maxLen)
  {
    len = Math.Min(len, maxLen);
    return Encoding.ASCII.GetString((byte*)p, len);
  }

  // ------------------------------------------------------------------------------------
  // Decodes a zero-terminated string.
  public static string PtrToAnsiString(byte* p, int maxLen)
  {
    int len = 0;
    while (len < maxLen && p[len] != 0)
    {
      len++;
    }
    return Encoding.ASCII.GetString((byte*)p, len);
  }

  // ------------------------------------------------------------------------------------
  [Obsolete("we are going to convert everything to utf-8!")]
  public static void WriteAnsiString(string value, byte* dest, int capacity)
  {
    var bytes = Encoding.ASCII.GetBytes(value ?? string.Empty);
    int n = Math.Min(bytes.Length, Math.Max(0, capacity - 1)); // leave room for NULL
    for (int i = 0; i < n; i++)
    {
      dest[i] = (byte)bytes[i];
    }

    dest[n] = 0;
    for (int i = n + 1; i < capacity; i++)
    {
      dest[i] = 0;
    }
  }
}
