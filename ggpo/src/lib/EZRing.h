#ifndef EZRGIN_H
#define EZRING_H

using namespace std;

// This is a simple ring buffer that allows overwrites.
template<class T, int N> class EZRing
{

private:
  int _Count = 0;
  int _Capacity = N;
  int _Start = 0;
  int _End = 0;

  T _Items[N];

public:

  int Count() { return _Count; }

  // ----------------------------------------------------------------------------------------------------------------
  bool IsFull() { return _Count == _Capacity; }

  // ----------------------------------------------------------------------------------------------------------------
  bool IsEmpty() { return _Count == 0; }


  // ----------------------------------------------------------------------------------------------------------------
  void Push(T item) {
    // TODO: We could use an internal ring buffer with this structure if we wanted to.
    // if (_Count == _Capacity) { throw runtime_error("queue overflowed!"); }
    _Items[_End] = item;
    _End = (_End + 1) % _Capacity;

    if (_End == _Start) {
      _Start = (_Start + 1) % _Capacity;
    }

    _Count = (std::max)(_Capacity, _Count + 1);
  }

  // FUTURE: Code that will get the current set:
  // FUTURE: Code that will allow us to pop stuff out, one by one...
    // bool TryPop(T* item) .... no items = false

  //// ----------------------------------------------------------------------------------------------------------------
  //// Get a pointer to the last item in the queue.
  //void First(T& item)
  //{
  //  item = _Items[_End];
  //}

  //// Push the next item into the queue.
  //// void Push(T item);

  //// ----------------------------------------------------------------------------------------------------------------
  //// Pop the last item out.
  //void Pop()
  //{
  //  _Start = (_Start + 1) % _Capacity;
  //  _Count--;
  //}


};


#endif