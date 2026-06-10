
// =================================================
// Error Codes.  Copy these into your interop application for more understandable handling.
enum EErrorCodes { 
  ERRORCODE_OK = 0,
  ERRORCODE_NOTIMPLEMENTED = 1,
  ERRORCODE_UNHANDLED = 2,
  ERRORCODE_FILENOTFOUND = 3,

  /// <summary>
  /// Indicates that there is no game input.  This should only be used when calling 'GetNextInput' or similar functions.
  /// </summary>
  ERRORCODE_NO_GAMEINPUT = 4
};