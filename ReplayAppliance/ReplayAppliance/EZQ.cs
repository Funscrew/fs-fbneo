using System.Numerics;

namespace drewCo.Tools
{
  // ========================================================================================================
  /// <summary>
  /// A simple little queue that uses a ring buffer internally.
  /// </summary>
  // TODO: Put this in the tools lib!
  [Obsolete("Use version from drewco.tools > 1.4.2.4")]
  public class EZQ<T>
  {
    private int Capacity = 0;
    private int _Count = 0;
    private int Start = 0;
    private int End = 0;

    private T[] _Items = null!;

    public bool IsEmpty { get { return _Count == 0; } }
    public bool IsFull { get { return _Count == Capacity; } }

    public int Count { get { return _Count; } }

    // ------------------------------------------------------------------------------------------------------
    public EZQ(int capacity_)
    {
      Capacity = capacity_;
      _Items = new T[Capacity];
    }

    // ------------------------------------------------------------------------------------------------------
    public void Push(T item)
    {
      if (_Count == Capacity) { throw new QueueOverflowException(); }
      _Items[End] = item;
      End = (End + 1) % Capacity;
      _Count++;
    }

    // ------------------------------------------------------------------------------------------------------
    /// <summary>
    /// Remove the first item in the queue.
    /// </summary>
    public void Pop()
    {
      Start = (Start + 1) % Capacity;
      _Count--;
    }


    // ------------------------------------------------------------------------------------------------------
    public void First(ref T item)
    {
      item = _Items[Start];
    }

    // ------------------------------------------------------------------------------------------------------
    public void Last(ref T item)
    {
      item = _Items[End];
    }
  }

  // ========================================================================================================
  [Serializable]
  public class QueueOverflowException : Exception
  {
    public QueueOverflowException() { }
    public QueueOverflowException(string message) : base(message) { }
    public QueueOverflowException(string message, Exception inner) : base(message, inner) { }
    protected QueueOverflowException(
    System.Runtime.Serialization.SerializationInfo info,
    System.Runtime.Serialization.StreamingContext context) : base(info, context) { }
  }
}
