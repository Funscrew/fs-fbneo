#ifndef EZQ_H
#define EZQ_H

using namespace std;

// This is a simple queue (FIFO) structure.
template<class T, int N> class EZQ
{

private:
  int _Count = 0;
  int _Capacity = N;
  int _Start = 0;
  int _End = 0;

  T _Items[N];

public:

  // EZQ<T, N>();

  int Count() { return _Count; }

  // ----------------------------------------------------------------------------------------------------------------
  void Push(T item) {
    // TODO: We could use an internal ring buffer with this structure if we wanted to.
    if (_Count == _Capacity) { throw runtime_error("queue is full!"); }
    _Items[_End] = item;
    _End = (_End + 1) % _Capacity;
    _Count++;
  }

  // ----------------------------------------------------------------------------------------------------------------
  // Get a pointer to the first item in the queue.
  void First(T& item)
  {
    item = _Items[_Start];
  }

  // Push the next item into the queue.
  // void Push(T item);

  // ----------------------------------------------------------------------------------------------------------------
  // Pop the last item out.
  void Pop()
  {
    if (_Count == 0) { throw runtime_error("queue is empty!"); }
    _Start = (_Start + 1) % _Capacity;
    _Count--;
  }

  // ----------------------------------------------------------------------------------------------------------------
  bool IsFull() { return _Count == _Capacity; }

  // ----------------------------------------------------------------------------------------------------------------
  bool IsEmpty(){ return _Count == 0; }

};


#endif