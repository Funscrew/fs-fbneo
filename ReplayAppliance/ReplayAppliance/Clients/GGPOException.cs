using System;
using System.Collections.Generic;
using System.Diagnostics.Contracts;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace funscrew
{

  public class GGPOException : Exception
  {
    public GGPOException() { }
    public GGPOException(string message) : base(message) { }
    public GGPOException(string? message, Exception? innerException) : base(message, innerException) { }
  }
}
