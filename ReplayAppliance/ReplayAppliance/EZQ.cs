using System.Numerics;

namespace drewCo.Tools
{
  // ========================================================================================================
  /// <summary>
  /// A simple little queue (FIFO).
  /// </summary>
  // TODO: Put this in the tools lib!
  [Obsolete("Use version from drewco.tools > 1.5.1")]
  public class EZQ<T>
  {
    private int Start = 0;
    private int End = 0;
    private T[] _Items = null!;

    public bool IsEmpty { get { return Count == 0; } }
    public bool IsFull { get { return Count == Capacity; } }

    public int Capacity { get; private set; } = 0;
    public int Count { get; private set; } = 0;

    // ------------------------------------------------------------------------------------------------------
    public EZQ(int capacity_)
    {
      Capacity = capacity_;
      _Items = new T[Capacity];
    }

    // ------------------------------------------------------------------------------------------------------
    public void Push(T item)
    {
      if (Count == Capacity) { throw new QueueOverflowException(); }
      _Items[End] = item;
      End = (End + 1) % Capacity;
      Count++;
    }

    // ------------------------------------------------------------------------------------------------------
    /// <summary>
    /// Remove the first item in the queue.
    /// </summary>
    public void Pop()
    {
      Start = (Start + 1) % Capacity;
      Count--;
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
